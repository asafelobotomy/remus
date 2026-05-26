#include "compendium_merge_resolver.h"

#include <QSqlError>
#include <QSqlQuery>

namespace Remus {
namespace Compendium {

// ── Helper ────────────────────────────────────────────────────────────────────

static int runInsert(QSqlDatabase &db, const QString &sql, QString &error)
{
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        error = q.lastError().text();
        return -1;
    }
    return q.numRowsAffected();
}

// ── MergeResolver::resolve ────────────────────────────────────────────────────
//
// Implements the merge_policy rules seeded in seeds/0003_merge_policy.sql.
// Each field group runs one or two SQL INSERT statements using SQLite window
// functions (ROW_NUMBER, COUNT) — requires SQLite ≥ 3.25; Qt 6 bundles ≥ 3.39.
// CTE syntax requires SQLite ≥ 3.35.
//
// Rules implemented per field:
//   canonical_title   exact_hash_source_priority (confidence DESC) + shortest_stable_title
//                     [normalized_name_similarity: deferred — requires string distance UDF]
//   developer         most_frequent_value via frequency CTE; tiebreak confidence
//   publisher         same as developer
//   release_date      full_date_preferred (LENGTH DESC) + higher_priority_source
//                     [newer_snapshot: deferred — requires snapshot timestamp join]
//   release_year      derive_from_release_date (join canonical release_date);
//                     fallback max_confidence_year
//   players_max       numeric_valid_range filter (1..16); fallback highest_confidence
//   description       longest_non_boilerplate (LENGTH DESC) + higher_priority_source
//   all others        highest_priority (source_priority DESC, confidence DESC)
//                     covers: genre, rating (normalized_rating_scale approximated
//                     by source_priority), primary_region_code (explicit_region_codes
//                     approximated by source_priority), and any future fields

bool MergeResolver::resolve(QSqlDatabase &db,
                             CompilerStats &stats,
                             QString &error) const
{
    int total = 0;
    int n     = 0;

    // ── 1. canonical_title ────────────────────────────────────────────────────
    // confidence DESC: exact-hash linked records have confidence near 1.0, so
    // they naturally sort first (exact_hash_source_priority).
    // LENGTH(field_value) ASC: shortest non-empty title wins on ties
    // (shortest_stable_title).
    n = runInsert(db, QStringLiteral(
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, field_name, fact_id, "
        "    CASE WHEN cnt = 1 THEN 'single_source' ELSE 'exact_hash_source_priority' END "
        "FROM ( "
        "    SELECT game_id, field_name, fact_id, "
        "           COUNT(*) OVER (PARTITION BY game_id, field_name) AS cnt, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id, field_name "
        "               ORDER BY confidence DESC, source_priority DESC, "
        "                        LENGTH(field_value) ASC, fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts WHERE field_name = 'title' "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 2. developer / publisher (most_frequent_value) ────────────────────────
    // freq CTE: count how many source facts carry each (game_id, field_value).
    // ranked CTE: join facts with their frequency; ROW_NUMBER picks the fact
    // whose value appears most often. Confidence breaks ties within the same
    // frequency bucket (exact_hash_source_priority as primary rule).
    n = runInsert(db, QStringLiteral(
        "WITH freq AS ( "
        "    SELECT game_id, field_name, field_value, COUNT(*) AS cnt "
        "    FROM game_facts "
        "    WHERE field_name IN ('developer', 'publisher') "
        "    GROUP BY game_id, field_name, field_value "
        "), "
        "ranked AS ( "
        "    SELECT gf.game_id, gf.field_name, gf.fact_id, "
        "           COUNT(*) OVER (PARTITION BY gf.game_id, gf.field_name) AS src_cnt, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY gf.game_id, gf.field_name "
        "               ORDER BY f.cnt DESC, gf.confidence DESC, "
        "                        gf.source_priority DESC, gf.fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts gf "
        "    JOIN freq f ON f.game_id = gf.game_id "
        "                AND f.field_name = gf.field_name "
        "                AND f.field_value = gf.field_value "
        ") "
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, field_name, fact_id, "
        "    CASE WHEN src_cnt = 1 THEN 'single_source' ELSE 'most_frequent_value' END "
        "FROM ranked WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 3. release_date (full_date_preferred) ─────────────────────────────────
    // LENGTH(field_value) DESC: full YYYY-MM-DD (10 chars) ranks above YYYY
    // (4 chars). Source priority breaks remaining ties (higher_priority_source).
    n = runInsert(db, QStringLiteral(
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, field_name, fact_id, "
        "    CASE WHEN cnt = 1 THEN 'single_source' ELSE 'full_date_preferred' END "
        "FROM ( "
        "    SELECT game_id, field_name, fact_id, "
        "           COUNT(*) OVER (PARTITION BY game_id, field_name) AS cnt, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id, field_name "
        "               ORDER BY LENGTH(field_value) DESC, source_priority DESC, "
        "                        fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts WHERE field_name = 'release_date' "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 4. release_year — step 1: derive from canonical release_date ──────────
    // date_year CTE: for each game with a resolved release_date, extract the
    // 4-char year prefix. best_year CTE: for each (game_id, year_value) pair,
    // pick the highest-confidence release_year fact with that value.
    // INSERT OR REPLACE: overrides any stale row from a prior run.
    n = runInsert(db, QStringLiteral(
        "WITH date_year AS ( "
        "    SELECT cr.game_id, SUBSTR(gf.field_value, 1, 4) AS derived_year "
        "    FROM canonical_resolution cr "
        "    JOIN game_facts gf ON gf.fact_id = cr.selected_fact_id "
        "    WHERE cr.field_name = 'release_date' "
        "      AND LENGTH(gf.field_value) >= 4 "
        "), "
        "best_year AS ( "
        "    SELECT gf.game_id, gf.fact_id, gf.field_value, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY gf.game_id, gf.field_value "
        "               ORDER BY gf.confidence DESC, gf.source_priority DESC, gf.fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts gf WHERE gf.field_name = 'release_year' "
        ") "
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT d.game_id, 'release_year', b.fact_id, 'derive_from_release_date' "
        "FROM date_year d "
        "JOIN best_year b ON b.game_id = d.game_id "
        "                 AND b.field_value = d.derived_year "
        "                 AND b.rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 4. release_year — step 2: fallback (max_confidence_year) ─────────────
    // INSERT OR IGNORE: only fills games not already handled by step 1.
    n = runInsert(db, QStringLiteral(
        "INSERT OR IGNORE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, 'release_year', fact_id, 'max_confidence_year' "
        "FROM ( "
        "    SELECT game_id, fact_id, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id "
        "               ORDER BY confidence DESC, source_priority DESC, fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts WHERE field_name = 'release_year' "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 5. players_max — step 1: numeric_valid_range (1..16) ─────────────────
    n = runInsert(db, QStringLiteral(
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, 'players_max', fact_id, 'numeric_valid_range' "
        "FROM ( "
        "    SELECT game_id, fact_id, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id "
        "               ORDER BY confidence DESC, source_priority DESC, fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts "
        "    WHERE field_name = 'players_max' "
        "      AND CAST(field_value AS INTEGER) BETWEEN 1 AND 16 "
        "      AND CAST(CAST(field_value AS INTEGER) AS TEXT) = field_value "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 5. players_max — step 2: fallback highest_confidence ──────────────────
    // INSERT OR IGNORE: only fills games where no valid-range fact existed.
    n = runInsert(db, QStringLiteral(
        "INSERT OR IGNORE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, 'players_max', fact_id, 'highest_confidence' "
        "FROM ( "
        "    SELECT game_id, fact_id, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id "
        "               ORDER BY confidence DESC, source_priority DESC, fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts WHERE field_name = 'players_max' "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 6. description (longest_non_boilerplate) ──────────────────────────────
    // LENGTH DESC picks the most content-rich description.
    // Source priority breaks ties (higher_priority_source).
    n = runInsert(db, QStringLiteral(
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, field_name, fact_id, "
        "    CASE WHEN cnt = 1 THEN 'single_source' ELSE 'longest_non_boilerplate' END "
        "FROM ( "
        "    SELECT game_id, field_name, fact_id, "
        "           COUNT(*) OVER (PARTITION BY game_id, field_name) AS cnt, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id, field_name "
        "               ORDER BY LENGTH(field_value) DESC, source_priority DESC, fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts WHERE field_name = 'description' "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    // ── 7. Generic fields ─────────────────────────────────────────────────────
    // Covers: genre (higher_priority_source), rating (normalized_rating_scale
    // approximated by source_priority), primary_region_code
    // (explicit_region_codes approximated by source_priority), and any future
    // field not explicitly handled above.
    n = runInsert(db, QStringLiteral(
        "INSERT OR REPLACE INTO canonical_resolution "
        "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
        "SELECT game_id, field_name, fact_id, "
        "    CASE WHEN cnt = 1 THEN 'single_source' ELSE 'highest_priority' END "
        "FROM ( "
        "    SELECT game_id, field_name, fact_id, "
        "           COUNT(*) OVER (PARTITION BY game_id, field_name) AS cnt, "
        "           ROW_NUMBER() OVER ( "
        "               PARTITION BY game_id, field_name "
        "               ORDER BY source_priority DESC, confidence DESC, fact_id ASC "
        "           ) AS rn "
        "    FROM game_facts "
        "    WHERE field_name NOT IN ( "
        "        'title', 'canonical_title', 'developer', 'publisher', "
        "        'release_date', 'release_year', 'players_max', 'description') "
        ") WHERE rn = 1"), error);
    if (n < 0) return false;
    total += n;

    stats.resolvedFields = total;

    // ── Record merge conflicts ────────────────────────────────────────────────
    // Build current conflict state in a temp table, then apply a differential
    // update to merge_conflicts to avoid full delete+reinsert churn.
    n = runInsert(db, QStringLiteral(
        "CREATE TEMP TABLE IF NOT EXISTS _current_merge_conflicts ("
        "game_id TEXT NOT NULL, "
        "field_name TEXT NOT NULL, "
        "fact_ids_json TEXT NOT NULL, "
        "resolution_status TEXT NOT NULL, "
        "chosen_fact_id INTEGER, "
        "PRIMARY KEY (game_id, field_name))"), error);
    if (n < 0) return false;

    n = runInsert(db, QStringLiteral("DELETE FROM _current_merge_conflicts"), error);
    if (n < 0) return false;

    n = runInsert(db, QStringLiteral(
        "INSERT INTO _current_merge_conflicts "
        "    (game_id, field_name, fact_ids_json, resolution_status, chosen_fact_id) "
        "SELECT "
        "    gf.game_id, "
        "    gf.field_name, "
        "    json_group_array(gf.fact_id), "
        "    CASE WHEN MAX(cr.selected_fact_id) IS NOT NULL "
        "         THEN 'auto_resolved' ELSE 'unresolved' END, "
        "    MAX(cr.selected_fact_id) "
        "FROM game_facts gf "
        "LEFT JOIN canonical_resolution cr "
        "    ON cr.game_id = gf.game_id AND cr.field_name = gf.field_name "
        "GROUP BY gf.game_id, gf.field_name "
        "HAVING COUNT(DISTINCT gf.field_value) > 1"), error);
    if (n < 0) return false;

    n = runInsert(db, QStringLiteral(
        "DELETE FROM merge_conflicts "
        "WHERE NOT EXISTS ("
        "    SELECT 1 FROM _current_merge_conflicts cur "
        "    WHERE cur.game_id = merge_conflicts.game_id "
        "      AND cur.field_name = merge_conflicts.field_name)"), error);
    if (n < 0) return false;

    n = runInsert(db, QStringLiteral(
        "UPDATE merge_conflicts "
        "SET fact_ids_json = ("
        "        SELECT cur.fact_ids_json FROM _current_merge_conflicts cur "
        "        WHERE cur.game_id = merge_conflicts.game_id "
        "          AND cur.field_name = merge_conflicts.field_name), "
        "    resolution_status = ("
        "        SELECT cur.resolution_status FROM _current_merge_conflicts cur "
        "        WHERE cur.game_id = merge_conflicts.game_id "
        "          AND cur.field_name = merge_conflicts.field_name), "
        "    chosen_fact_id = ("
        "        SELECT cur.chosen_fact_id FROM _current_merge_conflicts cur "
        "        WHERE cur.game_id = merge_conflicts.game_id "
        "          AND cur.field_name = merge_conflicts.field_name), "
        "    resolved_at = CASE "
        "        WHEN (SELECT cur.resolution_status FROM _current_merge_conflicts cur "
        "              WHERE cur.game_id = merge_conflicts.game_id "
        "                AND cur.field_name = merge_conflicts.field_name) = 'unresolved' "
        "        THEN NULL "
        "        ELSE COALESCE(merge_conflicts.resolved_at, CURRENT_TIMESTAMP) "
        "    END "
        "WHERE EXISTS ("
        "    SELECT 1 FROM _current_merge_conflicts cur "
        "    WHERE cur.game_id = merge_conflicts.game_id "
        "      AND cur.field_name = merge_conflicts.field_name)"), error);
    if (n < 0) return false;

    n = runInsert(db, QStringLiteral(
        "INSERT INTO merge_conflicts "
        "    (game_id, field_name, fact_ids_json, resolution_status, chosen_fact_id, resolved_at) "
        "SELECT cur.game_id, cur.field_name, cur.fact_ids_json, cur.resolution_status, "
        "       cur.chosen_fact_id, "
        "       CASE WHEN cur.resolution_status = 'unresolved' THEN NULL ELSE CURRENT_TIMESTAMP END "
        "FROM _current_merge_conflicts cur "
        "WHERE NOT EXISTS ("
        "    SELECT 1 FROM merge_conflicts mc "
        "    WHERE mc.game_id = cur.game_id AND mc.field_name = cur.field_name)"), error);
    if (n < 0) return false;

    // Update unresolved conflict count for the build report.
    {
        QSqlQuery cq(db);
        if (cq.exec(QStringLiteral(
                "SELECT COUNT(*) FROM merge_conflicts "
                "WHERE resolution_status = 'unresolved'"))
                && cq.next()) {
            stats.unresolvedConflicts = cq.value(0).toInt();
        }
    }

    // ── Materialize canonical_resolution → games.* ───────────────────────────
    // Propagates the winning fact value for each field back to the denormalised
    // games columns so that CompendiumProvider queries read resolved data.
    // field_name in game_facts → column name in games (most are identical).
    static constexpr struct { const char *factName; const char *column; } kFields[] = {
        {"title",            "canonical_title"},
        {"developer",        "developer"},
        {"publisher",        "publisher"},
        {"release_date",     "release_date"},
        {"release_year",     "release_year"},
        {"players_max",      "players_max"},
        {"description",      "description"},
        {"genre",            "genre"},
        {"rating",           "rating"},
        {"region",           "primary_region_code"},
    };

    for (const auto &f : kFields) {
        const QString sql = QStringLiteral(
            "UPDATE games "
            "SET \"%1\" = ("
            "    SELECT gf.field_value "
            "    FROM canonical_resolution cr "
            "    JOIN game_facts gf ON gf.fact_id = cr.selected_fact_id "
            "    WHERE cr.game_id = games.game_id AND cr.field_name = '%2'"
            ") "
            "WHERE EXISTS ("
            "    SELECT 1 FROM canonical_resolution "
            "    WHERE game_id = games.game_id AND field_name = '%2'"
            ")")
            .arg(QLatin1String(f.column), QLatin1String(f.factName));
        // runInsert handles both INSERT and UPDATE — the impl is identical.
        if (runInsert(db, sql, error) < 0) return false;
    }

    // Materialize canonical_confidence from the winning canonical_title fact.
    if (runInsert(db, QStringLiteral(
            "UPDATE games "
            "SET canonical_confidence = ("
            "    SELECT gf.confidence "
            "    FROM canonical_resolution cr "
            "    JOIN game_facts gf ON gf.fact_id = cr.selected_fact_id "
            "    WHERE cr.game_id = games.game_id AND cr.field_name = 'title'"
            ") "
            "WHERE EXISTS ("
            "    SELECT 1 FROM canonical_resolution "
            "    WHERE game_id = games.game_id AND field_name = 'title'"
            ")"), error) < 0) {
        return false;
    }

    // ── R2 fallback: derive release_year from release_date ────────────────────
    // The canonical_resolution system (step 3) can only resolve release_year
    // when a matching game_facts row with field_name='release_year' exists.
    // If an enricher wrote only a release_date fact (no separate release_year),
    // the steps above leave release_year NULL.  Derive it directly here so the
    // games.release_year column is never blank when release_date is available.
    if (runInsert(db, QStringLiteral(
            "UPDATE games "
            "SET release_year = SUBSTR(release_date, 1, 4) "
            "WHERE release_year IS NULL "
            "  AND release_date IS NOT NULL "
            "  AND LENGTH(release_date) >= 4"), error) < 0) {
        return false;
    }

    return true;
}

} // namespace Compendium
} // namespace Remus
