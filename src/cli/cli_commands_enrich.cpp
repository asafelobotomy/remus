#include "cli_commands.h"
#include "cli_helpers.h"
#include <QMap>
#include "../metadata/provider_orchestrator.h"
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

    for (const auto &c : candidates) {
        qInfo() << "Enriching:" << c.title << "(" << c.system << ")";

        // Build an existing metadata stub from what we already know so
        // enrichMissingFields can compute the field gap and skip providers
        // that cannot supply anything new.
        GameMetadata existing;
        existing.title       = c.title;
        existing.system      = c.system;
        existing.publisher   = c.publisher;
        existing.developer   = c.developer;
        existing.releaseDate = c.releaseYear > 0 ? QString::number(c.releaseYear) : QString();
        existing.description = c.description;
        existing.genres      = c.genres.isEmpty() ? QStringList() : c.genres.split(", ");
        existing.players     = c.players.toInt();

        const QString bestHash = !c.sha1.isEmpty() ? c.sha1 :
                                 !c.md5.isEmpty()  ? c.md5  :
                                 c.crc32;

        const ProviderOrchestrator::FieldSet gap =
            ProviderOrchestrator::computeFieldGap(existing);

        if (gap.isEmpty()) {
            qInfo() << "  Already complete — skipping";
            continue;
        }

        GameMetadata metadata = orchestrator->enrichMissingFields(
            gap, existing, bestHash, c.title, c.system, c.crc32, c.md5, c.sha1);

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
    return 0;
}
