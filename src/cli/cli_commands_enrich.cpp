#include "cli_commands.h"
#include "cli_helpers.h"
#include <QMap>
#include <QtConcurrent>
#include "../metadata/provider_orchestrator.h"
#include "../metadata/hasheous_provider.h"
#include "../core/constants/constants.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

int handleEnrichCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("enrich") && !ctx.processRequested) return 0;
    if (ctx.processRequested && ctx.processHandled) return 0;

    qInfo() << "";
    qInfo() << "=== Metadata Enrichment ===";
    qInfo() << "";

    QMap<int, Database::MatchResult> matches = ctx.db.getAllMatches();
    QList<FileRecord> files = ctx.db.getExistingFiles();

    // Build fileId→FileRecord lookup
    QMap<int, FileRecord> fileMap;
    for (const FileRecord &f : files)
        fileMap[f.id] = f;

    // Find games with sparse metadata
    struct EnrichCandidate {
        int gameId;
        int fileId;
        QString title;
        QString system;
        QString crc32;
        QString md5;
        QString sha1;
        QString publisher;
        QString developer;
        int releaseYear = 0;
        QString description;
        QString genres;
        QString players;
    };
    QList<EnrichCandidate> candidates;
    QSet<int> seenGames;

    for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
        const Database::MatchResult &m = it.value();
        if (seenGames.contains(m.gameId)) continue;
        seenGames.insert(m.gameId);
        if (!fileMap.contains(m.fileId)) continue;
        if (!fileMatchesProcessScope(fileMap.value(m.fileId), ctx.processFileScopeIds)) {
            continue;
        }
        if (!fileMatchesSystemFilter(fileMap.value(m.fileId), ctx.processSystemIdFilter, &m)) {
            continue;
        }

        const bool sparse = m.publisher.isEmpty() || m.developer.isEmpty()
                          || m.genre.isEmpty() || m.players.isEmpty()
                          || m.description.isEmpty() || m.releaseYear == 0;
        if (!sparse) continue;

        const QString system = fileMap.contains(m.fileId)
            ? ctx.db.getSystemDisplayName(fileMap[m.fileId].systemId)
            : QString();

        QString crc32, md5, sha1;
        if (fileMap.contains(m.fileId)) {
            const FileRecord &f = fileMap[m.fileId];
            crc32 = f.crc32;
            md5 = f.md5;
            sha1 = f.sha1;
        }
        candidates.append({m.gameId, m.fileId, m.gameTitle, system, crc32, md5, sha1,
                            m.publisher, m.developer, m.releaseYear, m.description, m.genre, m.players});
    }

    if (candidates.isEmpty()) {
        qInfo() << "All matched games already have metadata — nothing to enrich.";
        return 0;
    }

    auto orchestrator = buildOrchestrator(ctx.parser, &ctx.db);

    qInfo() << "Found" << candidates.size() << "game(s) with sparse metadata";
    qInfo() << "";

    int enriched = 0, failed = 0;

    // Phase 1 — Parallel metadata-stub and field-gap preparation.
    // computeFieldGap() and GameMetadata construction are pure in-memory
    // operations with no shared mutable state; safe for concurrent execution.
    struct PreparedEnrichTask {
        EnrichCandidate candidate;
        GameMetadata    existing;
        ProviderOrchestrator::FieldSet gap;
        bool alreadyComplete = false;
    };

    const QList<PreparedEnrichTask> tasks = QtConcurrent::blockingMapped(
        candidates,
        [](const EnrichCandidate &c) -> PreparedEnrichTask {
            PreparedEnrichTask t;
            t.candidate        = c;
            t.existing.title       = c.title;
            t.existing.system      = c.system;
            t.existing.publisher   = c.publisher;
            t.existing.developer   = c.developer;
            t.existing.releaseDate = c.releaseYear > 0
                ? QString::number(c.releaseYear) : QString();
            t.existing.description = c.description;
            t.existing.genres      = c.genres.isEmpty()
                ? QStringList() : c.genres.split(QStringLiteral(", "));
            t.existing.players     = c.players.toInt();
            t.gap = ProviderOrchestrator::computeFieldGap(t.existing);
            t.alreadyComplete = t.gap.isEmpty();
            return t;
        });

    // Phase 2 — Serial provider enrichment and DB writes.
    // QNetworkAccessManager inside HttpMetadataProvider has thread affinity to
    // its owning (main) thread; enrichMissingFields() and db.updateGame() must
    // not be dispatched to worker threads.
    for (const PreparedEnrichTask &task : tasks) {
        const auto &c = task.candidate;
        qInfo() << "Enriching:" << c.title << "(" << c.system << ")";

        if (task.alreadyComplete) {
            qInfo() << "  Already complete — skipping";
            continue;
        }

        const QString bestHash = !c.sha1.isEmpty() ? c.sha1 :
                                 !c.md5.isEmpty()  ? c.md5  :
                                 c.crc32;

        GameMetadata metadata = orchestrator->enrichMissingFields(
            task.gap, task.existing, bestHash, c.title, c.system,
            c.crc32, c.md5, c.sha1);

        if (metadata.title.isEmpty()) {
            qInfo() << "  ✗ No metadata found";
            failed++;
            continue;
        }

        const QString genres = metadata.genres.join(", ");
        const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();

        bool updated = ctx.db.updateGame(c.gameId,
                                         metadata.publisher,
                                         metadata.developer,
                                         metadata.releaseDate,
                                         metadata.description,
                                         genres,
                                         players,
                                         metadata.rating);
        const ProviderOrchestrator::FieldSet remaining =
            ProviderOrchestrator::computeFieldGap(metadata);
        if (updated) {
            if (remaining.isEmpty()) {
                qInfo() << "  \u2713 Fully enriched from" << metadata.providerId;
            } else {
                qInfo() << "  \u2713 Partially enriched from" << metadata.providerId
                        << "- still missing:" << QStringList(remaining.constBegin(), remaining.constEnd()).join(", ");
            }
            enriched++;
        } else {
            qInfo() << "  \u2717 No new metadata from any provider";
            failed++;
        }
    }

    qInfo() << "";
    qInfo() << "=== Enrichment Complete ===";
    qInfo() << "Enriched:" << enriched;
    qInfo() << "Failed:"   << failed;

    auto *hasheous = dynamic_cast<HasheousProvider*>(
        orchestrator->getProvider(Constants::Providers::HASHEOUS));
    if (hasheous && hasheous->igdbSkippedCount() > 0) {
        qInfo() << "";
        qInfo().noquote() << QString("%1 title(s) had IGDB data available — "
                                     "set hasheous_client_api_key for richer metadata.")
                                 .arg(hasheous->igdbSkippedCount());
    }
    return 0;
}
