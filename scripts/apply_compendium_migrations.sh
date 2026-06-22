#!/usr/bin/env bash
# Apply incremental compendium migrations (0008+) idempotently on an existing DB.
# Fresh bootstrap via setup_compendium_db.sh already includes the full stack.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"

MIGRATIONS=(
    "$ROOT_DIR/data/compendium/migrations/0008_game_facts_lookup_index.sql"
    "$ROOT_DIR/data/compendium/migrations/0009_game_signatures_source_entry_key.sql"
    "$ROOT_DIR/data/compendium/migrations/0010_game_extended_metadata.sql"
)

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

for sql_file in "${MIGRATIONS[@]}"; do
    if [[ ! -f "$sql_file" ]]; then
        echo "error: missing migration: $sql_file" >&2
        exit 1
    fi
    echo "==> Applying $(basename "$sql_file")"
    sqlite3 -batch "$DB_PATH" <<SQL
PRAGMA busy_timeout=60000;
SQL
    sqlite3 -batch "$DB_PATH" <"$sql_file"
done

echo "==> Migrations applied: $DB_PATH"
