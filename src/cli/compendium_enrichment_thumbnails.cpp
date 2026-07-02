#include "compendium_enrichment.h"
#include "compendium_artwork_blob_store.h"
#include "compendium_enrichment_sql.h"
#include "compendium_progress.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using CompendiumEnrichmentSql::EnrichmentBatchWriter;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::FactReplaceQueries;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

bool enrichFromRemusThumbnails(QSqlDatabase &database, int &gamesEnriched, int &factsInserted, QString &error) {
    QSqlQuery tableQ(database);
    if (!tableQ.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_assets' LIMIT 1"))
        || !tableQ.next()) {
        return true;
    }

    SourceSpec source {
        QStringLiteral("remus-thumbnails"),
        QStringLiteral("Remus consolidated artwork"),
        QStringLiteral("static-file"),
        QString(),
        true,
        15,
        QString(),
    };
    SnapshotSpec snapshot {
        QStringLiteral("remus-thumbnails-local"),
        QStringLiteral("Local remus-thumbnails consolidate pass"),
    };
    if (!upsertEnrichmentSource(database, source, snapshot, error)) {
        return false;
    }

    QSqlQuery gamesQ(database);
    if (!gamesQ.exec(QStringLiteral("SELECT g.game_id, ga.storage_path "
                                    "FROM game_assets ga "
                                    "JOIN games g ON g.game_id = ga.game_id "
                                    "WHERE ga.asset_type = 'box' "
                                    "AND (g.cover_url IS NULL OR TRIM(g.cover_url) = '' "
                                    "     OR g.cover_url LIKE 'https://%' OR g.cover_url LIKE 'http://%')"))) {
        error = gamesQ.lastError().text();
        return false;
    }

    FactReplaceQueries replaceQ(database);
    FactInsertSpec factSpec { source.sourceId, snapshot.snapshotId, source.priority, 1.0 };
    EnrichmentBatchWriter batchWriter(database);

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    QSqlQuery delQuery(database);
    delQuery.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    if (!batchWriter.begin(error)) {
        return false;
    }

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral("UPDATE games SET cover_url = ? WHERE game_id = ?"));

    int processed = 0;
    while (gamesQ.next()) {
        ++processed;
        const QString gameId = gamesQ.value(0).toString();
        const QString storagePath = gamesQ.value(1).toString();
        if (gameId.isEmpty() || storagePath.isEmpty()) {
            continue;
        }

        updateQ.addBindValue(storagePath);
        updateQ.addBindValue(gameId);
        if (!updateQ.exec()) {
            error = updateQ.lastError().text();
            return false;
        }
        ++gamesEnriched;

        bool inserted = false;
        if (!insertGameFact(replaceQ, delQuery, factQuery, factSpec, gameId, QStringLiteral("cover_url"), storagePath,
                QStringLiteral("text"), error, QStringLiteral("remus-thumbnails"), &inserted)) {
            return false;
        }
        if (inserted) {
            ++factsInserted;
        }
        if (!batchWriter.onGameProcessed(error)) {
            return false;
        }
        if (processed % 1000 == 0) {
            reportCompendiumEnrichmentProgress(QStringLiteral("linking"), processed, -1,
                QStringLiteral("%1 covers linked").arg(gamesEnriched), gamesEnriched, factsInserted);
        }
    }

    return batchWriter.finish(error);
}

bool ingestRemoteCoverArtIntoBlobStore(QSqlDatabase &database, const QString &repoRoot,
    const QString &thumbnailOutputDir, const QString &sourceId, int &gamesEnriched, QString &error) {
    QSqlQuery gamesQ(database);
    gamesQ.prepare(QStringLiteral("SELECT game_id, cover_url FROM games "
                                  "WHERE cover_url LIKE 'http%' "
                                  "AND game_id NOT IN (SELECT game_id FROM game_assets WHERE asset_type = 'box' "
                                  "AND source_id = ?)"));
    gamesQ.addBindValue(sourceId);
    if (!gamesQ.exec()) {
        error = gamesQ.lastError().text();
        return false;
    }

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral("UPDATE games SET cover_url = ? WHERE game_id = ?"));

    while (gamesQ.next()) {
        const QString gameId = gamesQ.value(0).toString();
        const QString coverUrl = gamesQ.value(1).toString();
        if (gameId.isEmpty() || coverUrl.isEmpty()) {
            continue;
        }

        CompendiumArtworkBlobStore::IngestRemoteImageResult blob;
        if (!CompendiumArtworkBlobStore::ingestRemoteImageToBlobStore(
                database, repoRoot, thumbnailOutputDir, coverUrl, true, 85, 512, blob, error)) {
            qWarning().noquote() << QStringLiteral("[artwork-ingest] skip %1: %2").arg(gameId, error);
            error.clear();
            continue;
        }
        if (!CompendiumArtworkBlobStore::upsertGameAssetFromBlob(
                database, gameId, QStringLiteral("box"), sourceId, coverUrl, blob, error)) {
            return false;
        }

        updateQ.addBindValue(blob.storagePath);
        updateQ.addBindValue(gameId);
        if (!updateQ.exec()) {
            error = updateQ.lastError().text();
            return false;
        }
        ++gamesEnriched;
    }
    return true;
}

} // namespace CompendiumEnrichment
