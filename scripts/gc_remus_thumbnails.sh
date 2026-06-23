#!/usr/bin/env bash
# Delete unreferenced blobs from data/remus-thumbnails/blobs/ using blob_inventory.
#
# Usage:
#   scripts/gc_remus_thumbnails.sh [--dry-run] [--compendium-db PATH]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="$ROOT_DIR/data/compendium/remus_compendium.db"
THUMBNAIL_ROOT="$ROOT_DIR/data/remus-thumbnails"
DRY_RUN=false

usage() {
    cat <<'USAGE'
Usage:
  scripts/gc_remus_thumbnails.sh [options]

Options:
  --compendium-db <path>  Compendium SQLite path (default: data/compendium/remus_compendium.db)
  --thumbnail-dir <path>  Remus thumbnails root (default: data/remus-thumbnails)
  --dry-run               List orphans without deleting
  -h, --help              Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compendium-db)
            DB_PATH="$2"
            shift 2
            ;;
        --thumbnail-dir)
            THUMBNAIL_ROOT="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required" >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

if ! sqlite3 -batch "$DB_PATH" "SELECT 1 FROM sqlite_master WHERE type='table' AND name='blob_inventory' LIMIT 1;" | grep -q '^1$'; then
    echo "error: blob_inventory table missing — apply migration 0012" >&2
    exit 1
fi

echo "==> Thumbnail blob GC"
echo "    db=$DB_PATH"
echo "    thumbnail_root=$THUMBNAIL_ROOT"

sqlite3 -batch "$DB_PATH" "PRAGMA busy_timeout=60000; PRAGMA wal_checkpoint(TRUNCATE);" >/dev/null 2>&1 || true

orphan_sql="SELECT content_sha256, storage_path FROM blob_inventory
WHERE content_sha256 NOT IN (SELECT DISTINCT content_sha256 FROM game_assets);"

deleted=0
freed=0
orphan_shas=()

while IFS='|' read -r sha path; do
    [[ -n "$sha" ]] || continue
    abs_path="$ROOT_DIR/$path"
    if [[ ! -f "$abs_path" ]]; then
        for ext in webp png; do
            candidate="$THUMBNAIL_ROOT/blobs/${sha:0:2}/${sha:2:2}/$sha.$ext"
            if [[ -f "$candidate" ]]; then
                abs_path="$candidate"
                break
            fi
        done
    fi
    if [[ -f "$abs_path" ]]; then
        size=$(stat -c%s "$abs_path" 2>/dev/null || echo 0)
        if $DRY_RUN; then
            echo "  would delete: $abs_path ($size bytes)"
        else
            rm -f -- "$abs_path"
            echo "  deleted: $abs_path"
        fi
        freed=$((freed + size))
    fi
    orphan_shas+=("$sha")
    deleted=$((deleted + 1))
done < <(sqlite3 -batch -separator '|' "$DB_PATH" "PRAGMA busy_timeout=60000; $orphan_sql")

if ! $DRY_RUN && [[ ${#orphan_shas[@]} -gt 0 ]]; then
    {
        echo "PRAGMA busy_timeout=60000;"
        echo "BEGIN;"
        for sha in "${orphan_shas[@]}"; do
            printf "DELETE FROM blob_inventory WHERE content_sha256 = '%s';\n" "${sha//\'/\'\'}"
        done
        echo "COMMIT;"
    } | sqlite3 -batch "$DB_PATH"
fi

echo "==> GC complete: orphans=$deleted freed_bytes=$freed dry_run=$DRY_RUN"
