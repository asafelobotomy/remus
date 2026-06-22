#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"

#include "../metadata/gametdb_provider.h"
#include "../metadata/libretro_metadata_parser.h"

#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace {

using CompendiumEnrichmentSql::EnrichmentBatchWriter;
using CompendiumEnrichmentSql::execPrepared;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::FactReplaceQueries;
using CompendiumEnrichmentSql::bulkClearSourceFactBlockers;
using CompendiumEnrichmentSql::insertGameFact;
using CompendiumEnrichmentSql::loadGamesWithMinSourceFieldFacts;
using CompendiumEnrichmentSql::nullableInt;
using CompendiumEnrichmentSql::nullableText;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using CompendiumEnrichmentSql::upsertEnrichmentSource;

} // namespace

namespace CompendiumEnrichment {

bool enrichFromLibretroMetadata(
    QSqlDatabase &database, const QString &metadataDir, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    Remus::LibretroMetadataParser parser;
    const int metadataEntries = parser.loadAll(metadataDir);
    if (metadataEntries <= 0) {
        return true;
    }

    const QString sourceId = QStringLiteral("libretro-metadata");
    const QString snapshotId = QStringLiteral("libretro-metadata-bulk");

    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("Libretro Metadata DAT"),
                QStringLiteral("libretro_metadata"),
                QStringLiteral("https://creativecommons.org/licenses/by-sa/4.0/"),
                /*attributionRequired=*/true,
                /*priority=*/30,
                QStringLiteral("CC-BY-SA-4.0"),
            },
            SnapshotSpec {
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
        if (!q.exec(QStringLiteral("SELECT game_id, hash_value FROM game_signatures WHERE hash_type = 'crc32'"))) {
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
        if (!q.exec(QStringLiteral("SELECT game_id, MIN(serial_value) FROM game_serials GROUP BY game_id"))) {
            error = QStringLiteral("Load serials: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next())
            gameSerial.insert(q.value(0).toString(), q.value(1).toString());
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
                                       "  AND (EXISTS (SELECT 1 FROM game_signatures gs "
                                       "               WHERE gs.game_id = g.game_id AND gs.hash_type = 'crc32') "
                                       "       OR EXISTS (SELECT 1 FROM game_serials s WHERE s.game_id = g.game_id))"))) {
        error = QStringLiteral("Load games for libretro enrichment: %1").arg(gameQuery.lastError().text());
        return false;
    }

    QSqlQuery updateQuery(database);
    updateQuery.prepare(QStringLiteral("UPDATE games SET "
                                       "genre        = COALESCE(genre, ?), "
                                       "developer    = COALESCE(developer, ?), "
                                       "publisher    = COALESCE(publisher, ?), "
                                       "players_max  = COALESCE(players_max, ?), "
                                       "release_year = COALESCE(release_year, ?), "
                                       "release_date = COALESCE(release_date, ?), "
                                       "description  = COALESCE(description, ?) "
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
        30,
        0.85,
    };

    if (!bulkClearSourceFactBlockers(database, sourceId, error))
        return false;

    const QSet<QString> skipGameIds = loadGamesWithMinSourceFieldFacts(database, sourceId, 4, error);
    if (!error.isEmpty())
        return false;
    if (!skipGameIds.isEmpty()) {
        qInfo().noquote() << QStringLiteral("[libretro] Skipping %1 games already enriched by source").arg(skipGameIds.size());
    }

    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    auto insertFact
        = [&](const QString &gameId, const QString &field, const QString &value, const QString &valueType) -> bool {
        bool inserted = false;
        if (!insertGameFact(replaceQueries, delQuery, factQuery, factSpec, gameId, field, value, valueType, error,
                QStringLiteral("libretro"), &inserted))
            return false;
        if (inserted)
            ++factsInserted;
        return true;
    };

    int scanned = 0;
    while (gameQuery.next()) {
        ++scanned;
        if (scanned % 10000 == 0) {
            qInfo().noquote() << QStringLiteral("[libretro] Scanned %1 gap games (%2 enriched, %3 facts)")
                                     .arg(scanned)
                                     .arg(gamesEnriched)
                                     .arg(factsInserted);
        }
        const QString gameId = gameQuery.value(0).toString();
        if (skipGameIds.contains(gameId)) {
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }
        const QString title = gameQuery.value(1).toString();
        const QString crc32 = gameCrc32.value(gameId);
        const QString serial = gameSerial.value(gameId);

        Remus::LibretroMetadata meta;
        if (!crc32.isEmpty())
            meta = parser.lookup(crc32);
        if (meta.genre.isEmpty() && meta.developer.isEmpty() && meta.publisher.isEmpty() && meta.maxUsers == 0
            && meta.releaseYear == 0 && meta.description.isEmpty() && !serial.isEmpty())
            meta = parser.lookupBySerial(serial);
        if (meta.genre.isEmpty() && meta.developer.isEmpty() && meta.publisher.isEmpty() && meta.maxUsers == 0
            && meta.releaseYear == 0 && meta.description.isEmpty() && !title.isEmpty())
            meta = parser.lookupByName(title);

        const bool hasData = !meta.genre.isEmpty() || !meta.developer.isEmpty() || !meta.publisher.isEmpty()
            || meta.maxUsers > 0 || meta.releaseYear > 0 || !meta.description.isEmpty();
        if (!hasData) {
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }

        const QString releaseDate = meta.releaseYear > 0 ? QStringLiteral("%1-01-01").arg(meta.releaseYear) : QString();

        updateQuery.bindValue(0, nullableText(meta.genre));
        updateQuery.bindValue(1, nullableText(meta.developer));
        updateQuery.bindValue(2, nullableText(meta.publisher));
        updateQuery.bindValue(3, nullableInt(meta.maxUsers));
        updateQuery.bindValue(4, nullableInt(meta.releaseYear));
        updateQuery.bindValue(5, nullableText(releaseDate));
        updateQuery.bindValue(6, nullableText(meta.description));
        updateQuery.bindValue(7, gameId);
        if (!execPrepared(updateQuery, error, QStringLiteral("Update game libretro metadata")))
            return false;
        if (updateQuery.numRowsAffected() > 0)
            ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"), meta.genre, QStringLiteral("text")))
            return false;
        if (!insertFact(gameId, QStringLiteral("developer"), meta.developer, QStringLiteral("text")))
            return false;
        if (!insertFact(gameId, QStringLiteral("publisher"), meta.publisher, QStringLiteral("text")))
            return false;
        if (!insertFact(gameId, QStringLiteral("description"), meta.description, QStringLiteral("text")))
            return false;
        if (meta.maxUsers > 0
            && !insertFact(
                gameId, QStringLiteral("players_max"), QString::number(meta.maxUsers), QStringLiteral("int")))
            return false;
        if (meta.releaseYear > 0
            && !insertFact(
                gameId, QStringLiteral("release_year"), QString::number(meta.releaseYear), QStringLiteral("int")))
            return false;
        if (!releaseDate.isEmpty()
            && !insertFact(gameId, QStringLiteral("release_date"), releaseDate, QStringLiteral("text")))
            return false;

        if (!batchWriter.onGameProcessed(error))
            return false;
    }

    if (!batchWriter.finish(error))
        return false;

    return true;
}

} // namespace CompendiumEnrichment
