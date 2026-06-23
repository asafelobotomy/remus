#!/usr/bin/env bash
# Download Hasheous per-platform offline dump ZIPs from the public API and extract
# JSON game records under data/hasheous/dumps/<platform>/.
#
# API: GET https://hasheous.org/api/v1/Dumps/platforms
#      GET https://hasheous.org/api/v1/Dumps/platforms/{filename}.zip
#
# Compendium builds use these files for offline Hasheous enrichment (igdb_id bridge,
# description, genre) without per-game HTTP calls.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=compendium_offline_helpers.sh
source "${SCRIPT_DIR}/compendium_offline_helpers.sh"
DUMP_ROOT="$ROOT_DIR/data/hasheous/dumps"
CACHE_DIR="${XDG_CACHE_HOME:-$ROOT_DIR/.cache}/remus/hasheous_dumps"
API_BASE="https://hasheous.org/api/v1/Dumps/platforms"
CACHE_TTL_SECONDS="${HASHEOUS_DUMP_CACHE_TTL_SECONDS:-604800}"

DOWNLOAD_ALL=false
DOWNLOAD_CORE=false
PLATFORM_FILTER=()

# Core platforms aligned with scripts/update_dats.sh CORE_SYSTEMS (Hasheous zip filenames).
CORE_PLATFORM_ZIPS=(
    "Atari 2600.zip"
    "Bandai WonderSwan Color.zip"
    "Bandai WonderSwan.zip"
    "ColecoVision.zip"
    "Commodore 64.zip"
    "Commodore Amiga.zip"
    "MSX.zip"
    "NEC PC-Engine CD & TurboGrafx-16 CD.zip"
    "Nintendo 3DS.zip"
    "Nintendo 64.zip"
    "Nintendo DS.zip"
    "Nintendo Entertainment System.zip"
    "Nintendo Game Boy Advance.zip"
    "Nintendo Game Boy Color.zip"
    "Nintendo Game Boy.zip"
    "Nintendo GameCube.zip"
    "Nintendo Super Nintendo Entertainment System.zip"
    "Nintendo Wii.zip"
    "Sega Dreamcast.zip"
    "Sega Master System.zip"
    "Sega Mega Drive _ Genesis.zip"
    "Sony PlayStation 2.zip"
    "Sony PlayStation.zip"
    "Sony Playstation Portable.zip"
    "TurboGrafx-16_PC Engine.zip"
)

