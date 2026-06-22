#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
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

    static const char kMetadataGapSql[] = "genre IS NULL OR TRIM(genre) = '' "
                                          "   OR developer IS NULL OR TRIM(developer) = '' "
                                          "   OR publisher IS NULL OR TRIM(publisher) = '' "
                                          "   OR release_year IS NULL "
                                          "   OR release_date IS NULL OR TRIM(release_date) = '' "
                                          "   OR description IS NULL OR TRIM(description) = '' "
                                          "   OR players_max IS NULL ";

    const GameMetadata &bestTheGamesDbCandidate(const QList<GameMetadata> &candidates) {
        Q_ASSERT(!candidates.isEmpty());
        int bestScore = -1;
        int bestIdx = 0;
        for (int i = 0; i < candidates.size(); ++i) {
            const GameMetadata &c = candidates.at(i);
            const int score = (!c.description.isEmpty() ? 1 : 0) + (!c.developer.isEmpty() ? 1 : 0)
                + (!c.publisher.isEmpty() ? 1 : 0) + (!c.genres.isEmpty() ? 1 : 0) + (c.releaseDate.size() >= 4 ? 1 : 0)
                + (c.players > 0 ? 1 : 0);
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        return candidates.at(bestIdx);
    }

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
    if (!apiKey.isEmpty())
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
                .arg(QLatin1String(kMetadataGapSql)))) {
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
        pendingQ.prepare(QStringLiteral("SELECT game_id, canonical_title FROM games "
                                        "WHERE system_id = ? AND (%1) "
                                        "ORDER BY game_id")
                .arg(QLatin1String(kMetadataGapSql)));
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
        int page = 1;
        while (provider.isAvailable()) {
            const QList<GameMetadata> pageGames = provider.fetchGamesByPlatformId(platformId, page);
            if (pageGames.isEmpty())
                break;
            for (const GameMetadata &gm : pageGames) {
                if (!gm.title.isEmpty())
                    tgdbIndex[normalizeMetadataTitle(gm.title)].append(gm);
            }
            ++page;
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
        for (const PendingGame &game : pending) {
            const auto candidates = tgdbIndex.value(normalizeMetadataTitle(game.title));
            if (candidates.isEmpty()) {
                if (!batchWriter.onGameProcessed(error))
                    return false;
                continue;
            }

            const GameMetadata &gm = bestTheGamesDbCandidate(candidates);
            if (!applyTheGamesDbMetadata(database, replaceQueries, updateQ, factQ, delQ, factSpec, game.gameId, gm,
                    gamesEnriched, factsInserted, error)) {
                return false;
            }
            ++matched;
            if (!batchWriter.onGameProcessed(error))
                return false;
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
