#!/usr/bin/env bash
# Field-level completeness and validation-tier summary for a populated compendium DB.
#
# Usage:
#   scripts/audit_compendium_data.sh [db_path]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required" >&2
    exit 1
fi

echo "==> Compendium data audit: $DB_PATH"
echo ""

sqlite3 -header -column "$DB_PATH" "
SELECT 'games' AS metric, COUNT(*) AS cnt FROM games
UNION ALL SELECT 'game_signatures', COUNT(*) FROM game_signatures
UNION ALL SELECT 'source_items', COUNT(*) FROM source_items
UNION ALL SELECT 'game_disc_sets', COUNT(*) FROM game_disc_sets
UNION ALL SELECT 'game_disc_tracks', COUNT(*) FROM game_disc_tracks
UNION ALL SELECT 'game_assets', COUNT(*) FROM game_assets
UNION ALL SELECT 'patch_entries', COUNT(*) FROM patch_entries;
"

echo ""
echo "==> Field completeness (games)"
sqlite3 -header -column "$DB_PATH" "
SELECT
  COUNT(*) AS total_games,
  ROUND(100.0*SUM(CASE WHEN genre IS NOT NULL AND TRIM(genre)!='' THEN 1 ELSE 0 END)/COUNT(*),1) AS pct_genre,
  ROUND(100.0*SUM(CASE WHEN developer IS NOT NULL AND TRIM(developer)!='' THEN 1 ELSE 0 END)/COUNT(*),1) AS pct_developer,
  ROUND(100.0*SUM(CASE WHEN description IS NOT NULL AND TRIM(description)!='' THEN 1 ELSE 0 END)/COUNT(*),1) AS pct_description,
  ROUND(100.0*SUM(CASE WHEN cover_url IS NOT NULL AND TRIM(cover_url)!='' THEN 1 ELSE 0 END)/COUNT(*),1) AS pct_cover_url,
  ROUND(100.0*SUM(CASE WHEN igdb_id IS NOT NULL AND TRIM(igdb_id)!='' THEN 1 ELSE 0 END)/COUNT(*),1) AS pct_igdb,
  ROUND(100.0*SUM(CASE WHEN ra_game_id IS NOT NULL AND TRIM(ra_game_id)!='' THEN 1 ELSE 0 END)/COUNT(*),1) AS pct_ra
FROM games;
"

echo ""
echo "==> Hash types"
sqlite3 -header -column "$DB_PATH" "
SELECT hash_type, COUNT(*) AS cnt FROM game_signatures GROUP BY hash_type ORDER BY cnt DESC;
"

echo ""
echo "==> Disc set gaps"
sqlite3 -header -column "$DB_PATH" "
SELECT COUNT(*) AS disc_sets_without_tracks
FROM game_disc_sets gds
WHERE NOT EXISTS (SELECT 1 FROM game_disc_tracks gdt WHERE gdt.disc_set_id = gds.disc_set_id);
"

if [[ -x "$ROOT_DIR/build/remus-cli" ]]; then
    echo ""
    echo "==> Coverage report (summary)"
    "$ROOT_DIR/build/remus-cli" --coverage-report --compendium-output "$DB_PATH" 2>/dev/null | head -3 || true
fi

echo ""
echo "==> Validation tiers (non-fatal warnings allowed)"
for tier in bootstrap phase1 quality extended ci artwork disc_ingest; do
    if bash "$ROOT_DIR/scripts/validate_compendium_tier.sh" "$tier" "$DB_PATH" >/tmp/remus_audit_${tier}.log 2>&1; then
        echo "  $tier: PASS"
    else
        echo "  $tier: FAIL (see /tmp/remus_audit_${tier}.log)"
    fi
done

echo ""
echo "==> Audit complete"
