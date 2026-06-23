#!/usr/bin/env bash
# Apply incremental compendium migrations (0008+) idempotently on an existing DB.
# Fresh bootstrap via setup_compendium_db.sh already includes the full stack.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

migration_applied() {
    local check_sql="$1"
    sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; $check_sql" | grep -q '^1$'
}

apply_sql() {
    local sql_file="$1"
    echo "==> Applying $(basename "$sql_file")"
    sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000;"
    sqlite3 -batch "$DB_PATH" <"$sql_file"
}

MIGRATION_0008="$ROOT_DIR/data/compendium/migrations/0008_game_facts_lookup_index.sql"
MIGRATION_0009="$ROOT_DIR/data/compendium/migrations/0009_game_signatures_source_entry_key.sql"
MIGRATION_0010="$ROOT_DIR/data/compendium/migrations/0010_game_extended_metadata.sql"
MIGRATION_0011="$ROOT_DIR/data/compendium/migrations/0011_materialized_coverage.sql"
MIGRATION_0012="$ROOT_DIR/data/compendium/migrations/0012_game_assets.sql"

for sql_file in "$MIGRATION_0008" "$MIGRATION_0009" "$MIGRATION_0010" "$MIGRATION_0011" "$MIGRATION_0012"; do
    if [[ ! -f "$sql_file" ]]; then
        echo "error: missing migration: $sql_file" >&2
        exit 1
    fi
done

if ! migration_applied "SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_game_facts_game_field_source' LIMIT 1;"; then
    apply_sql "$MIGRATION_0008"
else
    echo "==> Skipping $(basename "$MIGRATION_0008") (already applied)"
fi

if ! migration_applied "SELECT 1 FROM sqlite_master WHERE type='index' AND name='idx_game_signatures_source_entry_key' LIMIT 1;"; then
    apply_sql "$MIGRATION_0009"
else
    echo "==> Skipping $(basename "$MIGRATION_0009") (already applied)"
fi

if ! migration_applied "SELECT 1 FROM pragma_table_info('games') WHERE name='cover_url' LIMIT 1;"; then
    apply_sql "$MIGRATION_0010"
else
    echo "==> Skipping $(basename "$MIGRATION_0010") (already applied)"
fi

if ! migration_applied "SELECT 1 FROM sqlite_master WHERE type='table' AND name='compendium_coverage_stats' LIMIT 1;"; then
    apply_sql "$MIGRATION_0011"
else
    echo "==> Skipping $(basename "$MIGRATION_0011") (already applied)"
fi

if ! migration_applied "SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_assets' LIMIT 1;"; then
    apply_sql "$MIGRATION_0012"
else
    echo "==> Skipping $(basename "$MIGRATION_0012") (already applied)"
fi

echo "==> Migrations applied: $DB_PATH"
