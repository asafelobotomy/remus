#!/usr/bin/env bash
# One-time backfill of disc set topology for an existing compendium database.
# Applies migration 0007 when missing, rebuilds game_disc_sets/tracks from source_items,
# then runs schema + ingest validation gates.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"
FORCE=0

if [[ "${2:-}" == "--force" ]]; then
    FORCE=1
fi

CLI="$ROOT_DIR/build/remus-cli"
MIGRATION_0007="$ROOT_DIR/data/compendium/migrations/0007_disc_sets.sql"
VALIDATE="$ROOT_DIR/.github/scripts/validate-compendium-db.sh"
SCHEMA_CHECKS="$ROOT_DIR/data/compendium/validation/0004_disc_set_checks.sql"
INGEST_CHECKS="$ROOT_DIR/data/compendium/validation/0005_disc_set_ingest_checks.sql"

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: compendium database not found: $DB_PATH" >&2
    exit 1
fi

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

if [[ ! -f "$MIGRATION_0007" ]]; then
    echo "error: missing migration: $MIGRATION_0007" >&2
    exit 1
fi

has_disc_sets_table="$(sqlite3 "$DB_PATH" "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='game_disc_sets';")"
if [[ "$has_disc_sets_table" != "1" ]]; then
    echo "==> Applying migration 0007 (disc sets)"
    sqlite3 "$DB_PATH" < "$MIGRATION_0007"
fi

if [[ ! -x "$CLI" ]]; then
    echo "error: remus-cli not found at $CLI (build the project first)" >&2
    exit 1
fi

echo "==> Backfilling disc topology: $DB_PATH"
BACKFILL_ARGS=(--backfill-disc-sets --compendium-output "$DB_PATH")
if (( FORCE == 1 )); then
    BACKFILL_ARGS+=(--force-disc-set-backfill)
fi
"$CLI" "${BACKFILL_ARGS[@]}"

echo "==> Disc set schema validation"
bash "$VALIDATE" "$DB_PATH" "$SCHEMA_CHECKS"

if [[ -f "$INGEST_CHECKS" ]]; then
    echo "==> Disc set ingest validation (WARN checks allowed)"
    bash "$VALIDATE" "$DB_PATH" "$INGEST_CHECKS" --warn-only
fi

echo "==> Disc set backfill complete"
