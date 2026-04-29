#include "compendium_enrichment.h"

#include "../metadata/gametdb_provider.h"
#include "../metadata/libretro_metadata_parser.h"

#include <QDate>
#include <QDateTime>
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

    QSqlQuery gameQuery(database);
    if (!gameQuery.exec(QStringLiteral(
        "SELECT g.game_id, g.canonical_title, "
        "COALESCE((SELECT gs.hash_value FROM game_signatures gs "
        "          WHERE gs.game_id = g.game_id AND gs.hash_type = 'crc32' LIMIT 1), ''), "
        "COALESCE((SELECT sr.serial_value FROM game_serials sr "
        "          WHERE sr.game_id = g.game_id LIMIT 1), '') "
        "FROM games g"))) {
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
        factQuery.addBindValue(gameId);
        factQuery.addBindValue(field);
        factQuery.addBindValue(value);
        factQuery.addBindValue(valueType);
        factQuery.addBindValue(sourceId);
        factQuery.addBindValue(snapshotId);
        factQuery.addBindValue(30);
        factQuery.addBindValue(0.85);
        if (!execPrepared(factQuery, error,
                          QStringLiteral("Insert libretro fact %1").arg(field)))
            return false;
        if (factQuery.numRowsAffected() > 0) ++factsInserted;
        return true;
    };

    while (gameQuery.next()) {
        const QString gameId = gameQuery.value(0).toString();
        const QString title  = gameQuery.value(1).toString();
        const QString crc32  = gameQuery.value(2).toString();
        const QString serial = gameQuery.value(3).toString();

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

        updateQuery.addBindValue(nullStr(meta.genre));
        updateQuery.addBindValue(nullStr(meta.developer));
        updateQuery.addBindValue(nullStr(meta.publisher));
        updateQuery.addBindValue(nullInt(meta.maxUsers));
        updateQuery.addBindValue(nullInt(meta.releaseYear));
        updateQuery.addBindValue(gameId);
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

    // Fetch all games with their CRC32, SHA1, and MD5 hashes for lookup.
    QSqlQuery gameQuery(database);
    if (!gameQuery.exec(QStringLiteral(
        "SELECT g.game_id, "
        "COALESCE((SELECT gs.hash_value FROM game_signatures gs "
        "          WHERE gs.game_id = g.game_id AND gs.hash_type = 'crc32' LIMIT 1), ''), "
        "COALESCE((SELECT gs.hash_value FROM game_signatures gs "
        "          WHERE gs.game_id = g.game_id AND gs.hash_type = 'sha1' LIMIT 1), ''), "
        "COALESCE((SELECT gs.hash_value FROM game_signatures gs "
        "          WHERE gs.game_id = g.game_id AND gs.hash_type = 'md5'  LIMIT 1), '') "
        "FROM games g"))) {
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
        factQuery.addBindValue(gameId);
        factQuery.addBindValue(field);
        factQuery.addBindValue(value);
        factQuery.addBindValue(valueType);
        factQuery.addBindValue(sourceId);
        factQuery.addBindValue(snapshotId);
        factQuery.addBindValue(40);
        factQuery.addBindValue(0.90);
        if (!execPrepared(factQuery, error,
                          QStringLiteral("Insert GameTDB fact %1").arg(field)))
            return false;
        if (factQuery.numRowsAffected() > 0) ++factsInserted;
        return true;
    };

    while (gameQuery.next()) {
        const QString gameId = gameQuery.value(0).toString();
        const QString crc32  = gameQuery.value(1).toString();
        const QString sha1   = gameQuery.value(2).toString();
        const QString md5    = gameQuery.value(3).toString();

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

        updateQuery.addBindValue(nullStr(genre));
        updateQuery.addBindValue(nullStr(meta.developer));
        updateQuery.addBindValue(nullStr(meta.publisher));
        updateQuery.addBindValue(nullInt(meta.players));
        updateQuery.addBindValue(nullInt(releaseYear));
        updateQuery.addBindValue(gameId);
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
