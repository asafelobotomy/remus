#include "compendium_enrichment.h"

#include "../metadata/gametdb_provider.h"
#include "../metadata/libretro_metadata_parser.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace {

bool execPrepared(QSqlQuery &query, QString &error, const QString &context)
{
    if (!query.exec()) {
        error = QStringLiteral("%1 failed: %2").arg(context, query.lastError().text());
        return false;
    }
    return true;
}

bool upsertEnrichmentSource(QSqlDatabase &database,
                            const QString &sourceId,
                            const QString &displayName,
                            const QString &sourceType,
                            const QString &licenseUrl,
                            int priority,
                            const QString &snapshotId,
                            const QString &snapshotLabel,
                            QString &error)
{
    QSqlQuery srcQ(database);
    srcQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO sources "
        "(source_id, display_name, source_type, license_id, license_url, attribution_required, priority, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    srcQ.addBindValue(sourceId);
    srcQ.addBindValue(displayName);
    srcQ.addBindValue(sourceType);
    srcQ.addBindValue(QStringLiteral("CC-BY-SA-4.0"));
    srcQ.addBindValue(licenseUrl);
    srcQ.addBindValue(1);
    srcQ.addBindValue(priority);
    srcQ.addBindValue(1);
    if (!execPrepared(srcQ, error, QStringLiteral("Insert source %1").arg(sourceId)))
        return false;

    QSqlQuery snapQ(database);
    snapQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO source_snapshots "
        "(snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    snapQ.addBindValue(snapshotId);
    snapQ.addBindValue(sourceId);
    snapQ.addBindValue(snapshotLabel);
    snapQ.addBindValue(QVariant());
    snapQ.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    snapQ.addBindValue(QVariant());
    if (!execPrepared(snapQ, error, QStringLiteral("Insert snapshot %1").arg(snapshotId)))
        return false;

    return true;
}

} // namespace

