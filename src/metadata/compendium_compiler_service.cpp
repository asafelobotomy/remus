#include "compendium_compiler_service.h"

#include "compendium_dat_extractor.h"
#include "compendium_normalizer.h"
#include "compendium_identity_linker.h"
#include "compendium_fact_inserter.h"
#include "compendium_merge_resolver.h"
#include "compendium_source_purge.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QAtomicInt>
#include <QThread>
#include <QtConcurrent>

namespace Remus {
namespace Compendium {

    namespace {

        struct SourceExtractBatch {
            CompendiumSourceConfig source;
            QList<SourceRecordEnvelope> records;
            QString error;
        };

        int applyDedupMap(QSqlDatabase &db, QString &error) {
            QSqlQuery q(db);

            if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM _dedup_map"))) {
                error = q.lastError().text();
                return -1;
            }
            q.next();
            const int dupCount = q.value(0).toInt();

            if (dupCount == 0) {
                q.exec(QStringLiteral("DROP TABLE IF EXISTS _dedup_map"));
                return 0;
            }

            const QStringList updates = {
                QStringLiteral("UPDATE OR IGNORE game_names"
                               " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_names.game_id)"
                               " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
                QStringLiteral(
                    "UPDATE OR IGNORE game_signatures"
                    " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_signatures.game_id)"
                    " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
                QStringLiteral("UPDATE OR IGNORE game_serials"
                               " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_serials.game_id)"
                               " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
                QStringLiteral("UPDATE OR IGNORE game_facts"
                               " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_facts.game_id)"
                               " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
                QStringLiteral(
                    "UPDATE OR IGNORE game_disc_sets"
                    " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_disc_sets.game_id)"
                    " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
            };
            for (const QString &sql : updates) {
                if (!q.exec(sql)) {
                    error = q.lastError().text();
                    q.exec(QStringLiteral("DROP TABLE IF EXISTS _dedup_map"));
                    return -1;
                }
            }

            if (!q.exec(QStringLiteral("DELETE FROM games WHERE game_id IN (SELECT loser_id FROM _dedup_map)"))) {
                error = q.lastError().text();
                q.exec(QStringLiteral("DROP TABLE IF EXISTS _dedup_map"));
                return -1;
            }

            q.exec(QStringLiteral("DROP TABLE IF EXISTS _dedup_map"));
            return dupCount;
        }

        int buildAndApplyDedupMap(QSqlDatabase &db, const QString &createMapSql, QString &error) {
            QSqlQuery q(db);
            q.exec(QStringLiteral("DROP TABLE IF EXISTS _dedup_map"));
            if (!q.exec(createMapSql)) {
                error = q.lastError().text();
                return -1;
            }
            return applyDedupMap(db, error);
        }

    } // namespace

    int pruneRegionMismatchedSerials(QSqlDatabase &db, QString &error) {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral(R"(
            DELETE FROM game_serials
            WHERE serial_id IN (
                SELECT gs.serial_id
                FROM game_serials gs
                JOIN games g ON g.game_id = gs.game_id
                WHERE g.primary_region_code IS NOT NULL
                  AND TRIM(g.primary_region_code) != ''
                  AND (
                      (gs.serial_value LIKE 'ULUS-%' AND g.primary_region_code != 'USA')
                   OR (gs.serial_value LIKE 'ULAS-%' AND g.primary_region_code != 'ASIA')
                   OR (gs.serial_value LIKE 'ULES-%' AND g.primary_region_code != 'EUR')
                   OR (gs.serial_value LIKE 'ULJS-%' AND g.primary_region_code != 'JPN')
                   OR (gs.serial_value LIKE 'ULKS-%' AND g.primary_region_code != 'KOR')
                   OR (gs.serial_value LIKE 'SLUS-%' AND g.primary_region_code != 'USA')
                   OR (gs.serial_value LIKE 'SCUS-%' AND g.primary_region_code != 'USA')
                   OR (gs.serial_value LIKE 'SLES-%' AND g.primary_region_code != 'EUR')
                   OR (gs.serial_value LIKE 'SLPS-%' AND g.primary_region_code != 'JPN')
                  )
            ))"))) {
            error = q.lastError().text();
            return -1;
        }
        return q.numRowsAffected();
    }

    int deduplicateGames(QSqlDatabase &db, QString &error) {
        const int pruned = pruneRegionMismatchedSerials(db, error);
        if (pruned < 0) {
            return -1;
        }
        if (pruned > 0) {
            qInfo() << "[deduplicateGames] Pruned" << pruned << "region-mismatched serial row(s).";
        }

        int totalMerged = 0;

        const int titleMerged = buildAndApplyDedupMap(db, QStringLiteral(R"(
        CREATE TEMPORARY TABLE _dedup_map AS
        WITH sig_counts AS (
            SELECT g.game_id, g.system_id, g.canonical_title,
                   COUNT(gs.signature_id) AS sig_count
            FROM games g
            LEFT JOIN game_signatures gs ON gs.game_id = g.game_id
            GROUP BY g.game_id
        ),
        ranked AS (
            SELECT game_id, system_id, canonical_title, sig_count,
                   ROW_NUMBER() OVER (
                       PARTITION BY system_id, canonical_title
                       ORDER BY sig_count DESC, game_id ASC
                   ) AS rn
            FROM sig_counts
            WHERE (system_id, canonical_title) IN (
                SELECT system_id, canonical_title
                FROM games
                GROUP BY system_id, canonical_title
                HAVING COUNT(*) > 1
            )
        )
        SELECT loser.game_id  AS loser_id,
               winner.game_id AS winner_id
        FROM ranked loser
        JOIN ranked winner ON loser.system_id    = winner.system_id
                          AND loser.canonical_title = winner.canonical_title
                          AND winner.rn = 1
        WHERE loser.rn > 1)"),
            error);
        if (titleMerged < 0) {
            return -1;
        }
        totalMerged += titleMerged;

        const int serialMerged = buildAndApplyDedupMap(db, QStringLiteral(R"(
        CREATE TEMPORARY TABLE _dedup_map AS
        WITH sig_counts AS (
            SELECT g.game_id, g.system_id, gs.serial_value,
                   COUNT(sig.signature_id) AS sig_count
            FROM games g
            JOIN game_serials gs ON gs.game_id = g.game_id
            LEFT JOIN game_signatures sig ON sig.game_id = g.game_id
            GROUP BY g.game_id, g.system_id, gs.serial_value
        ),
        ranked AS (
            SELECT game_id, system_id, serial_value, sig_count,
                   ROW_NUMBER() OVER (
                       PARTITION BY system_id, serial_value
                       ORDER BY sig_count DESC, game_id ASC
                   ) AS rn
            FROM sig_counts
            WHERE (system_id, serial_value) IN (
                SELECT g.system_id, gs.serial_value
                FROM games g
                JOIN game_serials gs ON gs.game_id = g.game_id
                GROUP BY g.system_id, gs.serial_value
                HAVING COUNT(DISTINCT g.game_id) > 1
            )
        )
        SELECT loser.game_id  AS loser_id,
               winner.game_id AS winner_id
        FROM ranked loser
        JOIN ranked winner ON loser.system_id    = winner.system_id
                          AND loser.serial_value = winner.serial_value
                          AND winner.rn = 1
        WHERE loser.rn > 1)"),
            error);
        if (serialMerged < 0) {
            return -1;
        }
        totalMerged += serialMerged;

        return totalMerged;
    }

    CompilerStats CompendiumCompilerService::run(const CompendiumBuildConfig &config, QSqlDatabase &db, QString &error,
        ProgressCallback onProgress, const CompilerRunOptions &options) {
        CompilerStats stats;
        const CompendiumNormalizer normalizer;
        IdentityLinker linker;
        const FactInserter inserter;
        const MergeResolver resolver;

        if (options.preloadIdentityLinker) {
            if (!linker.loadFromDatabase(db, error)) {
                error = QStringLiteral("Failed to preload identity linker: %1").arg(error);
                return stats;
            }
        }

        QList<CompendiumSourceConfig> toProcess;
        for (const CompendiumSourceConfig &src : config.sources) {
            if (!src.enabled) {
                ++stats.skippedDisabled;
                continue;
            }
            if (!options.ingestSourceIds.isEmpty() && !options.ingestSourceIds.contains(src.sourceId)) {
                continue;
            }
            if (src.sourceType != QStringLiteral("dat")) {
                error = QStringLiteral("Source '%1' has unsupported source_type '%2' — only 'dat' is supported")
                            .arg(src.sourceId, src.sourceType);
                return stats;
            }
            toProcess.append(src);
        }

        const int totalEnabled = toProcess.size();
        int processed = 0;

        const int parallelism
            = options.extractParallelism <= 0 ? qMax(1, QThread::idealThreadCount()) : options.extractParallelism;

        const auto extractSource = [&](const CompendiumSourceConfig &src) -> SourceExtractBatch {
            SourceExtractBatch batch;
            batch.source = src;
            QString extractError;
            batch.records = DatExtractor::extract(src.filePath, src.sourceId, src.snapshotId, extractError);
            if (batch.records.isEmpty()) {
                if (!extractError.isEmpty()) {
                    batch.error = QStringLiteral("Source '%1': extraction failed: %2").arg(src.sourceId, extractError);
                } else {
                    batch.error = QStringLiteral("Source '%1': DAT produced zero ingestible records: %2")
                                      .arg(src.sourceId, src.filePath);
                }
                return batch;
            }
            for (SourceRecordEnvelope &rec : batch.records) {
                normalizer.normalize(rec);
            }
            return batch;
        };

        QAtomicInt extractCompleted { 0 };
        const auto extractSourceWithProgress = [&](const CompendiumSourceConfig &src) -> SourceExtractBatch {
            SourceExtractBatch batch = extractSource(src);
            if (options.onExtractProgress) {
                const int done = extractCompleted.fetchAndAddRelaxed(1) + 1;
                options.onExtractProgress(done, totalEnabled, src.sourceId);
            }
            return batch;
        };

        QList<SourceExtractBatch> batches;
        if (parallelism <= 1 || toProcess.size() <= 1) {
            batches.reserve(toProcess.size());
            for (const CompendiumSourceConfig &src : toProcess) {
                batches.append(extractSourceWithProgress(src));
            }
        } else {
            qInfo().noquote() << QStringLiteral(
                "[CompendiumCompilerService] Parallel DAT extraction (%1 workers, %2 sources)")
                                     .arg(parallelism)
                                     .arg(toProcess.size());
            batches = QtConcurrent::blockingMapped(toProcess, extractSourceWithProgress);
        }

        for (SourceExtractBatch &batch : batches) {
            if (!batch.error.isEmpty()) {
                error = batch.error;
                return stats;
            }

            if (options.purgeChangedSources) {
                if (!purgeSourceIngestData(db, batch.source.sourceId, error)) {
                    error = QStringLiteral("Failed to purge source '%1': %2").arg(batch.source.sourceId, error);
                    return stats;
                }
            }

            linker.link(batch.records);

            if (!inserter.insert(batch.records, db, stats, error)) {
                return stats;
            }

            ++processed;
            if (onProgress) {
                onProgress(processed, totalEnabled, batch.source.sourceId, stats);
            }
        }

        {
            QString dedupError;
            const int merged = deduplicateGames(db, dedupError);
            if (merged < 0) {
                error = QStringLiteral("Post-ingest dedup failed: %1").arg(dedupError);
                return stats;
            }
            stats.deduplicatedGames = merged;
            if (merged > 0) {
                qInfo() << "[CompendiumCompilerService] Merged" << merged << "duplicate game rows.";
            }
        }

        if (!resolver.resolve(db, stats, error)) {
            return stats;
        }

        return stats;
    }

} // namespace Compendium
} // namespace Remus
