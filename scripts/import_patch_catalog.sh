#!/usr/bin/env bash
# Import community patch / translation DAT files into the compendium patch catalog.
# Patch DATs are not bundled in libretro-database; place them under data/patches/.
#
# Sources for patch DATs:
#   - https://datomatic.no-intro.org/ (Non-Redump / Hacks sections)
#   - Community translation sets (e.g. ROMhacking.net project releases)
#   - System-specific patch catalogues referenced in docs/reports/COMPENDIUM-DATA-SOURCES.md
#
# Usage:
#   scripts/import_patch_catalog.sh [compendium.db]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"
PATCH_DIR="$ROOT_DIR/data/patches"

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: compendium database not found: $DB_PATH" >&2
    exit 1
fi

if [[ ! -d "$PATCH_DIR" ]]; then
    echo "==> No patch directory ($PATCH_DIR) — creating placeholder"
    mkdir -p "$PATCH_DIR"
    echo "    Place *.dat patch catalogues here and re-run this script."
    exit 0
fi

mapfile -d '' PATCH_FILES < <(find "$PATCH_DIR" -maxdepth 2 -type f -name '*.dat' -print0 | sort -z)
if ((${#PATCH_FILES[@]} == 0)); then
    echo "==> No patch DAT files found under $PATCH_DIR"
    echo "    See docs/reports/COMPENDIUM-DATA-SOURCES.md for recommended sources."
    exit 0
fi

echo "==> Patch catalog import: ${#PATCH_FILES[@]} file(s) → $DB_PATH"
echo "    Note: full DAT parsing requires remus-cli --import-patch-catalog (planned)."
echo "    For now, register sources in patch_catalog_sources for visibility:"

for dat_file in "${PATCH_FILES[@]}"; do
    system_name="$(basename "$dat_file" .dat)"
    catalog_name="$(basename "$(dirname "$dat_file")")"
    [[ "$catalog_name" == "patches" ]] && catalog_name="default"
    sqlite3 "$DB_PATH" "
        INSERT OR IGNORE INTO patch_catalog_sources
            (system_name, catalog_name, catalog_version, catalog_source, catalog_description, entry_count)
        VALUES (
            '$(printf '%s' "$system_name" | sed "s/'/''/g")',
            '$(printf '%s' "$catalog_name" | sed "s/'/''/g")',
            NULL,
            'community',
            'Imported from data/patches — entries pending parser wiring',
            0
        );
    "
    echo "    registered: $system_name ($dat_file)"
done

echo "==> Done. Run validation: bash .github/scripts/validate-compendium-db.sh $DB_PATH data/compendium/validation/0002_phase2_quality_checks.sql"
