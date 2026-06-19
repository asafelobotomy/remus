#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/http_metadata_provider.h"
#include "../core/constants/providers.h"
#include "../core/constants/settings.h"
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

    struct GameHashes {
        QString crc32;
        QString md5;
        QString sha1;
        QString sha256;
    };

    bool loadPendingGames(QSqlDatabase &database, QStringList &gameIds, QString &error) {
        QSqlQuery query(database);
        if (!query.exec(QStringLiteral("SELECT DISTINCT g.game_id "
                                       "FROM games g "
                                       "WHERE EXISTS ("
                                       "  SELECT 1 FROM game_signatures gs "
                                       "  WHERE gs.game_id = g.game_id "
                                       "    AND gs.hash_type IN ('md5', 'sha1', 'crc32', 'sha256')) "
                                       "  AND (g.igdb_id IS NULL OR TRIM(g.igdb_id) = '') "
                                       "  AND NOT EXISTS ("
                                       "  SELECT 1 FROM game_facts gf "
                                       "  WHERE gf.game_id = g.game_id "
                                       "    AND gf.field_name = 'igdb_id') "
                                       "ORDER BY g.game_id"))) {
            error = QStringLiteral("Query pending Hasheous games: %1").arg(query.lastError().text());
            return false;
        }

        while (query.next())
            gameIds.append(query.value(0).toString());
        query.finish();
        return true;
    }

    bool loadHashesForGames(
        QSqlDatabase &database, const QStringList &gameIds, QHash<QString, GameHashes> &hashesByGame, QString &error) {
        if (gameIds.isEmpty())
            return true;

        constexpr int kChunkSize = 500;
        for (int offset = 0; offset < gameIds.size(); offset += kChunkSize) {
            const QStringList chunk = gameIds.mid(offset, kChunkSize);
            QStringList placeholders;
            placeholders.reserve(chunk.size());
            for (int i = 0; i < chunk.size(); ++i)
                placeholders.append(QStringLiteral("?"));

            QSqlQuery query(database);
            query.prepare(QStringLiteral("SELECT game_id, hash_type, hash_value "
                                         "FROM game_signatures "
                                         "WHERE game_id IN (%1) "
                                         "  AND hash_type IN ('md5', 'sha1', 'crc32', 'sha256')")
                    .arg(placeholders.join(QLatin1Char(','))));
            for (const QString &gameId : chunk)
                query.addBindValue(gameId);

            if (!query.exec()) {
                error = QStringLiteral("Query hashes batch: %1").arg(query.lastError().text());
                return false;
            }

            while (query.next()) {
                const QString gameId = query.value(0).toString();
                const QString type = query.value(1).toString();
                const QString value = query.value(2).toString().trimmed().toLower();

                GameHashes &hashes = hashesByGame[gameId];
                if (type == QStringLiteral("crc32"))
                    hashes.crc32 = value;
                else if (type == QStringLiteral("md5"))
                    hashes.md5 = value;
                else if (type == QStringLiteral("sha1"))
                    hashes.sha1 = value;
                else if (type == QStringLiteral("sha256"))
                    hashes.sha256 = value;
            }
        }

        for (auto it = hashesByGame.begin(); it != hashesByGame.end();) {
            if (it->crc32.isEmpty() && it->md5.isEmpty() && it->sha1.isEmpty() && it->sha256.isEmpty())
                it = hashesByGame.erase(it);
            else
                ++it;
        }

        return true;
    }

} // anonymous namespace

