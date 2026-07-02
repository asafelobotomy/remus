#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "compendium_enrichment_match_utils.h"
#include "compendium_platform_index_cache.h"
#include "compendium_progress.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/http_metadata_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include "../services/credential_manager.h"

#include <QDebug>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;
using CompendiumEnrichmentSql::EnrichmentBatchWriter;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::FactReplaceQueries;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

namespace {

    using CompendiumEnrichmentMatchUtils::bestMetadataCandidate;

    bool applyTheGamesDbMetadata(QSqlDatabase &database, FactReplaceQueries &replaceQueries, QSqlQuery &updateQ,
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
        updateQ.bindValue(6, nullableInt(gm.players));
        updateQ.bindValue(7, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update game thegamesdb")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString playersStr = gm.players > 0 ? QString::number(gm.players) : QString();

        auto insertFact
            = [&](const QString &field, const QString &value, const QString &type = QStringLiteral("text")) {
                  bool inserted = false;
                  if (!insertGameFact(replaceQueries, delQ, factQ, factSpec, gameId, field, value, type, error,
                          QStringLiteral("thegamesdb"), &inserted))
                      return false;
                  if (inserted)
                      ++factsInserted;
                  return true;
              };

        return insertFact(QStringLiteral("thegamesdb_id"), gm.id)
            && insertFact(QStringLiteral("description"), gm.description)
            && insertFact(QStringLiteral("genre"), genreStr) && insertFact(QStringLiteral("developer"), gm.developer)
            && insertFact(QStringLiteral("publisher"), gm.publisher)
            && insertFact(QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            && insertFact(QStringLiteral("release_date"), releaseDateStr)
            && insertFact(QStringLiteral("players_max"), playersStr, QStringLiteral("integer"));
    }

} // anonymous namespace

bool enrichFromTheGamesDB(
    QSqlDatabase &database, const QString &credentialsPath, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    TheGamesDBProvider provider;
    const QString apiKey = CredentialManager::get(QStringLiteral("thegamesdb/api_key"), credentialsPath);
    if (apiKey.isEmpty()) {
        qInfo() << "[TheGamesDB] API key not configured — enrichment skipped";
        return true;
    }
    provider.setApiKey(apiKey);

    if (!provider.isAvailable()) {
        qWarning() << "[TheGamesDB] Monthly request budget exhausted — enrichment skipped";
        return true;
    }

    QSqlQuery sysQ(database);
    if (!sysQ.exec(QStringLiteral("SELECT DISTINCT g.system_id, s.display_name FROM games g "
                                  "JOIN systems s ON s.system_id = g.system_id "
                                  "WHERE %1 "
                                  "ORDER BY s.display_name")
                .arg(QLatin1String(kGameMetadataGapSql)))) {
        error = QStringLiteral("Query systems for TheGamesDB: %1").arg(sysQ.lastError().text());
        return false;
    }

    struct SysInfo {
        int id;
        QString name;
    };
    QList<SysInfo> systems;
    while (sysQ.next())
        systems.append({ sysQ.value(0).toInt(), sysQ.value(1).toString() });
    sysQ.finish();

    if (systems.isEmpty())
        return true;

    qInfo() << "[TheGamesDB] Starting bulk platform enrichment for" << systems.size() << "systems";

    const QString sourceId = QStringLiteral("thegamesdb");
    const QString snapshotId = QStringLiteral("thegamesdb-bulk");
    bool bulkCleared = false;

    const QSet<QString> skipGameIds = loadGamesWithMinSourceFieldFacts(database, sourceId, 4, error);
    if (!error.isEmpty())
        return false;
    if (!skipGameIds.isEmpty()) {
        qInfo().noquote()
            << QStringLiteral("[TheGamesDB] Skipping %1 games already enriched by source").arg(skipGameIds.size());
    }

    for (const SysInfo &sys : systems) {
        if (!provider.isAvailable()) {
            qWarning() << "[TheGamesDB] Monthly request budget reached — stopping early";
            break;
        }

        HttpMetadataProvider::processNetworkEvents();

        const QString platformId = SystemResolver::providerName(sys.id, Constants::Providers::THEGAMESDB);
        if (platformId.isEmpty())
            continue;

        QSqlQuery pendingQ(database);
        pendingQ.prepare(QStringLiteral("SELECT g.game_id, g.canonical_title FROM games g "
                                        "WHERE g.system_id = ? AND (%1) "
                                        "ORDER BY g.game_id")
                .arg(QLatin1String(kGameMetadataGapSql)));
        pendingQ.addBindValue(sys.id);
        if (!pendingQ.exec()) {
            error = QStringLiteral("Query TheGamesDB candidates for %1: %2").arg(sys.name, pendingQ.lastError().text());
            return false;
        }

        struct PendingGame {
            QString gameId;
            QString title;
        };
        QList<PendingGame> pending;
        while (pendingQ.next())
            pending.append({ pendingQ.value(0).toString(), pendingQ.value(1).toString() });
        pendingQ.finish();

        if (pending.isEmpty())
            continue;

        QHash<QString, QList<GameMetadata>> tgdbIndex;
        QList<GameMetadata> cachedGames;
        if (CompendiumPlatformIndexCache::loadPlatformIndex(QStringLiteral("thegamesdb"), platformId, cachedGames)) {
            for (const GameMetadata &gm : cachedGames) {
                if (!gm.title.isEmpty())
                    tgdbIndex[normalizeMetadataTitle(gm.title)].append(gm);
            }
            reportCompendiumEnrichmentProgress(QStringLiteral("platform_index"), tgdbIndex.size(), pending.size(),
                QStringLiteral("loaded %1 from disk cache").arg(cachedGames.size()));
        } else {
            QList<GameMetadata> fetched;
            int page = 1;
            while (provider.isAvailable()) {
                const QList<GameMetadata> pageGames = provider.fetchGamesByPlatformId(platformId, page);
                if (pageGames.isEmpty())
                    break;
                fetched.append(pageGames);
                for (const GameMetadata &gm : pageGames) {
                    if (!gm.title.isEmpty())
                        tgdbIndex[normalizeMetadataTitle(gm.title)].append(gm);
                }
                if (page % 5 == 0) {
                    reportCompendiumEnrichmentProgress(QStringLiteral("platform_download"), page, -1,
                        QStringLiteral("%1 entries for %2").arg(tgdbIndex.size()).arg(sys.name));
                }
                ++page;
            }
            if (!fetched.isEmpty())
                CompendiumPlatformIndexCache::storePlatformIndex(QStringLiteral("thegamesdb"), platformId, fetched);
        }

        if (tgdbIndex.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[TheGamesDB] %1: no platform entries — skipping").arg(sys.name);
            continue;
        }

        qInfo().noquote() << QStringLiteral("[TheGamesDB] %1: %2 entries indexed, %3 games pending")
                                 .arg(sys.name)
                                 .arg(tgdbIndex.size())
                                 .arg(pending.size());

        if (!bulkCleared) {
            if (!bulkClearSourceFactBlockers(database, sourceId, error))
                return false;
            bulkCleared = true;
        }

        if (!upsertEnrichmentSource(database,
                SourceSpec {
                    sourceId,
                    QStringLiteral("TheGamesDB"),
                    QStringLiteral("online-api"),
                    QStringLiteral("https://thegamesdb.net"),
                    /*attributionRequired=*/true,
                    /*priority=*/50,
                    QString(),
                },
                SnapshotSpec {
                    snapshotId,
                    QStringLiteral("TheGamesDB platform bulk enrichment"),
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
            sourceId,
            snapshotId,
            50,
            0.75,
        };
        FactReplaceQueries replaceQueries(database);
        EnrichmentBatchWriter batchWriter(database);

        int matched = 0;
        int processed = 0;
        for (const PendingGame &game : pending) {
            ++processed;
            if (skipGameIds.contains(game.gameId)) {
                if (!batchWriter.onGameProcessed(error))
                    return false;
                continue;
            }
            const auto candidates = tgdbIndex.value(normalizeMetadataTitle(game.title));
            if (candidates.isEmpty()) {
                if (!batchWriter.onGameProcessed(error))
                    return false;
                continue;
            }

            const GameMetadata &gm = bestMetadataCandidate(candidates);
            if (!applyTheGamesDbMetadata(database, replaceQueries, updateQ, factQ, delQ, factSpec, game.gameId, gm,
                    gamesEnriched, factsInserted, error)) {
                return false;
            }
            ++matched;
            if (!batchWriter.onGameProcessed(error))
                return false;
            if (processed % 2500 == 0 || processed == pending.size()) {
                reportCompendiumEnrichmentProgress(QStringLiteral("matching"), processed, pending.size(),
                    QStringLiteral("%1 matched").arg(matched), gamesEnriched, factsInserted);
            }
        }

        if (!batchWriter.finish(error))
            return false;

        qInfo().noquote() << QStringLiteral("[TheGamesDB] %1: matched %2 / %3 pending games")
                                 .arg(sys.name)
                                 .arg(matched)
                                 .arg(pending.size());
    }

    return true;
}

} // namespace CompendiumEnrichment
