-- Phase 2 compendium quality checks (informational thresholds)
-- Run with: sqlite3 -header -column <db_path> < data/compendium/validation/0002_phase2_quality_checks.sql

-- Wii U games should have hash signatures when sourced from Redump/Digital only.
SELECT 'identity.wiiu_games_without_signatures' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM games g
JOIN systems s ON s.system_id = g.system_id
WHERE s.display_name = 'Nintendo Wii U'
  AND NOT EXISTS (SELECT 1 FROM game_signatures gs WHERE gs.game_id = g.game_id);

-- Systems seeded in schema but with no ingested games (expected for modern platforms).
SELECT 'coverage.systems_with_zero_games' AS check_name,
       CASE WHEN COUNT(*) <= 8 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       8 AS expected
FROM systems s
LEFT JOIN games g ON g.system_id = s.system_id
GROUP BY s.system_id
HAVING COUNT(g.game_id) = 0;

-- Patch catalog should be populated for translation/patch verification at runtime.
SELECT 'catalog.patch_sources_nonempty' AS check_name,
       CASE WHEN COUNT(*) > 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       1 AS expected
FROM patch_catalog_sources;

-- MAME listxml enrichment prerequisite (file checked at build time; row proxy via arcade dev gaps).
SELECT 'enrichment.arcade_missing_developer' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM games
WHERE system_id = 39
  AND (developer IS NULL OR TRIM(developer) = '');

-- Overall metadata completeness floor (genre filled on at least 55% of games).
SELECT 'enrichment.genre_coverage_pct' AS check_name,
       CASE WHEN ROUND(100.0 * SUM(CASE WHEN genre IS NOT NULL AND TRIM(genre) <> '' THEN 1 ELSE 0 END)
                 / MAX(COUNT(*), 1)) >= 55
            THEN 'PASS' ELSE 'FAIL' END AS status,
       ROUND(100.0 * SUM(CASE WHEN genre IS NOT NULL AND TRIM(genre) <> '' THEN 1 ELSE 0 END)
             / MAX(COUNT(*), 1)) AS observed,
       55 AS expected
FROM games;

-- Shadowed sources: enabled sources with items but zero owned signatures.
SELECT 'coverage.shadowed_sources' AS check_name,
       CASE WHEN COUNT(*) <= 40 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       40 AS expected
FROM (
    SELECT si.source_id
    FROM source_items si
    JOIN sources s ON s.source_id = si.source_id AND s.enabled = 1
    GROUP BY si.source_id
    HAVING COUNT(*) > 100
       AND COALESCE((SELECT COUNT(*) FROM game_signatures gs WHERE gs.source_id = si.source_id), 0) = 0
);
