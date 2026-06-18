-- Disc set schema and integrity checks (migration 0007)
-- Run with: validate-compendium-db.sh <db> data/compendium/validation/0004_disc_set_checks.sql
--
-- Phase 1 stubs: pass on an empty bootstrap DB; tighten thresholds after Phase 2 ingest.

SELECT 'schema.game_disc_sets_table' AS check_name,
       CASE WHEN COUNT(*) = 1 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       1 AS expected
FROM sqlite_master
WHERE type = 'table' AND name = 'game_disc_sets';

SELECT 'schema.game_disc_tracks_table' AS check_name,
       CASE WHEN COUNT(*) = 1 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       1 AS expected
FROM sqlite_master
WHERE type = 'table' AND name = 'game_disc_tracks';

SELECT 'schema.idx_game_disc_sets_game' AS check_name,
       CASE WHEN COUNT(*) = 1 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       1 AS expected
FROM sqlite_master
WHERE type = 'index' AND name = 'idx_game_disc_sets_game';

SELECT 'schema.idx_game_disc_sets_set_key' AS check_name,
       CASE WHEN COUNT(*) = 1 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       1 AS expected
FROM sqlite_master
WHERE type = 'index' AND name = 'idx_game_disc_sets_set_key';

SELECT 'schema.idx_game_disc_tracks_disc' AS check_name,
       CASE WHEN COUNT(*) = 1 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       1 AS expected
FROM sqlite_master
WHERE type = 'index' AND name = 'idx_game_disc_tracks_disc';

SELECT 'orphan.game_disc_sets.game_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_disc_sets gds
LEFT JOIN games g ON g.game_id = gds.game_id
WHERE g.game_id IS NULL;

SELECT 'orphan.game_disc_sets.source_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_disc_sets gds
LEFT JOIN sources s ON s.source_id = gds.source_id
WHERE s.source_id IS NULL;

SELECT 'orphan.game_disc_tracks.disc_set_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_disc_tracks gdt
LEFT JOIN game_disc_sets gds ON gds.disc_set_id = gdt.disc_set_id
WHERE gds.disc_set_id IS NULL;

SELECT 'orphan.game_disc_tracks.signature_id' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_disc_tracks gdt
LEFT JOIN game_signatures gs ON gs.signature_id = gdt.signature_id
WHERE gdt.signature_id IS NOT NULL
  AND gs.signature_id IS NULL;

SELECT 'consistency.disc_count_below_disc_number' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_disc_sets
WHERE disc_count > 0
  AND disc_number > disc_count;

-- Per set_key, declared disc_count must cover the highest disc_number present.
SELECT 'consistency.set_key_max_disc_number' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM (
    SELECT set_key
    FROM game_disc_sets
    GROUP BY set_key
    HAVING MAX(disc_number) > MAX(disc_count)
);
