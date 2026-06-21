#!/usr/bin/env bash
set -euo pipefail
# update_mame_listxml.sh — Generate data/mame/listxml.xml for the MAME enricher
#
# Usage:
#   scripts/update_mame_listxml.sh
#
# Produces data/mame/listxml.xml — the raw MAME machine-description XML consumed
# by the compendium_enrichment_mame_listxml enricher during --build-compendium.
#
# Resolution order:
#   1. Locally installed mame / mame64 / mame-arcade binary (fastest; no download).
#      Install via "pacman -S mame", "apt install mame", or "brew install mame".
#   2. Download the latest mamedev/mame Linux release ZIP from GitHub, extract
#      the bundled XML (Strategy A) or the binary to run -listxml (Strategy B).
#
# Requires: bash ≥ 4, curl, python3 (for Strategy 2 ZIP asset URL resolution)
# Optional: unzip (for Strategy 2)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=gh_git_env.sh
source "${SCRIPT_DIR}/gh_git_env.sh"
MAME_DATA_DIR="$PROJECT_ROOT/data/mame"
OUT_FILE="$MAME_DATA_DIR/listxml.xml"
CACHE_DIR="${XDG_CACHE_HOME:-$PROJECT_ROOT/.cache}/remus/mame"

mame_bin_tmp=""
trap 'rm -rf "${mame_bin_tmp:-}"' EXIT

mkdir -p "$MAME_DATA_DIR"

# ── Step 1: look for a locally installed MAME binary ─────────────────────────
mame_bin=""
for candidate in mame mame64 mame-arcade; do
    if command -v "$candidate" &>/dev/null; then
        mame_bin="$(command -v "$candidate")"
        echo "Found local MAME binary: $mame_bin"
        break
    fi
done

# ── Step 2: no local binary — try GitHub release ZIP ─────────────────────────
if [[ -z "$mame_bin" ]]; then
    echo "No local MAME binary found — fetching from mamedev/mame GitHub releases..."

    mame_api_headers=( -H "Accept: application/vnd.github+json" )
    if [[ -n "${GITHUB_TOKEN:-}" ]]; then
        mame_api_headers+=( -H "Authorization: Bearer ${GITHUB_TOKEN}" )
    fi

    mame_lx_url=$(curl -fsSL --max-time 30 \
        "${mame_api_headers[@]}" \
        "https://api.github.com/repos/mamedev/mame/releases/latest" 2>/dev/null \
        | python3 -c "
import sys, json
d = json.load(sys.stdin)
url = next((a['browser_download_url'] for a in d.get('assets', [])
            if a['name'].endswith('lx.zip')), '')
print(url)
" 2>/dev/null || true)

    if [[ -n "$mame_lx_url" ]]; then
        mkdir -p "$CACHE_DIR"
        mame_cache_zip="$CACHE_DIR/$(basename "$mame_lx_url")"

        if [[ ! -f "$mame_cache_zip" ]]; then
            echo "Downloading $(basename "$mame_lx_url")..."
            curl -fL --max-time 600 --progress-bar \
                -o "$mame_cache_zip" "$mame_lx_url" \
                || { echo "Error: download failed"; exit 1; }
        else
            echo "Using cached $(basename "$mame_cache_zip")"
        fi

        mame_bin_tmp="$(mktemp -d)"

        # Strategy A: prefer a pre-built XML bundled in the release ZIP
        mame_xml_inner=$(unzip -l "$mame_cache_zip" 2>/dev/null \
            | awk '/[[:space:]]mame[^\/]*\.xml$/ { print $NF; exit }')
        if [[ -n "$mame_xml_inner" ]] \
           && unzip -o -q "$mame_cache_zip" "$mame_xml_inner" -d "$mame_bin_tmp" 2>/dev/null; then
            echo "Using bundled XML from release: $mame_xml_inner"
            cp "$mame_bin_tmp/$mame_xml_inner" "$OUT_FILE"
            echo "Written: $OUT_FILE"
            exit 0
        fi

        # Strategy B: extract the binary and run -listxml
        mame_inner=$(unzip -l "$mame_cache_zip" 2>/dev/null \
            | awk '!/\.xml$/ && /[[:space:]]mame[^\/]*$/ { print $NF; exit }')
        if [[ -n "$mame_inner" ]] \
           && unzip -o -q "$mame_cache_zip" "$mame_inner" -d "$mame_bin_tmp" 2>/dev/null; then
            chmod +x "$mame_bin_tmp/$mame_inner"
            mame_bin="$mame_bin_tmp/$mame_inner"
            echo "Extracted MAME binary: $mame_inner"
        else
            echo "Error: could not extract MAME binary or XML from release ZIP." >&2
            echo "  Install MAME locally (e.g. 'apt install mame') and re-run." >&2
            exit 1
        fi
    else
        echo "Error: could not resolve mamedev/mame release URL via GitHub API." >&2
        echo "  Install MAME locally (e.g. 'apt install mame' / 'pacman -S mame')" >&2
        echo "  or run 'gh auth login' (or set GITHUB_TOKEN) to avoid rate-limiting and re-run." >&2
        exit 1
    fi
fi

# ── Run -listxml using the resolved binary ────────────────────────────────────
echo "Running: $(basename "$mame_bin") -listxml  (may take ~30 s)..."
if ! "$mame_bin" -listxml > "$OUT_FILE" 2>/dev/null || [[ ! -s "$OUT_FILE" ]]; then
    rm -f "$OUT_FILE"
    echo "Error: mame -listxml produced no output." >&2
    exit 1
fi

echo "Written: $OUT_FILE ($(du -sh "$OUT_FILE" | cut -f1))"
