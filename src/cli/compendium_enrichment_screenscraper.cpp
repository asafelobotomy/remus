#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../core/system_resolver.h"
#include "../metadata/http_metadata_provider.h"
#include "../metadata/screenscraper_provider.h"
#include "../services/credential_manager.h"

#include <QDebug>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

namespace {

    struct PendingGame {
        QString gameId;
        QString systemName;
        QString crc32;
        QString md5;
        QString sha1;
    };

    bool loadPendingGames(QSqlDatabase &database, QList<PendingGame> &pending, QString &error) {
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("SELECT g.game_id, s.display_name "
                                       "FROM games g "
                                       "JOIN systems s ON s.system_id = g.system_id "
                                       "WHERE (g.genre IS NULL OR TRIM(g.genre) = '' "
                                       "   OR g.developer IS NULL OR TRIM(g.developer) = '' "
                                       "   OR g.publisher IS NULL OR TRIM(g.publisher) = '' "
                                       "   OR g.release_year IS NULL "
                                       "   OR g.description IS NULL OR TRIM(g.description) = '') "
                                       "  AND EXISTS ("
                                       "  SELECT 1 FROM game_signatures gs "
                                       "  WHERE gs.game_id = g.game_id "
                                       "    AND gs.hash_type IN ('md5', 'sha1', 'crc32', 'sha256')) "
                                       "ORDER BY g.game_id"))) {
            error = QStringLiteral("Query pending ScreenScraper games: %1").arg(query.lastError().text());
            return false;
        }

        while (query.next()) {
            PendingGame game;
            game.gameId = query.value(0).toString();
            game.systemName = query.value(1).toString();
            pending.append(game);
        }
        query.finish();
        return true;
    }

    bool loadHashes(QSqlDatabase &database, QList<PendingGame> &pending, QString &error) {
        if (pending.isEmpty())
            return true;

        QHash<QString, PendingGame *> byId;
        byId.reserve(pending.size());
        for (PendingGame &game : pending)
            byId.insert(game.gameId, &game);

        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("SELECT game_id, hash_type, hash_value FROM game_signatures "
                                       "WHERE hash_type IN ('md5', 'sha1', 'crc32', 'sha256')"))) {
            error = query.lastError().text();
            return false;
        }

        while (query.next()) {
            PendingGame *game = byId.value(query.value(0).toString());
            if (!game)
                continue;
            const QString type = query.value(1).toString();
            const QString value = query.value(2).toString().trimmed();
            if (type == QStringLiteral("crc32"))
                game->crc32 = value;
            else if (type == QStringLiteral("md5"))
                game->md5 = value;
            else if (type == QStringLiteral("sha1"))
                game->sha1 = value;
        }
        return true;
    }

    QString firstHashForLookup(const PendingGame &game) {
        if (!game.md5.isEmpty())
            return game.md5;
        if (!game.sha1.isEmpty())
            return game.sha1;
        if (!game.crc32.isEmpty())
            return game.crc32;
        return QString();
    }

} // anonymous namespace

