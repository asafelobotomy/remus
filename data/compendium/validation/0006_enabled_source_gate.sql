-- Strict gate: enabled ingest sources must have ingested at least one item.
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