usage() {
    cat <<'USAGE'
Usage:
  scripts/update_hasheous_dumps.sh [options]

Options:
  --all-core        Download curated core platform dumps (~1–2 GB compressed)
  --all             Download every platform dump listed by the API (large)
  --platform NAME   Download one platform ZIP (repeatable; NAME is the API filename,
                    e.g. "Nintendo DS.zip")
  --output-dir <p>  Dump extract root (default: data/hasheous/dumps)
  -h, --help        Show this help

Extracted JSON files land in: <output-dir>/<platform-stem>/*.json
Offline Hasheous enrichment runs when at least one *.json is present.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all-core)
            DOWNLOAD_CORE=true
            shift
            ;;
        --all)
            DOWNLOAD_ALL=true
            shift
            ;;
        --platform)
            PLATFORM_FILTER+=("$2")
            shift 2
            ;;
        --output-dir)
            DUMP_ROOT="$2"
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

if ! $DOWNLOAD_ALL && ! $DOWNLOAD_CORE && [[ ${#PLATFORM_FILTER[@]} -eq 0 ]]; then
    DOWNLOAD_CORE=true
fi

mkdir -p "$DUMP_ROOT" "$CACHE_DIR"

is_cache_fresh() {
    local file_path="$1"
    [[ -f "$file_path" ]] || return 1
    local file_mtime now age
    file_mtime=$(stat -c %Y "$file_path" 2>/dev/null || return 1)
    now=$(date +%s)
    age=$((now - file_mtime))
    [[ "$age" -lt "$CACHE_TTL_SECONDS" ]]
}

download_platform_zip() {
    local zip_name="$1"
    local url
    url="${API_BASE}/$(python3 -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$zip_name")"
    local cache_zip="$CACHE_DIR/$zip_name"
    local platform_dir="$DUMP_ROOT/${zip_name%.zip}"
    local zip_downloaded=false

    if is_cache_fresh "$cache_zip"; then
        echo "  Using cached $zip_name"
    else
        echo "  Downloading $zip_name..."
        mkdir -p "$(dirname "$cache_zip")"
        local tmp_zip
        tmp_zip="$(mktemp "${cache_zip}.XXXXXX")"
        if ! curl -fsSL --max-time 3600 --retry 3 --retry-delay 5 \
            -o "$tmp_zip" "$url"; then
            rm -f "$tmp_zip"
            echo "  Warning: failed to download $zip_name" >&2
            return 1
        fi
        mv "$tmp_zip" "$cache_zip"
        zip_downloaded=true
    fi

    local marker="$platform_dir/.hasheous_zip_sha256"
    local zip_hash json_count
    zip_hash="$(compendium_sha256_of "$cache_zip")"
    json_count="$(find "$platform_dir" -maxdepth 1 -type f -name '*.json' 2>/dev/null | wc -l | tr -d ' ')"

    if ! $zip_downloaded \
       && [[ -f "$marker" ]] \
       && [[ "$(cat "$marker")" == "$zip_hash" ]] \
       && [[ "$json_count" -gt 0 ]]; then
        echo "  Unchanged $zip_name ($json_count JSON files)"
        return 2
    fi

    mkdir -p "$platform_dir"
    local json_before json_after
    json_before="$(find "$platform_dir" -maxdepth 1 -type f -name '*.json' 2>/dev/null | wc -l | tr -d ' ')"
    if ! unzip -o -q "$cache_zip" '*.json' -d "$platform_dir" 2>/dev/null; then
        echo "  Warning: failed to extract JSON from $zip_name" >&2
        return 1
    fi
    json_after="$(find "$platform_dir" -maxdepth 1 -type f -name '*.json' 2>/dev/null | wc -l | tr -d ' ')"
    printf '%s\n' "$zip_hash" > "$marker"
    HASHEOUS_ANY_CHANGED=true
    echo "  Extracted $json_after JSON file(s) → $platform_dir (was $json_before)"
    return 0
}

resolve_platform_list() {
    local -n _out=$1
    _out=()

    if [[ ${#PLATFORM_FILTER[@]} -gt 0 ]]; then
        _out=("${PLATFORM_FILTER[@]}")
        return 0
    fi

    if $DOWNLOAD_CORE; then
        _out=("${CORE_PLATFORM_ZIPS[@]}")
        return 0
    fi

    if $DOWNLOAD_ALL; then
        local list_json
        list_json="$(curl -fsSL --max-time 120 "${API_BASE}")" || {
            echo "error: failed to fetch platform list from ${API_BASE}" >&2
            return 1
        }
        mapfile -t _out < <(printf '%s' "$list_json" | python3 -c 'import json,sys; print("\n".join(json.load(sys.stdin)))')
        return 0
    fi

    return 0
}

echo "==> Hasheous offline dumps → $DUMP_ROOT"
echo "    cache=$CACHE_DIR"

HASHEOUS_ANY_CHANGED=false
platforms=()
resolve_platform_list platforms

downloaded=0
unchanged=0
failed=0
for zip_name in "${platforms[@]}"; do
    [[ -n "$zip_name" ]] || continue
    dl_rc=0
    download_platform_zip "$zip_name" || dl_rc=$?
    case "$dl_rc" in
        0) downloaded=$((downloaded + 1)) ;;
        2) unchanged=$((unchanged + 1)) ;;
        *) failed=$((failed + 1)) ;;
    esac
done

json_count="$(find "$DUMP_ROOT" -type f -name '*.json' ! -name 'PlatformMapping.json' 2>/dev/null | wc -l | tr -d ' ')"

if $HASHEOUS_ANY_CHANGED; then
    # Drop cached offline index so the next build rebuilds from fresh dumps.
    rm -f "$ROOT_DIR/data/hasheous/hasheous_offline_index.sqlite" \
        "$ROOT_DIR/data/hasheous/hasheous_offline_index.sqlite-shm" \
        "$ROOT_DIR/data/hasheous/hasheous_offline_index.sqlite-wal" 2>/dev/null || true
fi

echo ""
echo "==> Hasheous dump summary"
echo "    platforms processed: ${#platforms[@]} (${downloaded} updated, ${unchanged} unchanged, ${failed} failed)"
echo "    game JSON files:     $json_count"
if [[ "$json_count" -eq 0 ]]; then
    echo "  Offline Hasheous enrichment will be skipped during compendium builds."
    exit 1
fi
echo "  Offline Hasheous enrichment available."
