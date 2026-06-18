#!/usr/bin/env bash
# test_enricher.sh — Build a source-specific compendium DB and validate enrichment output.
#
# Usage:
#   scripts/test_enricher.sh <source> [OPTIONS]
#
# Arguments:
#   <source>           Enricher key(s), comma-separated.
#                      Valid: libretro, gametdb, openvgdb, hasheous, playmatch,
#                             igdb, ra, mame-catver, mame-listxml, zxinfo
#
# Options:
#   --manifest <path>  Path to manifest JSON (default: data/compendium/compendium-manifest.json)
#   --output <path>    Explicit output DB path (default: auto-derived from source name)
#   --no-rebuild       Skip the build step; run coverage/stats against an existing output DB
#   -h, --help         Show this help
#
# Examples:
#   scripts/test_enricher.sh gametdb
#   scripts/test_enricher.sh mame-listxml --manifest data/compendium/compendium-manifest.json
#   scripts/test_enricher.sh igdb,gametdb
#   scripts/test_enricher.sh zxinfo --no-rebuild

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARY="${REPO_ROOT}/build/remus-cli"
DEFAULT_MANIFEST="${REPO_ROOT}/data/compendium/compendium-manifest.json"

# ── Argument parsing ──────────────────────────────────────────────────────────
SOURCE=""
MANIFEST="${DEFAULT_MANIFEST}"
OUTPUT=""
NO_REBUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --manifest|-m)  MANIFEST="$2"; shift 2 ;;
        --output|-o)    OUTPUT="$2";   shift 2 ;;
        --no-rebuild)   NO_REBUILD=1;  shift   ;;
        --help|-h)
            sed -n '/^# /p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        -*)
            echo "Unknown option: $1" >&2; exit 1 ;;
        *)
            SOURCE="$1"; shift ;;
    esac
done

if [[ -z "${SOURCE}" ]]; then
    echo "Error: enricher source name required." >&2
    echo "Usage: $0 <source> [--manifest <path>] [--output <path>] [--no-rebuild]" >&2
    echo "Valid sources: libretro, gametdb, openvgdb, hasheous, playmatch, igdb, ra, mame-catver, mame-listxml, zxinfo" >&2
    exit 1
fi

# ── Derive output path ────────────────────────────────────────────────────────
if [[ -z "${OUTPUT}" ]]; then
    SOURCE_SUFFIX="${SOURCE^^}"                  # uppercase
    SOURCE_SUFFIX="${SOURCE_SUFFIX//,/_}"        # commas → underscores
    SOURCE_SUFFIX="${SOURCE_SUFFIX//-/_}"        # hyphens → underscores
    OUTPUT="${REPO_ROOT}/data/compendium/remus_compendium_${SOURCE_SUFFIX}.db"
fi

echo "==> Enricher test: ${SOURCE}"
echo "    Manifest : ${MANIFEST}"
echo "    Output   : ${OUTPUT}"
echo ""

# ── Pre-flight checks ─────────────────────────────────────────────────────────
if [[ ! -f "${BINARY}" ]]; then
    echo "✗ Binary not found at ${BINARY}" >&2
    echo "  Run: cmake --build build -- -j\$(nproc)" >&2
    exit 1
fi

if [[ ${NO_REBUILD} -eq 0 && ! -f "${MANIFEST}" ]]; then
    echo "✗ Manifest not found: ${MANIFEST}" >&2
    exit 1
fi

# ── Build ─────────────────────────────────────────────────────────────────────
if [[ ${NO_REBUILD} -eq 0 ]]; then
    echo "==> Building compendium (--enrich-source ${SOURCE}) …"
    rm -f "${OUTPUT}" \
          "${OUTPUT%.db}_staged.db" \
          "${OUTPUT%.db}.report.json" \
          "${OUTPUT%.db}_staged.report.json"

    "${BINARY}" \
        --build-compendium \
        --compendium-manifest "${MANIFEST}" \
        --enrich-source "${SOURCE}" \
        --compendium-output "${OUTPUT}"

    echo "==> Build complete."
    echo ""
else
    if [[ ! -f "${OUTPUT}" ]]; then
        echo "✗ --no-rebuild set but output DB not found: ${OUTPUT}" >&2
        exit 1
    fi
    echo "==> Skipping build (--no-rebuild); using ${OUTPUT}"
    echo ""
fi

# ── Summary query ─────────────────────────────────────────────────────────────
if command -v sqlite3 &>/dev/null; then
    echo "==> Enrichment summary:"
    sqlite3 "${OUTPUT}" "
SELECT
  COUNT(*)                                                                        AS total_games,
  SUM(CASE WHEN description  IS NOT NULL AND TRIM(description)  != '' THEN 1 ELSE 0 END) AS has_description,
  SUM(CASE WHEN developer    IS NOT NULL AND TRIM(developer)    != '' THEN 1 ELSE 0 END) AS has_developer,
  SUM(CASE WHEN publisher    IS NOT NULL AND TRIM(publisher)    != '' THEN 1 ELSE 0 END) AS has_publisher,
  SUM(CASE WHEN genre        IS NOT NULL AND TRIM(genre)        != '' THEN 1 ELSE 0 END) AS has_genre,
  SUM(CASE WHEN release_year IS NOT NULL                              THEN 1 ELSE 0 END) AS has_release_year,
  SUM(CASE WHEN release_date IS NOT NULL AND TRIM(release_date) != '' THEN 1 ELSE 0 END) AS has_release_date,
  SUM(CASE WHEN players_max  IS NOT NULL                              THEN 1 ELSE 0 END) AS has_players,
  SUM(CASE WHEN rating       IS NOT NULL                              THEN 1 ELSE 0 END) AS has_rating
FROM games;" 2>&1
    echo ""

    echo "==> Facts by source:"
    sqlite3 "${OUTPUT}" "
SELECT source_id, field_name, COUNT(*) AS cnt
FROM game_facts
GROUP BY source_id, field_name
ORDER BY source_id, field_name;" 2>&1
    echo ""
else
    echo "(sqlite3 not found — skipping summary query)"
fi

# ── Coverage report ───────────────────────────────────────────────────────────
echo "==> Coverage (top 5 lowest):"
"${BINARY}" --coverage-report --compendium-output "${OUTPUT}" 2>/dev/null \
    | grep -v '^#' | awk -F'\t' 'NR>1 && $5+0 < 100 {print}' \
    | sort -t$'\t' -k5 -n \
    | head -5 \
    || true

echo ""
echo "==> Done: $(basename "${OUTPUT}")"
