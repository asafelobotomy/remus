-- Artwork storage schema and coverage (migration 0012+).
-- Phase 1: informational checks; hard-fail only on missing schema.
SELECT 'schema.migration_0012.game_assets' AS check_name,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM sqlite_master
      WHERE type = 'table'
        AND name = 'game_assets'
    ) = 1 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  (
    SELECT COUNT(*)
    FROM sqlite_master
    WHERE type = 'table'
      AND name = 'game_assets'
  ) AS observed,
  1 AS expected;
SELECT 'schema.migration_0012.blob_inventory' AS check_name,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM sqlite_master
      WHERE type = 'table'
        AND name = 'blob_inventory'
    ) = 1 THEN 'PASS'
    ELSE 'FAIL'
  END AS status,
  (
    SELECT COUNT(*)
    FROM sqlite_master
    WHERE type = 'table'
      AND name = 'blob_inventory'
  ) AS observed,
  1 AS expected;
SELECT 'artwork.cover_url_populated' AS check_name,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM games
    ) = 0 THEN 'SKIP'
    ELSE 'INFO'
  END AS status,
  (
    SELECT COUNT(*)
    FROM games
    WHERE cover_url IS NOT NULL
      AND TRIM(cover_url) != ''
  ) AS observed,
  (
    SELECT COUNT(*)
    FROM games
  ) AS expected;
SELECT 'artwork.game_assets_box' AS check_name,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM games
    ) = 0 THEN 'SKIP'
    ELSE 'INFO'
  END AS status,
  (
    SELECT COUNT(*)
    FROM game_assets
    WHERE asset_type = 'box'
  ) AS observed,
  (
    SELECT COUNT(*)
    FROM games
  ) AS expected;
SELECT 'artwork.cover_url_local_blob_pct' AS check_name,
  CASE
    WHEN (
      SELECT COUNT(*)
      FROM games
    ) = 0 THEN 'SKIP'
    WHEN (
      SELECT CAST(COUNT(*) AS REAL) * 100.0 / NULLIF(
          (
            SELECT COUNT(*)
            FROM games
          ),
          0
        )
      FROM games
      WHERE cover_url LIKE 'data/remus-thumbnails/blobs/%'
    ) >= 5.0 THEN 'PASS'
    WHEN (
      SELECT CAST(COUNT(*) AS REAL) * 100.0 / NULLIF(
          (
            SELECT COUNT(*)
            FROM games
          ),
          0
        )
      FROM games
      WHERE cover_url LIKE 'data/remus-thumbnails/blobs/%'
    ) >= 1.0 THEN 'WARN'
    ELSE 'INFO'
  END AS status,
  CAST(
    (
      SELECT COUNT(*)
      FROM games
      WHERE cover_url LIKE 'data/remus-thumbnails/blobs/%'
    ) AS REAL
  ) AS observed,
  (
    SELECT COUNT(*)
    FROM games
  ) AS expected;
