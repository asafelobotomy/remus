#!/usr/bin/env bash
# Create zstd-compressed transport archives of remus-thumbnails blobs per system.
#
# Usage:
#   scripts/archive_remus_thumbnails.sh [--output-dir PATH] [--system NAME]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="$ROOT_DIR/data/compendium/remus_compendium.db"
THUMBNAIL_ROOT="$ROOT_DIR/data/remus-thumbnails"
OUTPUT_DIR="$ROOT_DIR/data/remus-thumbnails/archives"
SYSTEM_FILTER=""

usage() {
    cat <<'USAGE'
Usage:
  scripts/archive_remus_thumbnails.sh [options]

Options:
  --compendium-db <path>   Compendium DB (default: data/compendium/remus_compendium.db)
  --thumbnail-dir <path>   Thumbnail root (default: data/remus-thumbnails)
  --output-dir <path>      Archive output directory
  --system <libretro_name> Only archive blobs for one system
  --help                   Show this help
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
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --system)
            SYSTEM_FILTER="$2"
            shift 2
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
if ! command -v zstd >/dev/null 2>&1; then
    echo "error: zstd is required" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

where_clause="1=1"
if [[ -n "$SYSTEM_FILTER" ]]; then
    esc="${SYSTEM_FILTER//\'/\'\'}"
    where_clause="s.libretro_name = '$esc'"
fi

query="SELECT DISTINCT ga.storage_path, s.libretro_name
FROM game_assets ga
JOIN games g ON g.game_id = ga.game_id
JOIN systems s ON s.system_id = g.system_id
WHERE $where_clause
ORDER BY s.libretro_name, ga.storage_path;"

current_system=""
list_file=""
archive_count=0

finish_archive() {
    if [[ -z "$current_system" || -z "$list_file" || ! -s "$list_file" ]]; then
        return 0
    fi
    safe_name="${current_system//\//-}"
    safe_name="${safe_name// /_}"
    out="$OUTPUT_DIR/${safe_name}.tar.zst"
    echo "==> Archiving $current_system -> $(basename "$out")"
    tar -c -T "$list_file" -C "$ROOT_DIR" 2>/dev/null | zstd -19 -o "$out"
    archive_count=$((archive_count + 1))
    rm -f "$list_file"
}

while IFS='|' read -r storage_path libretro_name; do
    [[ -n "$storage_path" ]] || continue
    if [[ "$libretro_name" != "$current_system" ]]; then
        finish_archive
        current_system="$libretro_name"
        list_file="$(mktemp)"
    fi
    echo "$storage_path" >>"$list_file"
done < <(sqlite3 -batch -separator '|' "$DB_PATH" "$query")

finish_archive

echo "==> Created $archive_count archive(s) in $OUTPUT_DIR"
