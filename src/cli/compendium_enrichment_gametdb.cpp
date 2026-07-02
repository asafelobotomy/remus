#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "compendium_progress.h"

#include "../metadata/gametdb_provider.h"
#include "../core/system_resolver.h"

#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace {

using CompendiumEnrichmentSql::bulkClearSourceFactBlockers;
using CompendiumEnrichmentSql::EnrichmentBatchWriter;
using CompendiumEnrichmentSql::execPrepared;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::FactReplaceQueries;
using CompendiumEnrichmentSql::insertGameFact;
using CompendiumEnrichmentSql::loadGamesWithMinSourceFieldFacts;
using CompendiumEnrichmentSql::metadataTitleIndexKeys;
using CompendiumEnrichmentSql::nullableInt;
using CompendiumEnrichmentSql::nullableText;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using CompendiumEnrichmentSql::upsertEnrichmentSource;

} // namespace

namespace CompendiumEnrichment {

bool enrichFromGameTDB(
    QSqlDatabase &database, const QString &gametdbDir, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    Remus::GameTDBProvider provider;
    const int loaded = provider.loadDatabases(gametdbDir);
    if (loaded <= 0) {
        return true;
    }

    const QString sourceId = QStringLiteral("gametdb");
    const QString snapshotId = QStringLiteral("gametdb-bulk");

    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("GameTDB"),
                QStringLiteral("static-file"),
                QStringLiteral("https://www.gametdb.com/"),
                /*attributionRequired=*/true,
                /*priority=*/55,
                QStringLiteral("CC-BY-SA-4.0"),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("GameTDB XML snapshot"),
            },
            error)) {
        return false;
    }

    // Preload CRC32, SHA1, MD5, SHA256 hashes to avoid per-row correlated subqueries (O(N*M) → O(N+M))
    QHash<QString, QString> gameCrc32;
    QHash<QString, QString> gameSha1;
    QHash<QString, QString> gameMd5;
    QHash<QString, QString> gameSha256;
    {
        QSqlQuery q(database);
        if (!q.exec(QStringLiteral("SELECT game_id, hash_type, hash_value FROM game_signatures "
                                   "WHERE hash_type IN ('crc32', 'sha1', 'md5', 'sha256')"))) {
            error = QStringLiteral("Load hashes for GameTDB enrichment: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next()) {
            const QString gid = q.value(0).toString();
            const QString type = q.value(1).toString();
            const QString val = q.value(2).toString();
            if (type == QLatin1String("crc32"))
                gameCrc32.insert(gid, val);
            else if (type == QLatin1String("sha1"))
                gameSha1.insert(gid, val);
            else if (type == QLatin1String("md5"))
                gameMd5.insert(gid, val);
            else if (type == QLatin1String("sha256"))
                gameSha256.insert(gid, val);
        }
    }

    QSqlQuery gameQuery(database);
    if (!gameQuery.exec(QStringLiteral("SELECT game_id, canonical_title FROM games g "
                                       "WHERE (genre IS NULL OR TRIM(genre) = '' "
                                       "   OR developer IS NULL OR TRIM(developer) = '' "
                                       "   OR publisher IS NULL OR TRIM(publisher) = '' "
                                       "   OR players_max IS NULL "
                                       "   OR release_year IS NULL "
                                       "   OR release_date IS NULL OR TRIM(release_date) = '' "
                                       "   OR description IS NULL OR TRIM(description) = '') "
                                       "  AND EXISTS (SELECT 1 FROM game_signatures gs "
                                       "              WHERE gs.game_id = g.game_id "
                                       "                AND gs.hash_type IN ('crc32', 'sha1', 'md5'))"))) {
        error = QStringLiteral("Load games for GameTDB enrichment: %1").arg(gameQuery.lastError().text());
        return false;
    }

    QSqlQuery updateQuery(database);
    updateQuery.prepare(QStringLiteral("UPDATE games SET "
                                       "genre        = COALESCE(NULLIF(genre, ''), ?), "
                                       "developer    = COALESCE(NULLIF(developer, ''), ?), "
                                       "publisher    = COALESCE(NULLIF(publisher, ''), ?), "
                                       "players_max  = COALESCE(players_max, ?), "
                                       "release_year = COALESCE(release_year, ?), "
                                       "release_date = COALESCE(NULLIF(release_date, ''), ?), "
                                       "description  = COALESCE(NULLIF(description, ''), ?) "
                                       "WHERE game_id = ?"));

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery delQuery(database);
    delQuery.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    const FactInsertSpec factSpec {
        sourceId,
        snapshotId,
        55,
        0.90,
    };

    if (!bulkClearSourceFactBlockers(database, sourceId, error))
        return false;

    const QSet<QString> skipGameIds = loadGamesWithMinSourceFieldFacts(database, sourceId, 4, error);
    if (!error.isEmpty())
        return false;
    if (!skipGameIds.isEmpty()) {
        qInfo().noquote()
            << QStringLiteral("[gametdb] Skipping %1 games already enriched by source").arg(skipGameIds.size());
    }

    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    auto insertFact
        = [&](const QString &gameId, const QString &field, const QString &value, const QString &valueType) -> bool {
        bool inserted = false;
        if (!insertGameFact(replaceQueries, delQuery, factQuery, factSpec, gameId, field, value, valueType, error,
                QStringLiteral("gametdb"), &inserted))
            return false;
        if (inserted)
            ++factsInserted;
        return true;
    };

    int scanned = 0;
    while (gameQuery.next()) {
        ++scanned;
        if (scanned % 2500 == 0) {
            qInfo().noquote() << QStringLiteral("[gametdb] Scanned %1 gap games (%2 enriched, %3 facts)")
                                     .arg(scanned)
                                     .arg(gamesEnriched)
                                     .arg(factsInserted);
            reportCompendiumEnrichmentProgress(QStringLiteral("scanning"), scanned, -1,
                QStringLiteral("%1 enriched, %2 facts").arg(gamesEnriched).arg(factsInserted), gamesEnriched,
                factsInserted);
        }
        const QString gameId = gameQuery.value(0).toString();
        if (skipGameIds.contains(gameId)) {
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }
        const QString crc32 = gameCrc32.value(gameId);
        const QString sha1 = gameSha1.value(gameId);
        const QString md5 = gameMd5.value(gameId);
        const QString sha256 = gameSha256.value(gameId);

        // GameTDBProvider::getByHash normalises the hash and checks
        // CRC32 → MD5 → SHA1 indexes in order.
        Remus::GameMetadata meta;
        if (!crc32.isEmpty())
            meta = provider.getByHash(crc32, QString());
        if (meta.title.isEmpty() && !sha256.isEmpty())
            meta = provider.getByHash(sha256, QString());
        if (meta.title.isEmpty() && !sha1.isEmpty())
            meta = provider.getByHash(sha1, QString());
        if (meta.title.isEmpty() && !md5.isEmpty())
            meta = provider.getByHash(md5, QString());

        // Title-based fallback using O(1) index (normalized title → first match).
        // searchByName() is O(N) over the full GameTDB corpus and too slow for
        // batch enrichment across hundreds of thousands of games.
        if (meta.title.isEmpty()) {
            const QString title = gameQuery.value(1).toString();
            if (!title.isEmpty()) {
                for (const QString &key : metadataTitleIndexKeys(title)) {
                    const QString gid = provider.gameIdByNormalizedTitle(key);
                    if (!gid.isEmpty()) {
                        meta = provider.getById(gid);
                        break;
                    }
                }
            }
        }

        if (meta.title.isEmpty()) {
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }

        const QString genre = meta.genres.join(QStringLiteral(", "));
        int releaseYear = 0;
        if (!meta.releaseDate.isEmpty()) {
            const QDate d = QDate::fromString(meta.releaseDate.left(10), QStringLiteral("yyyy-MM-dd"));
            if (d.isValid())
                releaseYear = d.year();
        }

        updateQuery.bindValue(0, nullableText(genre));
        updateQuery.bindValue(1, nullableText(meta.developer));
        updateQuery.bindValue(2, nullableText(meta.publisher));
        updateQuery.bindValue(3, nullableInt(meta.players));
        updateQuery.bindValue(4, nullableInt(releaseYear));
        updateQuery.bindValue(5, releaseYear > 0 ? QVariant(meta.releaseDate.left(10)) : QVariant());
        updateQuery.bindValue(6, nullableText(meta.description));
        updateQuery.bindValue(7, gameId);
        if (!execPrepared(updateQuery, error, QStringLiteral("Update game GameTDB metadata")))
            return false;
        if (updateQuery.numRowsAffected() > 0)
            ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"), genre, QStringLiteral("text")))
            return false;
        if (!insertFact(gameId, QStringLiteral("developer"), meta.developer, QStringLiteral("text")))
            return false;
        if (!insertFact(gameId, QStringLiteral("publisher"), meta.publisher, QStringLiteral("text")))
            return false;
        if (meta.players > 0
            && !insertFact(gameId, QStringLiteral("players_max"), QString::number(meta.players), QStringLiteral("int")))
            return false;
        if (releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"), QString::number(releaseYear), QStringLiteral("int")))
            return false;
        if (releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_date"), meta.releaseDate.left(10), QStringLiteral("text")))
            return false;
        if (!meta.description.isEmpty()
            && !insertFact(gameId, QStringLiteral("description"), meta.description, QStringLiteral("text")))
            return false;

        if (!batchWriter.onGameProcessed(error))
            return false;
    }

    if (!batchWriter.finish(error))
        return false;

    return true;
}

QSet<int> gametdbCoveredSystemIds(const QString &gametdbDir) {
    QSet<int> systemIds;
    if (gametdbDir.isEmpty())
        return systemIds;

    const QDir dir(gametdbDir);
    if (!dir.exists())
        return systemIds;

    struct PrefixMapping {
        const char *prefix;
        const char *typeName;
    };
    static constexpr PrefixMapping kMappings[] = {
        { "wiitdb", "Wii" },
        { "dstdb", "DS" },
        { "3dstdb", "3DS" },
        { "wiiutdb", "WiiU" },
        { "switchtdb", "Switch" },
        { "ps3tdb", "PS3" },
    };

    const QStringList xmlFiles = dir.entryList({ QStringLiteral("*.xml") }, QDir::Files);
    for (const QString &fileName : xmlFiles) {
        const QString baseName = QFileInfo(fileName).baseName().toLower();
        for (const PrefixMapping &mapping : kMappings) {
            if (!baseName.startsWith(QLatin1String(mapping.prefix)))
                continue;
            const int systemId = Remus::SystemResolver::systemIdByName(QString::fromLatin1(mapping.typeName));
            if (systemId > 0)
                systemIds.insert(systemId);
        }
    }

    return systemIds;
}

} // namespace CompendiumEnrichment
