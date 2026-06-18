-- Disc set ingest quality checks (populated compendium databases)
-- Run with: validate-compendium-db.sh <db> data/compendium/validation/0005_disc_set_ingest_checks.sql
--
-- FAIL rows block full builds; WARN rows are informational unless --strict is passed.

-- Every disc set row should have at least one linked track once topology ingest is live.
SELECT 'ingest.disc_sets_without_tracks' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM game_disc_sets gds
WHERE NOT EXISTS (
    SELECT 1 FROM game_disc_tracks gdt WHERE gdt.disc_set_id = gds.disc_set_id
);

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

-- Multi-disc sets should have contiguous disc numbers 1..disc_count (informational).
SELECT 'quality.disc_number_gaps' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'WARN' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM (
    SELECT set_key
    FROM game_disc_sets
    WHERE disc_count > 1
    GROUP BY set_key
    HAVING COUNT(DISTINCT disc_number) < MAX(disc_count)
);

-- Disc-based systems should have some topology rows after backfill / full ingest.
SELECT 'coverage.disc_based_systems_with_disc_sets' AS check_name,
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'WARN' END AS status,
       COUNT(*) AS observed,
       0 AS expected
FROM systems s
WHERE s.is_disc_based = 1
  AND EXISTS (SELECT 1 FROM games g WHERE g.system_id = s.system_id)
  AND NOT EXISTS (
      SELECT 1
      FROM game_disc_sets gds
      JOIN games g ON g.game_id = gds.game_id
      WHERE g.system_id = s.system_id
  );
