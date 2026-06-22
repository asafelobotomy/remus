#!/usr/bin/env bash
set -euo pipefail

# Tool: setup_compendium_db.sh
# Purpose: Build a fresh Phase 1 compendium SQLite database from the full schema stack.
# Usage: ./scripts/setup_compendium_db.sh [db_path]
# Inputs: Optional SQLite file path; default is data/compendium/remus_compendium.db.
# Outputs: A recreated SQLite database and printed sanity checks.
# Safety: Removes the target database file before recreating it.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"

SQL_STEPS=(
    "$ROOT_DIR/data/compendium/migrations/0001_phase1_canonical_schema.sql"
    "$ROOT_DIR/data/compendium/migrations/0002_patch_catalog.sql"
    "$ROOT_DIR/data/compendium/seeds/0001_regions.sql"
    "$ROOT_DIR/data/compendium/seeds/0002_systems.sql"
    "$ROOT_DIR/data/compendium/seeds/0003_merge_policy.sql"
    "$ROOT_DIR/data/compendium/migrations/0003_systems_libretro_name.sql"
    "$ROOT_DIR/data/compendium/migrations/0004_fts5_search_index.sql"
    "$ROOT_DIR/data/compendium/migrations/0005_game_external_ids.sql"
    "$ROOT_DIR/data/compendium/migrations/0006_game_achievement_count.sql"
    "$ROOT_DIR/data/compendium/migrations/0007_disc_sets.sql"
    "$ROOT_DIR/data/compendium/migrations/0008_game_facts_lookup_index.sql"
    "$ROOT_DIR/data/compendium/migrations/0009_game_signatures_source_entry_key.sql"
    "$ROOT_DIR/data/compendium/migrations/0010_game_extended_metadata.sql"
    "$ROOT_DIR/data/compendium/migrations/0011_materialized_coverage.sql"
)
VALIDATION_SQL="$ROOT_DIR/data/compendium/validation/0001_phase1_checks.sql"
DISC_SET_VALIDATION_SQL="$ROOT_DIR/data/compendium/validation/0004_disc_set_checks.sql"

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

for sql_file in "${SQL_STEPS[@]}" "$VALIDATION_SQL" "$DISC_SET_VALIDATION_SQL"; do
    if [[ ! -f "$sql_file" ]]; then
        echo "error: missing SQL file: $sql_file" >&2
        exit 1
    fi
done

mkdir -p "$(dirname "$DB_PATH")"
rm -f "$DB_PATH"

echo "==> Creating compendium database: $DB_PATH"
for sql_file in "${SQL_STEPS[@]}"; do
    sqlite3 "$DB_PATH" < "$sql_file"
done

echo "==> Seed count checks"
echo "systems_count=$(sqlite3 "$DB_PATH" "SELECT COUNT(*) FROM systems;")"
echo "regions_count=$(sqlite3 "$DB_PATH" "SELECT COUNT(*) FROM regions;")"
echo "merge_policy_count=$(sqlite3 "$DB_PATH" "SELECT COUNT(*) FROM merge_policy;")"

echo "==> SQLite integrity check"
sqlite3 "$DB_PATH" "PRAGMA integrity_check;"

echo "==> Disc set schema checks (migration 0007)"
sqlite3 -header -column "$DB_PATH" < "$DISC_SET_VALIDATION_SQL"

echo "==> Fresh bootstrap notes"
echo "Run the full validator after ingest/build populates games, signatures, and source items:"
echo "  sqlite3 -header -column '$DB_PATH' < '$VALIDATION_SQL'"

echo "==> Done"
