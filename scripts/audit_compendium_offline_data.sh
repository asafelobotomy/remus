#!/usr/bin/env bash
# Quick audit of offline compendium enrichment inputs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ok=0
warn=0
fail=0

check() {
    local label="$1" path="$2" kind="$3"
    case "$kind" in
        dir_dat)
            if [[ -d "$path" ]]; then
                local n
                n=$(find "$path" -maxdepth 3 -name '*.dat' 2>/dev/null | wc -l | tr -d ' ')
                if [[ "$n" -gt 0 ]]; then echo "✓ $label ($n DAT files)"; ok=$((ok+1)); else echo "✗ $label (no .dat files)"; fail=$((fail+1)); fi
            else echo "✗ $label (missing: $path)"; fail=$((fail+1)); fi ;;
        dir_xml)
            if [[ -d "$path" ]]; then
                local n
                n=$(find "$path" -maxdepth 2 -name '*.xml' 2>/dev/null | wc -l | tr -d ' ')
                if [[ "$n" -gt 0 ]]; then echo "✓ $label ($n XML files)"; ok=$((ok+1)); else echo "✗ $label (no .xml files)"; fail=$((fail+1)); fi
            else echo "✗ $label (missing: $path)"; fail=$((fail+1)); fi ;;
        file)
            if [[ -f "$path" ]]; then echo "✓ $label"; ok=$((ok+1)); else echo "✗ $label (missing: $path)"; fail=$((fail+1)); fi ;;
        json_tree)
            if [[ -d "$path" ]]; then
                local n
                n=$(find "$path" -type f -name '*.json' ! -name 'PlatformMapping.json' 2>/dev/null | wc -l | tr -d ' ')
                if [[ "$n" -gt 0 ]]; then echo "✓ $label ($n JSON dumps)"; ok=$((ok+1)); else echo "○ $label (no JSON dumps yet — Hasheous pass skipped offline)"; warn=$((warn+1)); fi
            else echo "○ $label (directory missing)"; warn=$((warn+1)); fi ;;
    esac
}

echo "==> Offline compendium data audit"
echo "    root=$ROOT"
echo ""
check "Libretro DAT catalogues" "$ROOT/data/databases" dir_dat
check "Libretro metadata DATs" "$ROOT/data/metadata" dir_dat
check "GameTDB XML" "$ROOT/data/gametdb" dir_xml
check "OpenVGDB SQLite" "$ROOT/data/openvgdb/openvgdb.sqlite" file
check "MAME catver.ini" "$ROOT/data/mame/catver.ini" file
check "MAME listxml.xml" "$ROOT/data/mame/listxml.xml" file
check "Hasheous offline dumps" "$ROOT/data/hasheous/dumps" json_tree
echo ""
echo "Summary: $ok ready, $warn optional/missing, $fail required gaps"
[[ "$fail" -eq 0 ]]