bool enrichFromScreenScraper(QSqlDatabase &database, const QString &credentialsPath, int &gamesEnriched,
    int &factsInserted, QString &error, int *apiCallsNeededOut, int *apiCallsPerformedOut) {
    gamesEnriched = 0;
    factsInserted = 0;
    int apiCallsNeeded = 0;
    int apiCallsPerformed = 0;

    const auto loadCredential = [&](const char *key) {
        return CredentialManager::get(QString::fromLatin1(key), credentialsPath);
    };
    const QString username = loadCredential("screenscraper/username");
    const QString password = loadCredential("screenscraper/password");
    const QString devId = loadCredential("screenscraper/devid");
    const QString devPassword = loadCredential("screenscraper/devpassword");
    if (username.isEmpty() || password.isEmpty() || devId.isEmpty() || devPassword.isEmpty()) {
        qInfo() << "[ScreenScraper] Credentials not configured — enrichment skipped";
        return true;
    }

    QList<PendingGame> pending;
    if (!loadPendingGames(database, pending, error))
        return false;
    if (pending.isEmpty())
        return true;

    if (!loadHashes(database, pending, error))
        return false;

    ScreenScraperProvider provider;
    provider.setCredentials(username, password);
    provider.setDeveloperCredentials(devId, devPassword);

    QHash<QString, GameMetadata> matchedMetadata;
    for (const PendingGame &game : pending) {
        const QString hash = firstHashForLookup(game);
        if (hash.isEmpty() || game.systemName.isEmpty())
            continue;
        ++apiCallsNeeded;
        ++apiCallsPerformed;
        if (apiCallsPerformed % 50 == 0)
            HttpMetadataProvider::processNetworkEvents();

        const GameMetadata metadata = provider.getByHash(hash, game.systemName);
        if (!metadata.title.isEmpty())
            matchedMetadata.insert(game.gameId, metadata);
    }

    if (apiCallsNeededOut)
        *apiCallsNeededOut = apiCallsNeeded;
    if (apiCallsPerformedOut)
        *apiCallsPerformedOut = apiCallsPerformed;

    if (matchedMetadata.isEmpty()) {
        qInfo() << "[ScreenScraper] No hash matches for pending games";
        return true;
    }

    const QString sourceId = QStringLiteral("screenscraper");
    const QString snapshotId = QStringLiteral("screenscraper-bulk");

    if (!bulkClearSourceFactBlockers(database, sourceId, error))
        return false;

    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("ScreenScraper"),
                QStringLiteral("online-api"),
                QStringLiteral("https://www.screenscraper.fr"),
                /*attributionRequired=*/true,
                /*priority=*/75,
                QString(),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("ScreenScraper hash enrichment"),
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
        75,
        0.85,
    };
    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    auto insertFact = [&](const QString &gameId, const QString &field, const QString &value,
                          const QString &type = QStringLiteral("text")) -> bool {
        bool inserted = false;
        if (!insertGameFact(replaceQueries,
                delQ, factQ, factSpec, gameId, field, value, type, error, QStringLiteral("screenscraper"), &inserted))
            return false;
        if (inserted)
            ++factsInserted;
        return true;
    };

    for (auto it = matchedMetadata.constBegin(); it != matchedMetadata.constEnd(); ++it) {
        const QString &gameId = it.key();
        const GameMetadata &metadata = it.value();
        const QString genre = metadata.genres.isEmpty() ? QString() : metadata.genres.join(QStringLiteral(", "));

        int releaseYear = 0;
        QString releaseDateStr;
        if (metadata.releaseDate.size() >= 10)
            releaseDateStr = metadata.releaseDate.left(10);
        if (metadata.releaseDate.size() >= 4) {
            bool ok = false;
            releaseYear = metadata.releaseDate.left(4).toInt(&ok);
            if (!ok)
                releaseYear = 0;
        }

        updateQ.bindValue(0, nullableText(metadata.description));
        updateQ.bindValue(1, nullableText(genre));
        updateQ.bindValue(2, nullableText(metadata.developer));
        updateQ.bindValue(3, nullableText(metadata.publisher));
        updateQ.bindValue(4, nullableInt(releaseYear));
        updateQ.bindValue(5, nullableText(releaseDateStr));
        updateQ.bindValue(6, nullableDouble(metadata.rating > 0.0f ? metadata.rating : 0.0));
        updateQ.bindValue(7, nullableInt(metadata.players));
        updateQ.bindValue(8, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("ScreenScraper metadata update for %1").arg(gameId)))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString ratingStr
            = metadata.rating > 0.0f ? QString::number(static_cast<double>(metadata.rating), 'f', 2) : QString();
        const QString playersStr = metadata.players > 0 ? QString::number(metadata.players) : QString();
        if (!insertFact(gameId, QStringLiteral("description"), metadata.description)
            || !insertFact(gameId, QStringLiteral("genre"), genre)
            || !insertFact(gameId, QStringLiteral("developer"), metadata.developer)
            || !insertFact(gameId, QStringLiteral("publisher"), metadata.publisher)
            || !insertFact(gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            || !insertFact(gameId, QStringLiteral("release_date"), releaseDateStr)
            || !insertFact(gameId, QStringLiteral("rating"), ratingStr, QStringLiteral("decimal"))
            || !insertFact(gameId, QStringLiteral("players_max"), playersStr, QStringLiteral("integer")))
            return false;

        if (!batchWriter.onGameProcessed(error))
            return false;
    }

    if (!batchWriter.finish(error))
        return false;

    qInfo().noquote() << QStringLiteral("[ScreenScraper] enrichment complete: %1 games updated, %2 facts")
                             .arg(gamesEnriched)
                             .arg(factsInserted);
    return true;
}

} // namespace CompendiumEnrichment
