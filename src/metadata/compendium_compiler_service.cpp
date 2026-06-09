#include "compendium_compiler_service.h"

#include "compendium_dat_extractor.h"
#include "compendium_normalizer.h"
#include "compendium_identity_linker.h"
#include "compendium_fact_inserter.h"
#include "compendium_merge_resolver.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {
namespace Compendium {

namespace {

    // Reassign child rows to winners and delete loser game rows.
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
            QStringLiteral("UPDATE OR IGNORE game_signatures"
                           " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_signatures.game_id)"
                           " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
            QStringLiteral("UPDATE OR IGNORE game_serials"
                           " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_serials.game_id)"
                           " WHERE game_id IN (SELECT loser_id FROM _dedup_map)"),
            QStringLiteral("UPDATE OR IGNORE game_facts"
                           " SET game_id = (SELECT winner_id FROM _dedup_map WHERE loser_id = game_facts.game_id)"
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

    // Remove Sony-style serial rows whose product-code prefix disagrees with the
    // game's primary region (e.g. ULUS on an ASIA game from a bad upstream DAT).
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

    // ── Post-ingest dedup ─────────────────────────────────────────────────────────
    // See header for full documentation.
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

    // ── Compiler service ──────────────────────────────────────────────────────────

    CompilerStats CompendiumCompilerService::run(
        const CompendiumBuildConfig &config, QSqlDatabase &db, QString &error, ProgressCallback onProgress) {
        CompilerStats stats;
        const CompendiumNormalizer normalizer;
        IdentityLinker linker; // stateful: accumulates maps across sources
        const FactInserter inserter;
        const MergeResolver resolver;

        // Count enabled sources once for accurate progress reporting.
        int totalEnabled = 0;
        for (const auto &s : config.sources) {
            if (s.enabled)
                ++totalEnabled;
        }
        int processed = 0;

        for (const CompendiumSourceConfig &src : config.sources) {
            if (!src.enabled) {
                ++stats.skippedDisabled;
                continue;
            }

            if (src.sourceType != QStringLiteral("dat")) {
                error = QStringLiteral("Source '%1' has unsupported source_type '%2' — only 'dat' is supported")
                            .arg(src.sourceId, src.sourceType);
                return stats;
            }

            // ── Extract ───────────────────────────────────────────────────────────
            QString extractError;
            QList<SourceRecordEnvelope> records
                = DatExtractor::extract(src.filePath, src.sourceId, src.snapshotId, extractError);

            if (records.isEmpty()) {
                if (!extractError.isEmpty()) {
                    qWarning() << "[CompendiumCompilerService] Extraction failed for" << src.sourceId << ":"
                               << extractError;
                }
                continue;
            }

            // ── Normalize ─────────────────────────────────────────────────────────
            for (SourceRecordEnvelope &rec : records) {
                normalizer.normalize(rec);
            }

            // ── Link identities (stateful: cross-source maps persist) ─────────────
            linker.link(records);

            // ── Persist ───────────────────────────────────────────────────────────
            if (!inserter.insert(records, db, stats, error)) {
                return stats; // error is set
            }

            ++processed;
            if (onProgress) {
                onProgress(processed, totalEnabled, src.sourceId, stats);
            }
        }

        // ── Post-ingest dedup: merge duplicate titles and shared serials ─────────
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

        // ── Merge resolution (single pass over all accumulated facts) ─────────────
        if (!resolver.resolve(db, stats, error)) {
            return stats;
        }

        return stats;
    }

} // namespace Compendium
} // namespace Remus
