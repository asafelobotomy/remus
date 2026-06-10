#!/usr/bin/env bash
# Import community patch / translation DAT files into the compendium patch catalog.
#
# Primary source: libretro-database metadat/hacks/ (synced by scripts/update_dats.sh
# into data/patches/hacks/). Additional *.dat files may be placed under data/patches/.
#
# Usage:
#   scripts/import_patch_catalog.sh [compendium.db] [--patch-dir <path>]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="$ROOT_DIR/data/compendium/remus_compendium.db"
PATCH_DIR="$ROOT_DIR/data/patches"
CLI="$ROOT_DIR/build/remus-cli"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --patch-dir)
            PATCH_DIR="${2:-}"
            shift 2
            ;;
        --help|-h)
            echo "Usage: scripts/import_patch_catalog.sh [compendium.db] [--patch-dir <path>]"
            exit 0
            ;;
        *)
            DB_PATH="$1"
            shift
            ;;
    esac
done

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: compendium database not found: $DB_PATH" >&2
    exit 1
fi

if [[ ! -x "$CLI" ]]; then
    echo "error: remus-cli not found at $CLI — build the project first" >&2
    exit 1
fi

if [[ ! -d "$PATCH_DIR" ]]; then
    echo "==> No patch directory ($PATCH_DIR) — creating placeholder"
    mkdir -p "$PATCH_DIR/hacks"
    echo "    Run scripts/update_dats.sh to sync libretro metadat/hacks/, or place *.dat files here."
    exit 0
fi

dat_count=$(find "$PATCH_DIR" -maxdepth 2 -type f -name '*.dat' | wc -l)
if [[ "$dat_count" -eq 0 ]]; then
    echo "==> No patch DAT files found under $PATCH_DIR"
    echo "    Run: bash scripts/update_dats.sh"
    echo "    See docs/reports/COMPENDIUM-DATA-SOURCES.md for additional sources."
    exit 0
fi

echo "==> Patch catalog import: $dat_count file(s) from $PATCH_DIR → $DB_PATH"
"$CLI" --import-patch-catalog --compendium-output "$DB_PATH" --patch-dir "$PATCH_DIR"

echo "==> Done. Validate with:"
echo "    bash .github/scripts/validate-compendium-db.sh $DB_PATH data/compendium/validation/0002_phase2_quality_checks.sql"
