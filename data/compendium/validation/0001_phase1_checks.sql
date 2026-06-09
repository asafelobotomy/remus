-- Phase 1 compendium validation report
-- Run with: sqlite3 -header -column <db_path> < data/compendium/validation/0001_phase1_checks.sql

SELECT 'seed_count.systems' AS check_name,
  CASE WHEN (SELECT COUNT(*) FROM systems) = 112 THEN 'PASS' ELSE 'FAIL' END AS status,
       (SELECT COUNT(*) FROM systems) AS observed,
  112 AS expected;

SELECT 'seed_count.regions' AS check_name,
       CASE WHEN (SELECT COUNT(*) FROM regions) = 21 THEN 'PASS' ELSE 'FAIL' END AS status,
       (SELECT COUNT(*) FROM regions) AS observed,
       21 AS expected;

SELECT 'seed_count.merge_policy' AS check_name,
       CASE WHEN (SELECT COUNT(*) FROM merge_policy) = 21 THEN 'PASS' ELSE 'FAIL' END AS status,
       (SELECT COUNT(*) FROM merge_policy) AS observed,
       21 AS expected;

SELECT 'content.games_nonzero' AS check_name,
       CASE WHEN (SELECT COUNT(*) FROM games) > 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       (SELECT COUNT(*) FROM games) AS observed,
       1 AS expected;

SELECT 'content.game_signatures_nonzero' AS check_name,
       CASE WHEN (SELECT COUNT(*) FROM game_signatures) > 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       (SELECT COUNT(*) FROM game_signatures) AS observed,
       1 AS expected;

SELECT 'content.source_items_nonzero' AS check_name,
       CASE WHEN (SELECT COUNT(*) FROM source_items) > 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       (SELECT COUNT(*) FROM source_items) AS observed,
       1 AS expected;

SELECT 'orphan.system_regions.system_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM system_regions sr
LEFT JOIN systems s ON s.system_id = sr.system_id
WHERE s.system_id IS NULL;

SELECT 'orphan.system_regions.region_code' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM system_regions sr
LEFT JOIN regions r ON r.region_code = sr.region_code
WHERE r.region_code IS NULL;

SELECT 'orphan.game_names.game_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_names gn
LEFT JOIN games g ON g.game_id = gn.game_id
WHERE g.game_id IS NULL;

SELECT 'orphan.game_signatures.game_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_signatures gs
LEFT JOIN games g ON g.game_id = gs.game_id
WHERE g.game_id IS NULL;

SELECT 'orphan.game_serials.game_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_serials gs
LEFT JOIN games g ON g.game_id = gs.game_id
WHERE g.game_id IS NULL;

SELECT 'orphan.source_items.snapshot_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM source_items si
LEFT JOIN source_snapshots ss ON ss.snapshot_id = si.snapshot_id
WHERE si.snapshot_id <> ''
  AND ss.snapshot_id IS NULL;

SELECT 'orphan.game_facts.snapshot_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_facts gf
LEFT JOIN source_snapshots ss ON ss.snapshot_id = gf.snapshot_id
WHERE gf.snapshot_id <> ''
  AND ss.snapshot_id IS NULL;

SELECT 'orphan.game_facts.game_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_facts gf
LEFT JOIN games g ON g.game_id = gf.game_id
WHERE g.game_id IS NULL;

SELECT 'collision.hash_signature' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM (
    SELECT hash_type, hash_value
    FROM game_signatures
    GROUP BY hash_type, hash_value
    HAVING COUNT(*) > 1
);

-- Serial collisions are scoped by system_id (identity linker uses system|serial keys).
SELECT 'collision.serial_multi_game' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM (
    SELECT gs.serial_value, g.system_id
    FROM game_serials gs
    JOIN games g ON g.game_id = gs.game_id
    GROUP BY gs.serial_value, g.system_id
    HAVING COUNT(DISTINCT gs.game_id) > 1
);

SELECT 'collision.canonical_resolution_selected_fact_mismatch' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM canonical_resolution cr
JOIN game_facts gf ON gf.fact_id = cr.selected_fact_id
WHERE gf.game_id != cr.game_id
   OR gf.field_name != cr.field_name;

SELECT 'collision.source_items_per_external_key' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM (
    SELECT source_id, snapshot_id, external_key
    FROM source_items
    GROUP BY source_id, snapshot_id, external_key
    HAVING COUNT(*) > 1
);

-- Diagnostic details for any unexpected signature collisions.
SELECT hash_type, hash_value, COUNT(*) AS duplicate_rows
FROM game_signatures
GROUP BY hash_type, hash_value
HAVING COUNT(*) > 1
ORDER BY duplicate_rows DESC, hash_type, hash_value;

-- Diagnostic details for serial values shared by multiple canonical games (same system).
SELECT gs.serial_value, g.system_id, COUNT(DISTINCT gs.game_id) AS games_sharing_serial
FROM game_serials gs
JOIN games g ON g.game_id = gs.game_id
GROUP BY gs.serial_value, g.system_id
HAVING COUNT(DISTINCT gs.game_id) > 1
ORDER BY games_sharing_serial DESC, gs.serial_value, g.system_id;
