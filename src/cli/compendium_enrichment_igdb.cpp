#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/http_metadata_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include "../services/credential_manager.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

namespace {

    // Metadata gaps that justify a full IGDB platform catalog download (rating-only
    // gaps are excluded — those are cheaper to fill via Hasheous or left empty).
    static const char kIgdbBulkGameGapSql[] =
        "description IS NULL OR TRIM(description) = '' "
        "   OR genre IS NULL OR TRIM(genre) = '' "
        "   OR developer IS NULL OR TRIM(developer) = '' "
        "   OR publisher IS NULL OR TRIM(publisher) = '' "
        "   OR release_year IS NULL "
        "   OR release_date IS NULL OR TRIM(release_date) = '' "
        "   OR players_max IS NULL ";

    // Returns the candidate with the greatest number of non-empty enrichable fields.
    // Used to resolve title-collision ties in the IGDB index (multiple entries with
    // the same normalized title, e.g. different regional variants of the same game).
    const GameMetadata &bestCandidate(const QList<GameMetadata> &candidates) {
        Q_ASSERT(!candidates.isEmpty());
        int bestScore = -1;
        int bestIdx = 0;
        for (int i = 0; i < candidates.size(); ++i) {
            const GameMetadata &c = candidates.at(i);
            const int score = (!c.description.isEmpty() ? 1 : 0) + (!c.developer.isEmpty() ? 1 : 0)
                + (!c.publisher.isEmpty() ? 1 : 0) + (!c.genres.isEmpty() ? 1 : 0) + (c.releaseDate.size() >= 4 ? 1 : 0)
                + (c.rating > 0.0f ? 1 : 0) + (c.players > 0 ? 1 : 0);
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        return candidates.at(bestIdx);
    }

} // anonymous namespace

bool enrichFromIGDB(
    QSqlDatabase &database, const QString &credentialsPath, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    // Load credentials — when a path is supplied, read that file only so callers
    // can enforce "missing file means skip" without ambient env vars.
    const auto loadCredential = [&](const char *key) {
        const QString qkey = QString::fromLatin1(key);
        return credentialsPath.isEmpty() ? CredentialManager::get(qkey)
                                         : CredentialManager::getFromFile(qkey, credentialsPath);
    };
    const QString clientId = loadCredential("igdb/client_id");
    const QString clientSecret = loadCredential("igdb/client_secret");
    if (clientId.isEmpty() || clientSecret.isEmpty()) {
        qInfo() << "[IGDB] Credentials not configured — enrichment skipped";
        return true;
    }
    qInfo() << "[IGDB] Credentials loaded — starting bulk platform enrichment";

    // Systems that still have games missing enrichable fields (excluding rating-only).
    QSqlQuery sysQ(database);
    if (!sysQ.exec(QStringLiteral("SELECT DISTINCT g.system_id, s.display_name FROM games g "
                                  "JOIN systems s ON s.system_id = g.system_id "
                                  "WHERE %1 "
                                  "ORDER BY s.display_name")
                       .arg(QLatin1String(kIgdbBulkGameGapSql)))) {
        error = QStringLiteral("Query systems: %1").arg(sysQ.lastError().text());
        return false;
    }
    struct SysInfo {
        int id;
        QString name;
    };
    QList<SysInfo> systems;
    while (sysQ.next())
        systems.append({ sysQ.value(0).toInt(), sysQ.value(1).toString() });
    // Release the cursor/prepared-statement before starting per-system write
    // transactions — an open cursor can cause SQLITE_LOCKED in some SQLite
    // journal modes when the nested QEventLoop (waitForReply) re-enters.
    sysQ.finish();

    if (systems.isEmpty())
        return true;

    // Single provider (and therefore single QNAM) shared across all systems.
    // A per-system QNAM caused crashes during QNAM destruction because Qt's SSL
    // teardown is asynchronous; tearing it down at the end of every loop iteration
    // races with background socket cleanup. Instead, we flush all pending
    // deleteLater() calls at the START of each iteration (while the QNAM is still
    // alive) so reply cleanup happens before new requests are issued.
    IGDBProvider provider;
    provider.setCredentials(clientId, clientSecret);

    const QString snapshotId = QStringLiteral("igdb-bulk");
    static const int PAGE_SIZE = 500;
    int systemsSkippedNoSlug = 0;
    int systemsSkippedEmptyIndex = 0;

    for (const SysInfo &sys : systems) {
        // Flush any deleteLater() events posted by the previous system's network
        // replies before issuing new requests on the shared QNAM.
        HttpMetadataProvider::processNetworkEvents();

        const QString igdbSlug = SystemResolver::providerName(sys.id, Constants::Providers::IGDB);
        if (igdbSlug.isEmpty()) {
            ++systemsSkippedNoSlug;
            continue;
        }

        // Skip full platform download when this system only has rating gaps.
        {
            QSqlQuery bulkGapQ(database);
            bulkGapQ.prepare(QStringLiteral("SELECT 1 FROM games WHERE system_id = ? AND (%1) LIMIT 1")
                                 .arg(QLatin1String(kIgdbBulkGameGapSql)));
            bulkGapQ.addBindValue(sys.id);
            if (!bulkGapQ.exec()) {
                error = QStringLiteral("IGDB bulk gap check for %1: %2").arg(sys.name, bulkGapQ.lastError().text());
                return false;
            }
            if (!bulkGapQ.next())
                continue;
        }

        // Bulk-fetch all IGDB games for this platform
        QHash<QString, QList<GameMetadata>> igdbIndex;
        int offset = 0;
        while (true) {
            const QList<GameMetadata> page = provider.fetchGamesByPlatformSlug(igdbSlug, offset, PAGE_SIZE);
            for (const GameMetadata &gm : page) {
                if (!gm.title.isEmpty())
                    igdbIndex[normalizeMetadataTitle(gm.title)].append(gm);
            }
            if (page.size() < PAGE_SIZE)
                break;
            offset += PAGE_SIZE;
        }

        if (igdbIndex.isEmpty()) {
            ++systemsSkippedEmptyIndex;
            qWarning().noquote() << QStringLiteral(
                "[IGDB] %1 (slug=%2): API returned no entries — check slug or network")
                                        .arg(sys.name, igdbSlug);
            continue;
        }
        qInfo().noquote()
            << QStringLiteral("[IGDB] %1 (%2): %3 entries indexed").arg(sys.name, igdbSlug).arg(igdbIndex.size());

        // Per-system transaction — keeps lock duration short relative to network time
        if (!database.transaction()) {
            error = QStringLiteral("Failed to start transaction for system %1: %2")
                        .arg(sys.name, database.lastError().text());
            return false;
        }

        if (!upsertEnrichmentSource(database,
                SourceSpec {
                    QStringLiteral("igdb"),
                    QStringLiteral("IGDB"),
                    QStringLiteral("online-api"),
                    QStringLiteral("https://www.igdb.com"),
                    /*attributionRequired=*/true,
                    /*priority=*/70,
                    QString(),
                },
                SnapshotSpec {
                    snapshotId,
                    QStringLiteral("IGDB bulk enrichment"),
                },
                error)) {
            database.rollback();
            return false;
        }

        QSqlQuery updateQ(database);
        updateQ.prepare(QStringLiteral("UPDATE games SET "
                                       "description  = COALESCE(NULLIF(description, ''), ?), "
                                       "genre        = COALESCE(NULLIF(genre, ''), ?), "
                                       "developer    = COALESCE(NULLIF(developer, ''), ?), "
                                       "publisher    = COALESCE(NULLIF(publisher, ''), ?), "
                                       "release_year = COALESCE(release_year, ?), "
                                       "release_date = COALESCE(release_date, ?), "
                                       "rating       = COALESCE(rating, ?), "
                                       "players_max  = COALESCE(players_max, ?) "
                                       "WHERE game_id = ?"));

        QSqlQuery factQ(database);
        factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                     "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                     "source_priority, confidence) "
                                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        QSqlQuery delQ(database);
        delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec factSpec {
            QStringLiteral("igdb"),
            snapshotId,
            70,
            0.80,
        };

        auto insertFact = [&](const QString &gameId, const QString &field, const QString &value,
                              const QString &type = QStringLiteral("text")) -> bool {
            bool inserted = false;
            if (!insertGameFact(
                    delQ, factQ, factSpec, gameId, field, value, type, error, QStringLiteral("igdb"), &inserted))
                return false;
            if (inserted)
                ++factsInserted;
            return true;
        };

        QSqlQuery gamesQ(database);
        gamesQ.prepare(QStringLiteral("SELECT game_id, canonical_title FROM games "
                                      "WHERE system_id = ? "
                                      "  AND (%1)")
                           .arg(QLatin1String(kIgdbBulkGameGapSql)));
        gamesQ.addBindValue(sys.id);
        if (!gamesQ.exec()) {
            database.rollback();
            error = QStringLiteral("Query games for system %1: %2").arg(sys.name, gamesQ.lastError().text());
            return false;
        }

        int sysEnriched = 0;
        while (gamesQ.next()) {
            const QString gameId = gamesQ.value(0).toString();
            const QString norm = normalizeMetadataTitle(gamesQ.value(1).toString());
            const auto it = igdbIndex.constFind(norm);
            if (it == igdbIndex.cend())
                continue;
            const GameMetadata &gm = bestCandidate(it.value());

            int releaseYear = 0;
            if (gm.releaseDate.size() >= 4) {
                bool ok = false;
                const int y = gm.releaseDate.left(4).toInt(&ok);
                if (ok && y > 1970 && y < 2030)
                    releaseYear = y;
            }
            const QString releaseDateStr = (gm.releaseDate.size() >= 10) ? gm.releaseDate.left(10) : QString();

            updateQ.bindValue(0, nullableText(gm.description));
            const QString genreStr
                = gm.genres.isEmpty() ? QString() : gm.genres.join(QStringLiteral(", "));
            updateQ.bindValue(1, nullableText(genreStr));
            updateQ.bindValue(2, nullableText(gm.developer));
            updateQ.bindValue(3, nullableText(gm.publisher));
            updateQ.bindValue(4, nullableInt(releaseYear));
            updateQ.bindValue(5, nullableText(releaseDateStr));
            updateQ.bindValue(6, nullableDouble(static_cast<double>(gm.rating)));
            updateQ.bindValue(7, nullableInt(gm.players));
            updateQ.bindValue(8, gameId);
            if (!execPrepared(updateQ, error, QStringLiteral("Update game igdb"))) {
                database.rollback();
                return false;
            }
            if (updateQ.numRowsAffected() > 0) {
                ++gamesEnriched;
                ++sysEnriched;
            }

            const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
            const QString ratingStr
                = gm.rating > 0.0f ? QString::number(static_cast<double>(gm.rating), 'f', 2) : QString();
            const QString playersStr = gm.players > 0 ? QString::number(gm.players) : QString();
            const QString igdbId = gm.externalIds.value(Constants::Providers::ExternalId::IGDB, gm.id);
            if (!insertFact(gameId, QStringLiteral("igdb_id"), igdbId)
                || !insertFact(gameId, QStringLiteral("description"), gm.description)
                || !insertFact(gameId, QStringLiteral("genre"), genreStr)
                || !insertFact(gameId, QStringLiteral("developer"), gm.developer)
                || !insertFact(gameId, QStringLiteral("publisher"), gm.publisher)
                || !insertFact(gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
                || !insertFact(gameId, QStringLiteral("release_date"), releaseDateStr)
                || !insertFact(gameId, QStringLiteral("rating"), ratingStr, QStringLiteral("decimal"))
                || !insertFact(gameId, QStringLiteral("players_max"), playersStr, QStringLiteral("integer"))) {
                database.rollback();
                return false;
            }
        }

        if (!database.commit()) {
            error = QStringLiteral("Failed to commit enrichment for %1: %2").arg(sys.name, database.lastError().text());
            return false;
        }

        qInfo().noquote() << QStringLiteral("[IGDB] %1: +%2 games enriched").arg(sys.name).arg(sysEnriched);
    }

    // Flush any pending deleteLater() events from the last system's replies before
    // the single shared IGDBProvider (and its QNAM) goes out of scope at function return.
    HttpMetadataProvider::processNetworkEvents();

    qInfo().noquote() << QStringLiteral("[IGDB] Complete: %1 games enriched, %2 facts inserted, "
                                        "%3 systems skipped (no slug), %4 systems skipped (empty API index)")
                             .arg(gamesEnriched)
                             .arg(factsInserted)
                             .arg(systemsSkippedNoSlug)
                             .arg(systemsSkippedEmptyIndex);

    return true;
}

} // namespace CompendiumEnrichment
