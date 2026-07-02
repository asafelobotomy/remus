#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "compendium_enrichment_match_utils.h"
#include "compendium_platform_index_cache.h"
#include "compendium_progress.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/http_metadata_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include "../services/credential_manager.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

namespace {

    QString alternateTitlesJson(const QStringList &titles) {
        if (titles.isEmpty())
            return QString();
        QJsonArray arr;
        for (const QString &title : titles)
            arr.append(title);
        return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }

    static QString igdbBulkGameGapSql(const QString &colPrefix = QString()) {
        return QStringLiteral("%1description IS NULL OR TRIM(%1description) = '' "
                              "   OR %1genre IS NULL OR TRIM(%1genre) = '' "
                              "   OR %1developer IS NULL OR TRIM(%1developer) = '' "
                              "   OR %1publisher IS NULL OR TRIM(%1publisher) = '' "
                              "   OR %1release_year IS NULL "
                              "   OR %1release_date IS NULL OR TRIM(%1release_date) = '' "
                              "   OR %1players_max IS NULL ")
            .arg(colPrefix);
    }

    bool applyIgdbGameMetadata(QSqlDatabase &database, FactReplaceQueries &replaceQueries, QSqlQuery &updateQ,
        QSqlQuery &factQ, QSqlQuery &delQ, const FactInsertSpec &factSpec, const QString &gameId,
        const GameMetadata &gm, int &gamesEnriched, int &factsInserted, QString &error) {
        int releaseYear = 0;
        if (gm.releaseDate.size() >= 4) {
            bool ok = false;
            const int y = gm.releaseDate.left(4).toInt(&ok);
            if (ok && y > 1970 && y < 2030)
                releaseYear = y;
        }
        const QString releaseDateStr = (gm.releaseDate.size() >= 10) ? gm.releaseDate.left(10) : QString();
        const QString genreStr = gm.genres.isEmpty() ? QString() : gm.genres.join(QStringLiteral(", "));

        updateQ.bindValue(0, nullableText(gm.description));
        updateQ.bindValue(1, nullableText(genreStr));
        updateQ.bindValue(2, nullableText(gm.developer));
        updateQ.bindValue(3, nullableText(gm.publisher));
        updateQ.bindValue(4, nullableInt(releaseYear));
        updateQ.bindValue(5, nullableText(releaseDateStr));
        updateQ.bindValue(6, nullableDouble(static_cast<double>(gm.rating)));
        updateQ.bindValue(7, nullableInt(gm.players));
        updateQ.bindValue(8, nullableText(gm.boxArtUrl));
        updateQ.bindValue(9, nullableText(gm.series));
        updateQ.bindValue(10, nullableText(gm.ageRating));
        updateQ.bindValue(11, nullableText(alternateTitlesJson(gm.alternateTitles)));
        updateQ.bindValue(12, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update game igdb")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString ratingStr
            = gm.rating > 0.0f ? QString::number(static_cast<double>(gm.rating), 'f', 2) : QString();
        const QString playersStr = gm.players > 0 ? QString::number(gm.players) : QString();
        const QString igdbId = gm.externalIds.value(Constants::Providers::ExternalId::IGDB, gm.id);

        auto insertFact
            = [&](const QString &field, const QString &value, const QString &type = QStringLiteral("text")) {
                  bool inserted = false;
                  if (!insertGameFact(replaceQueries, delQ, factQ, factSpec, gameId, field, value, type, error,
                          QStringLiteral("igdb"), &inserted))
                      return false;
                  if (inserted)
                      ++factsInserted;
                  return true;
              };

        const bool factsOk = insertFact(QStringLiteral("igdb_id"), igdbId)
            && insertFact(QStringLiteral("description"), gm.description)
            && insertFact(QStringLiteral("genre"), genreStr) && insertFact(QStringLiteral("developer"), gm.developer)
            && insertFact(QStringLiteral("publisher"), gm.publisher)
            && insertFact(QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            && insertFact(QStringLiteral("release_date"), releaseDateStr)
            && insertFact(QStringLiteral("rating"), ratingStr, QStringLiteral("decimal"))
            && insertFact(QStringLiteral("players_max"), playersStr, QStringLiteral("integer"))
            && insertFact(QStringLiteral("cover_url"), gm.boxArtUrl) && insertFact(QStringLiteral("series"), gm.series)
            && insertFact(QStringLiteral("age_rating"), gm.ageRating)
            && insertFact(QStringLiteral("alternate_titles"), alternateTitlesJson(gm.alternateTitles));
        return factsOk;
    }

} // anonymous namespace

bool enrichFromIGDB(
    QSqlDatabase &database, const QString &credentialsPath, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    // Load credentials (JSON file beside DB, then REMUS_* env, keychain, QSettings).
    const auto loadCredential
        = [&](const char *key) { return CredentialManager::get(QString::fromLatin1(key), credentialsPath); };
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
                .arg(igdbBulkGameGapSql(QStringLiteral("g."))))) {
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
    const QString byIdSnapshotId = QStringLiteral("igdb-by-id");
    const QString sourceId = QStringLiteral("igdb");
    static const int PAGE_SIZE = 500;
    int systemsSkippedNoSlug = 0;
    int systemsSkippedEmptyIndex = 0;
    int byIdGamesEnriched = 0;
    bool bulkCleared = false;

    const QSet<QString> skipGameIds = loadGamesWithMinSourceFieldFacts(database, sourceId, 4, error);
    if (!error.isEmpty())
        return false;
    if (!skipGameIds.isEmpty()) {
        qInfo().noquote()
            << QStringLiteral("[IGDB] Skipping %1 games already enriched by source").arg(skipGameIds.size());
    }

    // Phase 1: targeted fetch for games that already have igdb_id from hash bridges.
    {
        QSqlQuery pendingQ(database);
        if (!pendingQ.exec(QStringLiteral("SELECT g.game_id, g.igdb_id, "
                                          "(SELECT gf.field_value FROM game_facts gf "
                                          " WHERE gf.game_id = g.game_id AND gf.field_name = 'igdb_id' "
                                          " ORDER BY gf.source_priority DESC, gf.confidence DESC, gf.fact_id DESC "
                                          " LIMIT 1) AS fact_igdb_id "
                                          "FROM games g "
                                          "WHERE (%1) "
                                          "  AND ((g.igdb_id IS NOT NULL AND TRIM(g.igdb_id) <> '') "
                                          "    OR EXISTS (SELECT 1 FROM game_facts gf "
                                          "               WHERE gf.game_id = g.game_id "
                                          "                 AND gf.field_name = 'igdb_id')) "
                                          "ORDER BY g.game_id")
                    .arg(igdbBulkGameGapSql(QStringLiteral("g."))))) {
            error = QStringLiteral("Query IGDB per-id candidates: %1").arg(pendingQ.lastError().text());
            return false;
        }

        struct PendingIgdbGame {
            QString gameId;
            QString igdbId;
        };
        QList<PendingIgdbGame> pendingGames;
        while (pendingQ.next()) {
            QString igdbId = pendingQ.value(1).toString().trimmed();
            if (igdbId.isEmpty())
                igdbId = pendingQ.value(2).toString().trimmed();
            if (igdbId.isEmpty())
                continue;
            pendingGames.append({ pendingQ.value(0).toString(), igdbId });
        }
        pendingQ.finish();

        if (!pendingGames.isEmpty()) {
            qInfo().noquote()
                << QStringLiteral("[IGDB] Per-id fetch: %1 games with known igdb_id").arg(pendingGames.size());

            if (!bulkCleared) {
                if (!bulkClearSourceFactBlockers(database, sourceId, error))
                    return false;
                bulkCleared = true;
            }

            if (!upsertEnrichmentSource(database,
                    SourceSpec {
                        sourceId,
                        QStringLiteral("IGDB"),
                        QStringLiteral("online-api"),
                        QStringLiteral("https://www.igdb.com"),
                        /*attributionRequired=*/true,
                        /*priority=*/70,
                        QString(),
                    },
                    SnapshotSpec {
                        byIdSnapshotId,
                        QStringLiteral("IGDB per-id enrichment"),
                    },
                    error)) {
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
                                           "players_max  = COALESCE(players_max, ?), "
                                           "cover_url    = COALESCE(NULLIF(cover_url, ''), ?), "
                                           "series       = COALESCE(NULLIF(series, ''), ?), "
                                           "age_rating   = COALESCE(NULLIF(age_rating, ''), ?), "
                                           "alternate_titles = COALESCE(NULLIF(alternate_titles, ''), ?) "
                                           "WHERE game_id = ?"));

            QSqlQuery factQ(database);
            factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                         "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                         "source_priority, confidence) "
                                         "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

            QSqlQuery delQ(database);
            delQ.prepare(
                QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

            const FactInsertSpec byIdFactSpec {
                sourceId,
                byIdSnapshotId,
                70,
                0.85,
            };
            FactReplaceQueries replaceQueries(database);
            EnrichmentBatchWriter batchWriter(database);

            int callIdx = 0;
            for (const PendingIgdbGame &pending : pendingGames) {
                ++callIdx;
                if (skipGameIds.contains(pending.gameId)) {
                    if (!batchWriter.onGameProcessed(error))
                        return false;
                    continue;
                }
                if (callIdx % 20 == 0)
                    HttpMetadataProvider::processNetworkEvents();
                if (callIdx % 2500 == 0 || callIdx == pendingGames.size()) {
                    reportCompendiumEnrichmentProgress(QStringLiteral("per_id_fetch"), callIdx, pendingGames.size(),
                        QStringLiteral("%1 enriched").arg(byIdGamesEnriched), byIdGamesEnriched, factsInserted);
                }

                const GameMetadata gm = provider.getById(pending.igdbId);
                if (gm.title.isEmpty()) {
                    if (!batchWriter.onGameProcessed(error))
                        return false;
                    continue;
                }

                if (!applyIgdbGameMetadata(database, replaceQueries, updateQ, factQ, delQ, byIdFactSpec, pending.gameId,
                        gm, byIdGamesEnriched, factsInserted, error)) {
                    return false;
                }
                if (!batchWriter.onGameProcessed(error))
                    return false;
            }

            if (!batchWriter.finish(error))
                return false;

            gamesEnriched += byIdGamesEnriched;
            qInfo().noquote()
                << QStringLiteral("[IGDB] Per-id fetch complete: +%1 games enriched").arg(byIdGamesEnriched);
        }
    }

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
            bulkGapQ.prepare(
                QStringLiteral("SELECT 1 FROM games WHERE system_id = ? AND (%1) LIMIT 1").arg(igdbBulkGameGapSql()));
            bulkGapQ.addBindValue(sys.id);
            if (!bulkGapQ.exec()) {
                error = QStringLiteral("IGDB bulk gap check for %1: %2").arg(sys.name, bulkGapQ.lastError().text());
                return false;
            }
            if (!bulkGapQ.next())
                continue;
        }

        // Skip full catalog download when every game with metadata gaps already has an igdb_id.
        {
            QSqlQuery idGapQ(database);
            idGapQ.prepare(QStringLiteral("SELECT 1 FROM games WHERE system_id = ? AND (%1) "
                                          "AND (igdb_id IS NULL OR TRIM(igdb_id) = '') "
                                          "AND NOT EXISTS (SELECT 1 FROM game_facts gf "
                                          "              WHERE gf.game_id = games.game_id "
                                          "                AND gf.field_name = 'igdb_id') "
                                          "LIMIT 1")
                    .arg(igdbBulkGameGapSql()));
            idGapQ.addBindValue(sys.id);
            if (!idGapQ.exec()) {
                error = QStringLiteral("IGDB igdb_id gap check for %1: %2").arg(sys.name, idGapQ.lastError().text());
                return false;
            }
            if (!idGapQ.next())
                continue;
        }

        // Bulk-fetch all IGDB games for this platform
        QHash<QString, QList<GameMetadata>> igdbIndex;
        QList<GameMetadata> cachedGames;
        if (CompendiumPlatformIndexCache::loadPlatformIndex(QStringLiteral("igdb"), igdbSlug, cachedGames)) {
            for (const GameMetadata &gm : cachedGames) {
                if (!gm.title.isEmpty())
                    igdbIndex[normalizeMetadataTitle(gm.title)].append(gm);
            }
            reportCompendiumEnrichmentProgress(QStringLiteral("platform_index"), igdbIndex.size(), -1,
                QStringLiteral("loaded %1 from disk cache").arg(cachedGames.size()));
        } else {
            QList<GameMetadata> fetched;
            int offset = 0;
            while (true) {
                HttpMetadataProvider::processNetworkEvents();
                const QList<GameMetadata> page = provider.fetchGamesByPlatformSlug(igdbSlug, offset, PAGE_SIZE);
                fetched.append(page);
                for (const GameMetadata &gm : page) {
                    if (!gm.title.isEmpty())
                        igdbIndex[normalizeMetadataTitle(gm.title)].append(gm);
                }
                if (offset > 0 && offset % 2500 == 0) {
                    reportCompendiumEnrichmentProgress(QStringLiteral("platform_download"), offset, -1,
                        QStringLiteral("%1 entries for %2").arg(igdbIndex.size()).arg(sys.name));
                }
                if (page.size() < PAGE_SIZE)
                    break;
                offset += PAGE_SIZE;
            }
            if (!fetched.isEmpty())
                CompendiumPlatformIndexCache::storePlatformIndex(QStringLiteral("igdb"), igdbSlug, fetched);
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

        if (!bulkCleared) {
            if (!bulkClearSourceFactBlockers(database, sourceId, error))
                return false;
            bulkCleared = true;
        }

        if (!upsertEnrichmentSource(database,
                SourceSpec {
                    sourceId,
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
                                       "players_max  = COALESCE(players_max, ?), "
                                       "cover_url    = COALESCE(NULLIF(cover_url, ''), ?), "
                                       "series       = COALESCE(NULLIF(series, ''), ?), "
                                       "age_rating   = COALESCE(NULLIF(age_rating, ''), ?), "
                                       "alternate_titles = COALESCE(NULLIF(alternate_titles, ''), ?) "
                                       "WHERE game_id = ?"));

        QSqlQuery factQ(database);
        factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                     "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                     "source_priority, confidence) "
                                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        QSqlQuery delQ(database);
        delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec factSpec {
            sourceId,
            snapshotId,
            70,
            0.80,
        };
        FactReplaceQueries replaceQueries(database);
        EnrichmentBatchWriter batchWriter(database);

        QSqlQuery gamesQ(database);
        gamesQ.prepare(QStringLiteral("SELECT game_id, canonical_title FROM games "
                                      "WHERE system_id = ? "
                                      "  AND (%1)")
                .arg(igdbBulkGameGapSql()));
        gamesQ.addBindValue(sys.id);
        if (!gamesQ.exec()) {
            error = QStringLiteral("Query games for system %1: %2").arg(sys.name, gamesQ.lastError().text());
            return false;
        }

        int sysEnriched = 0;
        int sysProcessed = 0;
        while (gamesQ.next()) {
            ++sysProcessed;
            const QString gameId = gamesQ.value(0).toString();
            if (skipGameIds.contains(gameId)) {
                if (!batchWriter.onGameProcessed(error))
                    return false;
                continue;
            }
            const QString norm = normalizeMetadataTitle(gamesQ.value(1).toString());
            const auto it = igdbIndex.constFind(norm);
            if (it == igdbIndex.cend()) {
                if (!batchWriter.onGameProcessed(error))
                    return false;
                continue;
            }
            const GameMetadata &gm = CompendiumEnrichmentMatchUtils::bestMetadataCandidate(it.value(), true);

            if (!applyIgdbGameMetadata(database, replaceQueries, updateQ, factQ, delQ, factSpec, gameId, gm,
                    gamesEnriched, factsInserted, error)) {
                return false;
            }
            if (updateQ.numRowsAffected() > 0)
                ++sysEnriched;
            if (!batchWriter.onGameProcessed(error))
                return false;
            if (sysProcessed % 2500 == 0) {
                reportCompendiumEnrichmentProgress(QStringLiteral("matching"), sysProcessed, -1,
                    QStringLiteral("%1: %2 enriched").arg(sys.name).arg(sysEnriched), gamesEnriched, factsInserted);
            }
        }

        if (!batchWriter.finish(error))
            return false;

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
