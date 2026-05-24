#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"

#include "../metadata/gametdb_provider.h"

#include <QDate>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace {

using CompendiumEnrichmentSql::execPrepared;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::insertGameFact;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using CompendiumEnrichmentSql::upsertEnrichmentSource;

} // namespace

namespace CompendiumEnrichment {

bool enrichFromGameTDB(QSqlDatabase &database,
                       const QString &gametdbDir,
                       int &gamesEnriched,
                       int &factsInserted,
                       QString &error)
{
    gamesEnriched = 0;
    factsInserted = 0;

    Remus::GameTDBProvider provider;
    const int loaded = provider.loadDatabases(gametdbDir);
    if (loaded <= 0) {
        return true;
    }

    const QString sourceId   = QStringLiteral("gametdb");
    const QString snapshotId = QStringLiteral("gametdb-")
        + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));

    if (!upsertEnrichmentSource(
            database,
            SourceSpec{
                sourceId,
                QStringLiteral("GameTDB"),
                QStringLiteral("gametdb"),
                QStringLiteral("https://www.gametdb.com/"),
                /*attributionRequired=*/true,
                /*priority=*/40,
                QStringLiteral("CC-BY-SA-4.0"),
            },
            SnapshotSpec{
                snapshotId,
                QStringLiteral("GameTDB XML snapshot"),
            },
            error)) {
        return false;
    }

    // Preload CRC32, SHA1, MD5 hashes to avoid per-row correlated subqueries (O(N*M) → O(N+M))
    QHash<QString, QString> gameCrc32;
    QHash<QString, QString> gameSha1;
    QHash<QString, QString> gameMd5;
    {
        QSqlQuery q(database);
        if (!q.exec(QStringLiteral(
                "SELECT game_id, hash_type, hash_value FROM game_signatures "
                "WHERE hash_type IN ('crc32', 'sha1', 'md5')"))) {
            error = QStringLiteral("Load hashes for GameTDB enrichment: %1")
                .arg(q.lastError().text());
            return false;
        }
        while (q.next()) {
            const QString gid  = q.value(0).toString();
            const QString type = q.value(1).toString();
            const QString val  = q.value(2).toString();
            if (type == QLatin1String("crc32"))      gameCrc32.insert(gid, val);
            else if (type == QLatin1String("sha1"))  gameSha1.insert(gid, val);
            else if (type == QLatin1String("md5"))   gameMd5.insert(gid, val);
        }
    }

    QSqlQuery gameQuery(database);
    if (!gameQuery.exec(QStringLiteral("SELECT game_id, canonical_title FROM games"))) {
        error = QStringLiteral("Load games for GameTDB enrichment: %1")
            .arg(gameQuery.lastError().text());
        return false;
    }

    QSqlQuery updateQuery(database);
    updateQuery.prepare(QStringLiteral(
        "UPDATE games SET "
        "genre        = COALESCE(genre, ?), "
        "developer    = COALESCE(developer, ?), "
        "publisher    = COALESCE(publisher, ?), "
        "players_max  = COALESCE(players_max, ?), "
        "release_year = COALESCE(release_year, ?), "
        "description  = COALESCE(description, ?) "
        "WHERE game_id = ?"));

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    const FactInsertSpec factSpec{
        sourceId,
        snapshotId,
        40,
        0.90,
    };

    auto insertFact = [&](const QString &gameId,
                          const QString &field,
                          const QString &value,
                          const QString &valueType) -> bool {
        bool inserted = false;
        if (!insertGameFact(factQuery,
                            factSpec,
                            gameId,
                            field,
                            value,
                            valueType,
                            error,
                            QStringLiteral("gametdb"),
                            &inserted))
            return false;
        if (inserted) ++factsInserted;
        return true;
    };

    while (gameQuery.next()) {
        const QString gameId = gameQuery.value(0).toString();
        const QString crc32  = gameCrc32.value(gameId);
        const QString sha1   = gameSha1.value(gameId);
        const QString md5    = gameMd5.value(gameId);

        // GameTDBProvider::getByHash normalises the hash and checks
        // CRC32 → MD5 → SHA1 indexes in order.
        Remus::GameMetadata meta;
        if (!crc32.isEmpty())
            meta = provider.getByHash(crc32, QString());
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
                const QString gid = provider.gameIdByNormalizedTitle(
                    title.trimmed().toLower());
                if (!gid.isEmpty())
                    meta = provider.getById(gid);
            }
        }

        if (meta.title.isEmpty()) continue;

        const QString genre = meta.genres.join(QStringLiteral(", "));
        int releaseYear = 0;
        if (!meta.releaseDate.isEmpty()) {
            const QDate d = QDate::fromString(meta.releaseDate.left(10),
                                              QStringLiteral("yyyy-MM-dd"));
            if (d.isValid()) releaseYear = d.year();
        }

        auto nullStr = [](const QString &s) {
            return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
        };
        auto nullInt = [](int v) {
            return v > 0 ? QVariant(v) : QVariant(QMetaType(QMetaType::Int));
        };

        updateQuery.bindValue(0, nullStr(genre));
        updateQuery.bindValue(1, nullStr(meta.developer));
        updateQuery.bindValue(2, nullStr(meta.publisher));
        updateQuery.bindValue(3, nullInt(meta.players));
        updateQuery.bindValue(4, nullInt(releaseYear));
        updateQuery.bindValue(5, nullStr(meta.description));
        updateQuery.bindValue(6, gameId);
        if (!execPrepared(updateQuery, error, QStringLiteral("Update game GameTDB metadata")))
            return false;
        if (updateQuery.numRowsAffected() > 0) ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"),        genre,                    QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("developer"),    meta.developer,            QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("publisher"),    meta.publisher,            QStringLiteral("text"))) return false;
        if (meta.players > 0
            && !insertFact(gameId, QStringLiteral("players_max"),
                           QString::number(meta.players),       QStringLiteral("int")))   return false;
        if (releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"),
                           QString::number(releaseYear),        QStringLiteral("int")))   return false;
        if (!meta.description.isEmpty()
            && !insertFact(gameId, QStringLiteral("description"), meta.description,          QStringLiteral("text"))) return false;
    }

    return true;
}

} // namespace CompendiumEnrichment
