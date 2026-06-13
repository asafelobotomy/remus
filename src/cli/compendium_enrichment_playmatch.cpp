#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../metadata/playmatch_provider.h"
#include "../metadata/http_metadata_provider.h"
#include "../core/constants/providers.h"

#include <QDebug>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

namespace {

    struct PendingPlayMatchGame {
        QString gameId;
        QString title;
        qint64 fileSize = 0;
        QString crc32;
        QString md5;
        QString sha1;
    };

    bool loadPendingGames(QSqlDatabase &database, QList<PendingPlayMatchGame> &pending, QString &error) {
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("SELECT g.game_id, g.canonical_title "
                                       "FROM games g "
                                       "WHERE EXISTS ("
                                       "  SELECT 1 FROM game_signatures gs "
                                       "  WHERE gs.game_id = g.game_id "
                                       "    AND gs.hash_type IN ('md5', 'sha1', 'crc32')) "
                                       "  AND NOT EXISTS ("
                                       "  SELECT 1 FROM game_facts gf "
                                       "  WHERE gf.game_id = g.game_id "
                                       "    AND gf.field_name = 'igdb_id' "
                                       "    AND gf.source_id = 'playmatch') "
                                       "ORDER BY g.game_id"))) {
            error = QStringLiteral("Query pending PlayMatch games: %1").arg(query.lastError().text());
            return false;
        }

        while (query.next()) {
            PendingPlayMatchGame game;
            game.gameId = query.value(0).toString();
            game.title = query.value(1).toString();
            pending.append(game);
        }
        query.finish();
        return true;
    }

    bool loadHashesAndSize(QSqlDatabase &database, PendingPlayMatchGame &game, QString &error) {
        QSqlQuery hashQuery(database);
        hashQuery.prepare(QStringLiteral("SELECT hash_type, hash_value FROM game_signatures "
                                         "WHERE game_id = ? AND hash_type IN ('md5', 'sha1', 'crc32')"));
        hashQuery.addBindValue(game.gameId);
        if (!hashQuery.exec()) {
            error = hashQuery.lastError().text();
            return false;
        }
        while (hashQuery.next()) {
            const QString type = hashQuery.value(0).toString();
            const QString value = hashQuery.value(1).toString().trimmed();
            if (type == QStringLiteral("crc32"))
                game.crc32 = value;
            else if (type == QStringLiteral("md5"))
                game.md5 = value;
            else if (type == QStringLiteral("sha1"))
                game.sha1 = value;
        }

        QSqlQuery payloadQuery(database);
        payloadQuery.prepare(QStringLiteral("SELECT payload_json FROM source_items WHERE game_id = ? LIMIT 1"));
        payloadQuery.addBindValue(game.gameId);
        if (payloadQuery.exec() && payloadQuery.next()) {
            const QJsonObject payload = QJsonDocument::fromJson(payloadQuery.value(0).toByteArray()).object();
            const QJsonValue sizeValue = payload.value(QStringLiteral("size"));
            if (sizeValue.isDouble())
                game.fileSize = static_cast<qint64>(sizeValue.toDouble());
            else if (sizeValue.isString()) {
                bool ok = false;
                game.fileSize = sizeValue.toString().toLongLong(&ok);
                if (!ok)
                    game.fileSize = 0;
            }
        }
        return true;
    }

} // anonymous namespace

bool enrichFromPlayMatch(QSqlDatabase &database, int &gamesEnriched, int &factsInserted, QString &error,
    int *apiCallsNeededOut, int *apiCallsPerformedOut) {
    gamesEnriched = 0;
    factsInserted = 0;
    int apiCallsNeeded = 0;
    int apiCallsPerformed = 0;

    QList<PendingPlayMatchGame> pending;
    if (!loadPendingGames(database, pending, error))
        return false;
    if (pending.isEmpty()) {
        if (apiCallsNeededOut)
            *apiCallsNeededOut = 0;
        if (apiCallsPerformedOut)
            *apiCallsPerformedOut = 0;
        return true;
    }

    PlayMatchProvider provider;
    QHash<QString, GameMetadata> matchedMetadata;

    for (PendingPlayMatchGame &game : pending) {
        if (!loadHashesAndSize(database, game, error))
            return false;
        if (game.fileSize <= 0 || game.title.trimmed().isEmpty())
            continue;
        ++apiCallsNeeded;
        ++apiCallsPerformed;
        if (apiCallsPerformed % 50 == 0)
            HttpMetadataProvider::processNetworkEvents();

        const GameMetadata metadata = provider.identifyBySignals(
            game.title, game.fileSize, game.crc32, game.md5, game.sha1, QString(), QString());
        if (metadata.externalIds.contains(Constants::Providers::ExternalId::IGDB))
            matchedMetadata.insert(game.gameId, metadata);
    }

    if (matchedMetadata.isEmpty()) {
        if (apiCallsNeededOut)
            *apiCallsNeededOut = apiCallsNeeded;
        if (apiCallsPerformedOut)
            *apiCallsPerformedOut = apiCallsPerformed;
        return true;
    }

    if (!database.transaction()) {
        error = QStringLiteral("Failed to start PlayMatch enrichment transaction: %1").arg(database.lastError().text());
        return false;
    }

    const QString snapshotId = QStringLiteral("playmatch-bulk");
    if (!upsertEnrichmentSource(database,
            SourceSpec {
                QStringLiteral("playmatch"),
                QStringLiteral("PlayMatch"),
                QStringLiteral("online-api"),
                QStringLiteral("https://playmatch.retrorealm.dev"),
                /*attributionRequired=*/true,
                /*priority=*/88,
                QString(),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("PlayMatch hash bridge enrichment"),
            },
            error)) {
        database.rollback();
        return false;
    }

    QSqlQuery factQ(database);
    factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                 "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                 "source_priority, confidence) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery delQ(database);
    delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    const FactInsertSpec factSpec {
        QStringLiteral("playmatch"),
        snapshotId,
        88,
        0.85,
    };

    for (auto it = matchedMetadata.constBegin(); it != matchedMetadata.constEnd(); ++it) {
        const QString igdbId = it.value().externalIds.value(Constants::Providers::ExternalId::IGDB);
        if (igdbId.isEmpty())
            continue;
        bool inserted = false;
        if (!insertGameFact(delQ, factQ, factSpec, it.key(), QStringLiteral("igdb_id"), igdbId,
                QStringLiteral("text"), error, QStringLiteral("playmatch"), &inserted)) {
            database.rollback();
            return false;
        }
        if (inserted)
            ++factsInserted;
        ++gamesEnriched;
    }

    if (!database.commit()) {
        error = QStringLiteral("Failed to commit PlayMatch enrichment transaction: %1").arg(database.lastError().text());
        database.rollback();
        return false;
    }

    if (apiCallsNeededOut)
        *apiCallsNeededOut = apiCallsNeeded;
    if (apiCallsPerformedOut)
        *apiCallsPerformedOut = apiCallsPerformed;
    qInfo().noquote() << QStringLiteral("[PlayMatch] enrichment complete: %1 igdb_id facts, %2 games updated")
                             .arg(factsInserted)
                             .arg(gamesEnriched);
    return true;
}

} // namespace CompendiumEnrichment
