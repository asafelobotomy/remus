#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"

#include "../metadata/gametdb_provider.h"
#include "../metadata/libretro_metadata_parser.h"

#include <QDate>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace {

using CompendiumEnrichmentSql::execPrepared;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::insertGameFact;
using CompendiumEnrichmentSql::nullableInt;
using CompendiumEnrichmentSql::nullableText;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using CompendiumEnrichmentSql::upsertEnrichmentSource;

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

    if (!upsertEnrichmentSource(
            database,
            SourceSpec{
                sourceId,
                QStringLiteral("Libretro Metadata DAT"),
                QStringLiteral("libretro_metadata"),
                QStringLiteral("https://creativecommons.org/licenses/by-sa/4.0/"),
                /*attributionRequired=*/true,
                /*priority=*/30,
                QStringLiteral("CC-BY-SA-4.0"),
            },
            SnapshotSpec{
                snapshotId,
                QStringLiteral("libretro metadata"),
            },
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
        30,
        0.85,
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
                            QStringLiteral("libretro"),
                            &inserted))
            return false;
        if (inserted) ++factsInserted;
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
                && meta.releaseYear == 0 && meta.description.isEmpty() && !serial.isEmpty())
            meta = parser.lookupBySerial(serial);
        if (meta.genre.isEmpty() && meta.developer.isEmpty()
                && meta.publisher.isEmpty() && meta.maxUsers == 0
                && meta.releaseYear == 0 && meta.description.isEmpty() && !title.isEmpty())
            meta = parser.lookupByName(title);

        const bool hasData = !meta.genre.isEmpty() || !meta.developer.isEmpty()
            || !meta.publisher.isEmpty() || meta.maxUsers > 0 || meta.releaseYear > 0
            || !meta.description.isEmpty();
        if (!hasData) continue;

        updateQuery.bindValue(0, nullableText(meta.genre));
        updateQuery.bindValue(1, nullableText(meta.developer));
        updateQuery.bindValue(2, nullableText(meta.publisher));
        updateQuery.bindValue(3, nullableInt(meta.maxUsers));
        updateQuery.bindValue(4, nullableInt(meta.releaseYear));
        updateQuery.bindValue(5, nullableText(meta.description));
        updateQuery.bindValue(6, gameId);
        if (!execPrepared(updateQuery, error, QStringLiteral("Update game libretro metadata")))
            return false;
        if (updateQuery.numRowsAffected() > 0) ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"),       meta.genre,               QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("developer"),   meta.developer,            QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("publisher"),   meta.publisher,            QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("description"), meta.description,          QStringLiteral("text"))) return false;
        if (meta.maxUsers > 0
            && !insertFact(gameId, QStringLiteral("players_max"),
                           QString::number(meta.maxUsers),     QStringLiteral("int")))   return false;
        if (meta.releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"),
                           QString::number(meta.releaseYear),  QStringLiteral("int")))   return false;
    }

    return true;
}


} // namespace CompendiumEnrichment
