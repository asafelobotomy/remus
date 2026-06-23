#!/usr/bin/env bash
# Retry artwork consolidate on an already-populated compendium DB (no DAT re-ingest).
# Usage: scripts/resume_compendium_artwork.sh [--restore-latest-backup] [remus-cli consolidate flags...]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/ensure_npm_build_tools.sh"
ensure_npm_build_tools
# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/compendium_db_guard.sh"

OUTPUT_DB="$ROOT_DIR/data/compendium/remus_compendium.db"
RESTORE_BACKUP=false
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-db)
            OUTPUT_DB="$2"
            shift 2
            ;;
        --restore-latest-backup)
            RESTORE_BACKUP=true
            shift
            ;;
        -h|--help)
            cat <<'USAGE'
Usage: scripts/resume_compendium_artwork.sh [options] [-- remus-cli flags]

Options:
  --output-db <path>         Compendium DB (default: data/compendium/remus_compendium.db)
  --restore-latest-backup    Restore newest data/compendium/backups/*.post-ingest.*.db first
  -h, --help                 Show this help

Examples:
  scripts/resume_compendium_artwork.sh --thumbnail-snap-lossless
  scripts/resume_compendium_artwork.sh --restore-latest-backup --thumbnail-snap-lossless
USAGE
            exit 0
            ;;
        --)
            shift
            EXTRA_ARGS+=("$@")
            break
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ! -x "$ROOT_DIR/build/remus-cli" ]]; then
    echo "error: missing executable: $ROOT_DIR/build/remus-cli" >&2
    exit 1
fi

if $RESTORE_BACKUP; then
    compendium_restore_post_ingest_backup "$OUTPUT_DB"
fi

if ! compendium_db_is_populated "$OUTPUT_DB"; then
    count="$(compendium_db_game_count "$OUTPUT_DB")"
    echo "error: $OUTPUT_DB is not populated ($count games)" >&2
    echo "hint: run scripts/build_compendium_full.sh for a full ingest, or pass --restore-latest-backup" >&2
    exit 1
fi

consolidate_args=(
    --consolidate-thumbnails
    --compendium-output "$OUTPUT_DB"
)
if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
    consolidate_args+=("${EXTRA_ARGS[@]}")
fi

echo "==> Resuming artwork consolidate on populated compendium"
echo "    db=$OUTPUT_DB"
"$ROOT_DIR/scripts/run_compendium_job.sh" --db "$OUTPUT_DB" -- \
    "$ROOT_DIR/build/remus-cli" "${consolidate_args[@]}"