namespace CompendiumEnrichment {

bool enrichFromLibretroMetadata(QSqlDatabase &database,
                                const QString &metadataDir,
                                int &gamesEnriched,
                                int &factsInserted,
                                QString &error)
{
    gamesEnriched = 0;
    factsInserted = 0;

    Remus::LibretroMetadataParser parser;
    const int metadataEntries = parser.loadAll(metadataDir);
    if (metadataEntries <= 0) {
        return true;
    }

    const QString sourceId   = QStringLiteral("libretro-metadata");
    const QString snapshotId = QStringLiteral("libretro-metadata-")
        + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));

    if (!upsertEnrichmentSource(database,
                                sourceId,
                                QStringLiteral("Libretro Metadata DAT"),
                                QStringLiteral("libretro_metadata"),
                                QStringLiteral("https://creativecommons.org/licenses/by-sa/4.0/"),
                                30,
                                snapshotId,
                                QStringLiteral("libretro metadata"),
                                error)) {
        return false;
    }

    // Preload CRC32 hashes to avoid per-row correlated subqueries (O(N*M) → O(N+M))
    QHash<QString, QString> gameCrc32;
    {
        QSqlQuery q(database);
        if (!q.exec(QStringLiteral(
                "SELECT game_id, hash_value FROM game_signatures WHERE hash_type = 'crc32'"))) {
            error = QStringLiteral("Load CRC32 hashes: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next())
            gameCrc32.insert(q.value(0).toString(), q.value(1).toString());
    }

    // Preload first serial per game
    QHash<QString, QString> gameSerial;
    {
        QSqlQuery q(database);
        if (!q.exec(QStringLiteral(
                "SELECT game_id, MIN(serial_value) FROM game_serials GROUP BY game_id"))) {
            error = QStringLiteral("Load serials: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next())
            gameSerial.insert(q.value(0).toString(), q.value(1).toString());
    }

    QSqlQuery gameQuery(database);
    if (!gameQuery.exec(QStringLiteral("SELECT game_id, canonical_title FROM games"))) {
        error = QStringLiteral("Load games for libretro enrichment: %1")
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
        "release_year = COALESCE(release_year, ?) "
        "WHERE game_id = ?"));

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    auto insertFact = [&](const QString &gameId,
                          const QString &field,
                          const QString &value,
                          const QString &valueType) -> bool {
        if (value.isEmpty()) return true;
        factQuery.bindValue(0, gameId);
        factQuery.bindValue(1, field);
        factQuery.bindValue(2, value);
        factQuery.bindValue(3, valueType);
        factQuery.bindValue(4, sourceId);
        factQuery.bindValue(5, snapshotId);
        factQuery.bindValue(6, 30);
        factQuery.bindValue(7, 0.85);
        if (!execPrepared(factQuery, error,
                          QStringLiteral("Insert libretro fact %1").arg(field)))
            return false;
        if (factQuery.numRowsAffected() > 0) ++factsInserted;
        return true;
    };

    while (gameQuery.next()) {
        const QString gameId = gameQuery.value(0).toString();
        const QString title  = gameQuery.value(1).toString();
        const QString crc32  = gameCrc32.value(gameId);
        const QString serial = gameSerial.value(gameId);

        Remus::LibretroMetadata meta;
        if (!crc32.isEmpty())
            meta = parser.lookup(crc32);
        if (meta.genre.isEmpty() && meta.developer.isEmpty()
                && meta.publisher.isEmpty() && meta.maxUsers == 0
                && meta.releaseYear == 0 && !serial.isEmpty())
            meta = parser.lookupBySerial(serial);
        if (meta.genre.isEmpty() && meta.developer.isEmpty()
                && meta.publisher.isEmpty() && meta.maxUsers == 0
                && meta.releaseYear == 0 && !title.isEmpty())
            meta = parser.lookupByName(title);

        const bool hasData = !meta.genre.isEmpty() || !meta.developer.isEmpty()
            || !meta.publisher.isEmpty() || meta.maxUsers > 0 || meta.releaseYear > 0;
        if (!hasData) continue;

        auto nullStr = [](const QString &s) {
            return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
        };
        auto nullInt = [](int v) {
            return v > 0 ? QVariant(v) : QVariant(QMetaType(QMetaType::Int));
        };

        updateQuery.bindValue(0, nullStr(meta.genre));
        updateQuery.bindValue(1, nullStr(meta.developer));
        updateQuery.bindValue(2, nullStr(meta.publisher));
        updateQuery.bindValue(3, nullInt(meta.maxUsers));
        updateQuery.bindValue(4, nullInt(meta.releaseYear));
        updateQuery.bindValue(5, gameId);
        if (!execPrepared(updateQuery, error, QStringLiteral("Update game libretro metadata")))
            return false;
        if (updateQuery.numRowsAffected() > 0) ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"),       meta.genre,               QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("developer"),   meta.developer,            QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("publisher"),   meta.publisher,            QStringLiteral("text"))) return false;
        if (meta.maxUsers > 0
            && !insertFact(gameId, QStringLiteral("players_max"),
                           QString::number(meta.maxUsers),     QStringLiteral("int")))   return false;
        if (meta.releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"),
                           QString::number(meta.releaseYear),  QStringLiteral("int")))   return false;
    }

    return true;
}

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

    if (!upsertEnrichmentSource(database,
                                sourceId,
                                QStringLiteral("GameTDB"),
                                QStringLiteral("gametdb"),
                                QStringLiteral("https://www.gametdb.com/"),
                                40,
                                snapshotId,
                                QStringLiteral("GameTDB XML snapshot"),
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
    if (!gameQuery.exec(QStringLiteral("SELECT game_id FROM games"))) {
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
        "release_year = COALESCE(release_year, ?) "
        "WHERE game_id = ?"));

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    auto insertFact = [&](const QString &gameId,
                          const QString &field,
                          const QString &value,
                          const QString &valueType) -> bool {
        if (value.isEmpty()) return true;
        factQuery.bindValue(0, gameId);
        factQuery.bindValue(1, field);
        factQuery.bindValue(2, value);
        factQuery.bindValue(3, valueType);
        factQuery.bindValue(4, sourceId);
        factQuery.bindValue(5, snapshotId);
        factQuery.bindValue(6, 40);
        factQuery.bindValue(7, 0.90);
        if (!execPrepared(factQuery, error,
                          QStringLiteral("Insert GameTDB fact %1").arg(field)))
            return false;
        if (factQuery.numRowsAffected() > 0) ++factsInserted;
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
        updateQuery.bindValue(5, gameId);
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
            && !insertFact(gameId, QStringLiteral("synopsis"),  meta.description,          QStringLiteral("text"))) return false;
    }

    return true;
}

} // namespace CompendiumEnrichment
