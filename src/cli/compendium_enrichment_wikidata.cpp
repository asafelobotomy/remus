#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include "../metadata/wikidata_provider.h"
#include "../metadata/http_metadata_provider.h"

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

    const GameMetadata &bestWikidataCandidate(const QList<GameMetadata> &candidates) {
        Q_ASSERT(!candidates.isEmpty());
        int bestScore = -1;
        int bestIdx = 0;
        for (int i = 0; i < candidates.size(); ++i) {
            const GameMetadata &c = candidates.at(i);
            const int score = (!c.description.isEmpty() ? 1 : 0) + (!c.developer.isEmpty() ? 1 : 0)
                + (!c.publisher.isEmpty() ? 1 : 0) + (!c.genres.isEmpty() ? 1 : 0)
                + (c.releaseDate.size() >= 4 ? 1 : 0);
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        return candidates.at(bestIdx);
    }

    bool applyWikidataMetadata(QSqlDatabase &database, FactReplaceQueries &replaceQueries, QSqlQuery &updateQ,
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
        updateQ.bindValue(6, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update game wikidata")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString wikidataId = gm.externalIds.value(QStringLiteral("wikidata"), gm.id);

        auto insertFact
            = [&](const QString &field, const QString &value, const QString &type = QStringLiteral("text")) {
                  bool inserted = false;
                  if (!insertGameFact(replaceQueries, delQ, factQ, factSpec, gameId, field, value, type, error,
                          QStringLiteral("wikidata"), &inserted))
                      return false;
                  if (inserted)
                      ++factsInserted;
                  return true;
              };

        return insertFact(QStringLiteral("wikidata_id"), wikidataId)
            && insertFact(QStringLiteral("description"), gm.description)
            && insertFact(QStringLiteral("genre"), genreStr) && insertFact(QStringLiteral("developer"), gm.developer)
            && insertFact(QStringLiteral("publisher"), gm.publisher)
            && insertFact(QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            && insertFact(QStringLiteral("release_date"), releaseDateStr);
    }

} // anonymous namespace

bool enrichFromWikidata(QSqlDatabase &database, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    QSqlQuery sysQ(database);
    if (!sysQ.exec(QStringLiteral("SELECT DISTINCT g.system_id, s.display_name FROM games g "
                                  "JOIN systems s ON s.system_id = g.system_id "
                                  "WHERE %1 "
                                  "ORDER BY s.display_name")
                .arg(QLatin1String(kMetadataGapSql)))) {
        error = QStringLiteral("Query systems for Wikidata: %1").arg(sysQ.lastError().text());
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

    qInfo() << "[Wikidata] Starting bulk platform enrichment for" << systems.size() << "systems";

    WikidataProvider provider;
    const QString sourceId = QStringLiteral("wikidata");
    const QString snapshotId = QStringLiteral("wikidata-bulk");
    static constexpr int PAGE_SIZE = 500;
    bool bulkCleared = false;

    for (const SysInfo &sys : systems) {
        HttpMetadataProvider::processNetworkEvents();

        QSqlQuery pendingQ(database);
        pendingQ.prepare(QStringLiteral("SELECT game_id, canonical_title FROM games "
                                        "WHERE system_id = ? AND (%1) "
                                        "ORDER BY game_id")
                .arg(QLatin1String(kMetadataGapSql)));
        pendingQ.addBindValue(sys.id);
        if (!pendingQ.exec()) {
            error = QStringLiteral("Query Wikidata candidates for %1: %2").arg(sys.name, pendingQ.lastError().text());
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

        const QString wikidataPlatform = SystemResolver::providerName(sys.id, Constants::Providers::WIKIDATA);
        const QString platformKey = wikidataPlatform.isEmpty() ? sys.name : wikidataPlatform;

        QHash<QString, QList<GameMetadata>> wikidataIndex;
        int offset = 0;
        while (true) {
            const QList<GameMetadata> page = provider.fetchGamesForPlatform(platformKey, PAGE_SIZE, offset);
            for (const GameMetadata &gm : page) {
                if (!gm.title.isEmpty())
                    wikidataIndex[normalizeMetadataTitle(gm.title)].append(gm);
            }
            if (page.size() < PAGE_SIZE)
                break;
            offset += PAGE_SIZE;
            if (offset % 2000 == 0)
                qInfo().noquote() << QStringLiteral("[Wikidata] %1: indexed %2 entries …").arg(sys.name).arg(offset);
        }

        if (wikidataIndex.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[Wikidata] %1 (%2): no platform matches in Wikidata — skipping")
                                     .arg(sys.name, platformKey);
            continue;
        }

        qInfo().noquote() << QStringLiteral("[Wikidata] %1 (%2): %3 entries indexed, %4 games pending")
                                 .arg(sys.name, platformKey)
                                 .arg(wikidataIndex.size())
                                 .arg(pending.size());

        if (!bulkCleared) {
            if (!bulkClearSourceFactBlockers(database, sourceId, error))
                return false;
            bulkCleared = true;
        }

        if (!upsertEnrichmentSource(database,
                SourceSpec {
                    sourceId,
                    QStringLiteral("Wikidata"),
                    QStringLiteral("online-api"),
                    QStringLiteral("https://www.wikidata.org"),
                    /*attributionRequired=*/true,
                    /*priority=*/40,
                    QStringLiteral("CC0"),
                },
                SnapshotSpec {
                    snapshotId,
                    QStringLiteral("Wikidata SPARQL bulk enrichment"),
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
                                       "release_date = COALESCE(release_date, ?) "
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
            40,
            0.75,
        };
        FactReplaceQueries replaceQueries(database);
        EnrichmentBatchWriter batchWriter(database);

        int matched = 0;
        for (const PendingGame &game : pending) {
            const auto candidates = wikidataIndex.value(normalizeMetadataTitle(game.title));
            if (candidates.isEmpty()) {
                if (!batchWriter.onGameProcessed(error))
                    return false;
                continue;
            }

            const GameMetadata &gm = bestWikidataCandidate(candidates);
            if (!applyWikidataMetadata(database, replaceQueries, updateQ, factQ, delQ, factSpec, game.gameId, gm,
                    gamesEnriched, factsInserted, error)) {
                return false;
            }
            ++matched;
            if (!batchWriter.onGameProcessed(error))
                return false;
        }

        if (!batchWriter.finish(error))
            return false;

        qInfo().noquote() << QStringLiteral("[Wikidata] %1: matched %2 / %3 pending games")
                                 .arg(sys.name)
                                 .arg(matched)
                                 .arg(pending.size());
    }

    return true;
}

} // namespace CompendiumEnrichment