bool enrichFromHasheous(QSqlDatabase &database, const QString &credentialsPath, int &gamesEnriched, int &factsInserted,
    QString &error, int *apiCallsNeededOut, int *apiCallsPerformedOut) {
    gamesEnriched = 0;
    factsInserted = 0;
    int apiCallsNeeded = 0;
    int apiCallsPerformed = 0;

    QStringList pendingGameIds;
    if (!loadPendingGames(database, pendingGameIds, error))
        return false;
    if (pendingGameIds.isEmpty()) {
        if (apiCallsNeededOut)
            *apiCallsNeededOut = 0;
        if (apiCallsPerformedOut)
            *apiCallsPerformedOut = 0;
        return true;
    }

    QHash<QString, GameHashes> hashesByGame;
    if (!loadHashesForGames(database, pendingGameIds, hashesByGame, error))
        return false;
    if (hashesByGame.isEmpty()) {
        if (apiCallsNeededOut)
            *apiCallsNeededOut = 0;
        if (apiCallsPerformedOut)
            *apiCallsPerformedOut = 0;
        return true;
    }

    const QString clientApiKey = CredentialManager::get(
        QString::fromLatin1(Constants::Settings::Providers::HASHEOUS_CLIENT_API_KEY), credentialsPath);

    HasheousProvider provider;
    if (!clientApiKey.isEmpty())
        provider.setApiKey(clientApiKey);

    apiCallsNeeded = hashesByGame.size();
    qInfo().noquote() << QStringLiteral("[Hasheous] %1 games pending igdb_id enrichment (%2 API calls)")
                             .arg(hashesByGame.size())
                             .arg(apiCallsNeeded);

    QHash<QString, GameMetadata> matchedMetadata;
    int callIdx = 0;
    for (auto it = hashesByGame.constBegin(); it != hashesByGame.constEnd(); ++it) {
        ++callIdx;
        ++apiCallsPerformed;
        if (callIdx % 50 == 0)
            HttpMetadataProvider::processNetworkEvents();
        if (callIdx % 100 == 0) {
            qInfo().noquote() << QStringLiteral("[Hasheous] lookup %1/%2 ...").arg(callIdx).arg(apiCallsNeeded);
        }

        const GameMetadata metadata = provider.getByHashes(it->crc32, it->md5, it->sha1, QString(), it->sha256);
        if (metadata.externalIds.contains(Constants::Providers::ExternalId::IGDB))
            matchedMetadata.insert(it.key(), metadata);
    }

    if (matchedMetadata.isEmpty()) {
        if (apiCallsNeededOut)
            *apiCallsNeededOut = apiCallsNeeded;
        if (apiCallsPerformedOut)
            *apiCallsPerformedOut = apiCallsPerformed;
        qInfo() << "[Hasheous] No IGDB IDs resolved for pending games";
        return true;
    }

    if (!database.transaction()) {
        error = QStringLiteral("Failed to start Hasheous enrichment transaction: %1").arg(database.lastError().text());
        return false;
    }

    const QString snapshotId = QStringLiteral("hasheous-bulk");
    if (!upsertEnrichmentSource(database,
            SourceSpec {
                QStringLiteral("hasheous"),
                QStringLiteral("Hasheous"),
                QStringLiteral("online-api"),
                QStringLiteral("https://hasheous.org"),
                /*attributionRequired=*/true,
                /*priority=*/88,
                QString(),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("Hasheous hash bridge enrichment"),
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
                                   "rating       = COALESCE(rating, ?) "
                                   "WHERE game_id = ?"));

    QSqlQuery factQ(database);
    factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                 "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                 "source_priority, confidence) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery delQ(database);
    delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    const FactInsertSpec factSpec {
        QStringLiteral("hasheous"),
        snapshotId,
        88,
        0.85,
    };

    auto insertFact = [&](const QString &gameId, const QString &field, const QString &value,
                          const QString &type = QStringLiteral("text"), bool *insertedOut = nullptr) -> bool {
        bool inserted = false;
        if (!insertGameFact(
                delQ, factQ, factSpec, gameId, field, value, type, error, QStringLiteral("hasheous"), &inserted))
            return false;
        if (inserted) {
            ++factsInserted;
            if (insertedOut)
                *insertedOut = true;
        }
        return true;
    };

    for (auto it = matchedMetadata.constBegin(); it != matchedMetadata.constEnd(); ++it) {
        const QString &gameId = it.key();
        const GameMetadata &metadata = it.value();
        const QString igdbId = metadata.externalIds.value(Constants::Providers::ExternalId::IGDB);
        if (igdbId.isEmpty())
            continue;

        if (!insertFact(gameId, QStringLiteral("igdb_id"), igdbId)) {
            database.rollback();
            return false;
        }

        const bool hasMetadataGap = !metadata.description.isEmpty() || !metadata.genres.isEmpty()
            || !metadata.developer.isEmpty() || !metadata.publisher.isEmpty() || !metadata.releaseDate.isEmpty()
            || metadata.rating > 0.0f;
        if (!hasMetadataGap) {
            ++gamesEnriched;
            continue;
        }

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
            if (releaseDateStr.isEmpty() && releaseYear > 0)
                releaseDateStr = QStringLiteral("%1-01-01").arg(releaseYear);
        }

        updateQ.bindValue(0, nullableText(metadata.description));
        updateQ.bindValue(1, nullableText(genre));
        updateQ.bindValue(2, nullableText(metadata.developer));
        updateQ.bindValue(3, nullableText(metadata.publisher));
        updateQ.bindValue(4, nullableInt(releaseYear));
        updateQ.bindValue(5, nullableText(releaseDateStr));
        updateQ.bindValue(6, nullableDouble(metadata.rating > 0.0f ? metadata.rating : 0.0));
        updateQ.bindValue(7, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Hasheous metadata update for %1").arg(gameId))) {
            database.rollback();
            return false;
        }

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString ratingStr
            = metadata.rating > 0.0f ? QString::number(static_cast<double>(metadata.rating), 'f', 2) : QString();
        if (!insertFact(gameId, QStringLiteral("description"), metadata.description)
            || !insertFact(gameId, QStringLiteral("genre"), genre)
            || !insertFact(gameId, QStringLiteral("developer"), metadata.developer)
            || !insertFact(gameId, QStringLiteral("publisher"), metadata.publisher)
            || !insertFact(gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            || !insertFact(gameId, QStringLiteral("release_date"), releaseDateStr)
            || !insertFact(gameId, QStringLiteral("rating"), ratingStr, QStringLiteral("decimal"))) {
            database.rollback();
            return false;
        }

        ++gamesEnriched;
    }

    if (!database.commit()) {
        error = QStringLiteral("Failed to commit Hasheous enrichment transaction: %1").arg(database.lastError().text());
        database.rollback();
        return false;
    }

    if (apiCallsNeededOut)
        *apiCallsNeededOut = apiCallsNeeded;
    if (apiCallsPerformedOut)
        *apiCallsPerformedOut = apiCallsPerformed;

    qInfo().noquote() << QStringLiteral("[Hasheous] enrichment complete: %1 igdb_id facts, %2 games metadata-updated")
                             .arg(factsInserted)
                             .arg(gamesEnriched);
    return true;
}

} // namespace CompendiumEnrichment
