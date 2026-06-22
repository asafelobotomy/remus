-- Phase 2 extended compendium quality checks (informational thresholds)
-- Run with: sqlite3 -header -column <db_path> < data/compendium/validation/0003_phase2_extended_checks.sql
-- SHA256 signature baseline (modern No-Intro / digital sets).
SELECT 'identity.sha256_signature_count' AS check_name,
  CASE
    WHEN COUNT(*) > 0 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  COUNT(*) AS observed,
  1 AS expected
FROM game_signatures
WHERE hash_type = 'sha256';
-- IGDB bridge coverage on hash-linked games (column or fact).
SELECT 'enrichment.igdb_id_coverage_pct' AS check_name,
  CASE
    WHEN ROUND(
      100.0 * SUM(
        CASE
          WHEN has_igdb THEN 1
          ELSE 0
        END
      ) / MAX(COUNT(*), 1)
    ) >= 1 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  ROUND(
    100.0 * SUM(
      CASE
        WHEN has_igdb THEN 1
        ELSE 0
      END
    ) / MAX(COUNT(*), 1)
  ) AS observed,
  1 AS expected
FROM (
    SELECT g.game_id,
      (
        COALESCE(NULLIF(TRIM(g.igdb_id), ''), '') <> ''
        OR EXISTS (
          SELECT 1
          FROM game_facts gf
          WHERE gf.game_id = g.game_id
            AND gf.field_name = 'igdb_id'
        )
      ) AS has_igdb
    FROM games g
    WHERE EXISTS (
        SELECT 1
        FROM game_signatures gs
        WHERE gs.game_id = g.game_id
      )
  ) AS hash_linked;
-- Post-migration 0006: achievement_count populated where RA facts exist.
SELECT 'enrichment.ra_achievement_count_populated' AS check_name,
  CASE
    WHEN COUNT(*) = 0 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  COUNT(*) AS observed,
  0 AS expected
FROM games g
WHERE EXISTS (
    SELECT 1
    FROM game_facts gf
    WHERE gf.game_id = g.game_id
      AND gf.field_name = 'ra_game_id'
      AND gf.source_id = 'ra'
  )
  AND (
    g.achievement_count IS NULL
    OR g.achievement_count = 0
  );
-- Source items with no signature bridge into games (anti-join; needs idx on source_entry_key).
SELECT 'ingest.unmapped_source_items' AS check_name,
  CASE
    WHEN ROUND(
      100.0 * COUNT(*) / MAX(
        (
          SELECT COUNT(*)
          FROM source_items
        ),
        1
      )
    ) <= 5 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  COUNT(*) AS observed,
  ROUND(
    5.0 * (
      SELECT COUNT(*)
      FROM source_items
    ) / 100.0
  ) AS expected
FROM source_items si
  LEFT JOIN game_signatures gs ON gs.source_entry_key = si.external_key
WHERE gs.source_entry_key IS NULL;
-- Enabled ingest sources (DAT/manifest) that ingested zero items; enrichment APIs are excluded.
SELECT 'ingest.enabled_source_zero_items' AS check_name,
  CASE
    WHEN COUNT(*) = 0 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  COUNT(*) AS observed,
  0 AS expected
FROM sources s
WHERE s.enabled = 1
  AND s.source_type NOT IN ('online-api', 'static-file')
  AND NOT EXISTS (
    SELECT 1
    FROM source_items si
    WHERE si.source_id = s.source_id
  );
-- FTS index coverage vs materialized games.
SELECT 'fts.coverage_vs_games' AS check_name,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM games
    ) = 0 THEN 'PASS'
    WHEN ROUND(
      100.0 * (
        SELECT COUNT(*)
        FROM games_search
      ) / MAX(
        (
          SELECT COUNT(*)
          FROM games
        ),
        1
      )
    ) >= 95 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM games
    ) = 0 THEN 0
    ELSE ROUND(
      100.0 * (
        SELECT COUNT(*)
        FROM games_search
      ) / MAX(
        (
          SELECT COUNT(*)
          FROM games
        ),
        1
      )
    )
  END AS observed,
  95 AS expected;
-- Unresolved merge conflicts (build exits 2 when >0; track in validation report).
SELECT 'merge.unresolved_conflicts' AS check_name,
  CASE
    WHEN COUNT(*) = 0 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  COUNT(*) AS observed,
  0 AS expected
FROM merge_conflicts
WHERE resolution_status = 'unresolved';
-- Bulk enrichment snapshots present after a build (offline or online profile).
-- Offline builds pass with >=2 local bulk snapshots; online builds typically have more.
SELECT 'enrichment.snapshot_presence' AS check_name,
  CASE
    WHEN COUNT(*) >= 2 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  COUNT(*) AS observed,
  2 AS expected
FROM source_snapshots
WHERE snapshot_id IN (
    'libretro-metadata-bulk',
    'gametdb-bulk',
    'openvgdb-v29.0',
    'mame-catver-bulk',
    'mame-listxml-bulk',
    'hasheous-bulk',
    'playmatch-bulk',
    'igdb-bulk',
    'igdb-by-id',
    'retroachievements-bulk',
    'screenscraper-bulk',
    'zxinfo-bulk'
  );
