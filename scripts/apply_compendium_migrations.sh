#!/usr/bin/env bash
# Apply compendium migrations idempotently using schema_migrations ledger + manifest.json.
# Fresh bootstrap via setup_compendium_db.sh already includes the full stack.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"
MANIFEST="$ROOT_DIR/data/compendium/migrations/manifest.json"

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "error: jq is required for migration manifest parsing" >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

if [[ ! -f "$MANIFEST" ]]; then
    echo "error: missing migration manifest: $MANIFEST" >&2
    exit 1
fi

sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; CREATE TABLE IF NOT EXISTS schema_migrations (
    migration_id TEXT PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT (datetime('now'))
);"

migration_in_ledger() {
    local migration_id="$1"
    sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM schema_migrations WHERE migration_id = '$migration_id' LIMIT 1;" \
        | grep -q '^1$'
}

record_migration() {
    local migration_id="$1"
    sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; INSERT OR IGNORE INTO schema_migrations (migration_id) VALUES ('$migration_id');"
}

migration_already_applied() {
    local migration_id="$1"
    case "$migration_id" in
        0001_phase1_canonical_schema.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='compendium_builds' LIMIT 1;" | grep -q '^1$'
            ;;
        0002_patch_catalog.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='patch_catalog_sources' LIMIT 1;" | grep -q '^1$'
            ;;
        0003_systems_libretro_name.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM pragma_table_info('systems') WHERE name='libretro_name' LIMIT 1;" | grep -q '^1$'
            ;;
        0004_fts5_search_index.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='games_search' LIMIT 1;" | grep -q '^1$'
            ;;
        0005_game_external_ids.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_games_igdb_id' LIMIT 1;" | grep -q '^1$'
            ;;
        0006_game_achievement_count.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM pragma_table_info('games') WHERE name='achievement_count' LIMIT 1;" | grep -q '^1$'
            ;;
        0007_disc_sets.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_disc_sets' LIMIT 1;" | grep -q '^1$'
            ;;
        0008_game_facts_lookup_index.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_game_facts_game_field_source' LIMIT 1;" | grep -q '^1$'
            ;;
        0009_game_signatures_source_entry_key.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_game_signatures_source_entry_key' LIMIT 1;" | grep -q '^1$'
            ;;
        0010_game_extended_metadata.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM pragma_table_info('games') WHERE name='cover_url' LIMIT 1;" | grep -q '^1$'
            ;;
        0011_materialized_coverage.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='compendium_coverage_stats' LIMIT 1;" | grep -q '^1$'
            ;;
        0012_game_assets.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_assets' LIMIT 1;" | grep -q '^1$'
            ;;
        0013_disc_tracks_per_set_unique.sql)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_disc_tracks' AND sql LIKE '%UNIQUE (disc_set_id, source_entry_key)%' LIMIT 1;" | grep -q '^1$'
            ;;
        *)
            sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; SELECT 1 FROM sqlite_master WHERE type='table' AND name='games' LIMIT 1;" | grep -q '^1$'
            ;;
    esac
}

apply_sql() {
    local sql_file="$1"
    echo "==> Applying $(basename "$sql_file")"
    sqlite3 -batch "$DB_PATH" <<SQL
PRAGMA busy_timeout=60000;
BEGIN IMMEDIATE;
.read $sql_file
COMMIT;
SQL
}

while IFS= read -r migration_id; do
    [[ -n "$migration_id" ]] || continue
    sql_file="$ROOT_DIR/data/compendium/migrations/$migration_id"
    if [[ ! -f "$sql_file" ]]; then
        echo "error: missing migration file: $sql_file" >&2
        exit 1
    fi

    if migration_in_ledger "$migration_id"; then
        echo "==> Skipping $migration_id (ledger)"
        continue
    fi

    if migration_already_applied "$migration_id"; then
        echo "==> Recording $migration_id (detected pre-ledger)"
        record_migration "$migration_id"
        continue
    fi

    apply_sql "$sql_file"
    record_migration "$migration_id"
done < <(jq -r '.migrations[]' "$MANIFEST")

echo "==> Migrations applied: $DB_PATH"
