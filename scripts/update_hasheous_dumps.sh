#!/usr/bin/env bash
# Download Hasheous offline platform dump ZIPs into data/hasheous/dumps/.
# These dumps power offline hash→metadata enrichment during compendium builds.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DUMP_ROOT="$ROOT_DIR/data/hasheous/dumps"
CACHE_DIR="${UPDATE_HASHEOUS_CACHE_DIR:-${XDG_CACHE_HOME:-$ROOT_DIR/.cache}/remus/update_hasheous}"
API_BASE="${HASHEOUS_DUMP_API:-https://hasheous.org/api/v1/Dumps/platforms}"

# Platform slugs with modest dump sizes suitable for local dev/CI smoke downloads.
DEFAULT_PLATFORMS=(
    "NintendoDS"
    "GameBoyAdvance"
    "SonyPlayStation"
    "SegaDreamcast"
    "NintendoGameCube"
)

usage() {
    cat <<'USAGE'
Usage:
  scripts/update_hasheous_dumps.sh [options] [platform ...]

Options:
  --all-core        Download the default core platform set
  --output-dir <p>  Output root (default: data/hasheous/dumps)
  -h, --help        Show this help

Examples:
  scripts/update_hasheous_dumps.sh --all-core
  scripts/update_hasheous_dumps.sh NintendoDS SegaDreamcast
USAGE
}

download_with_cache() {
    local url="$1"
    local cache_path="$2"
    local dest_path="$3"
    local label="$4"

    mkdir -p "$(dirname "$cache_path")" "$(dirname "$dest_path")"
    if [[ -f "$cache_path" && -f "$dest_path" ]]; then
        if cmp -s "$cache_path" "$dest_path"; then
            echo "  Using cached $label"
            return 0
        fi
    fi

    local tmp
    tmp="$(mktemp)"
    if curl -fsSL --retry 2 --retry-delay 1 --max-time 600 -o "$tmp" "$url"; then
        cp "$tmp" "$cache_path"
        cp "$tmp" "$dest_path"
        rm -f "$tmp"
        echo "  Downloaded $label"
        return 0
    fi
    rm -f "$tmp"
    echo "  warning: failed to download $label from $url" >&2
    return 1
}

ALL_CORE=false
PLATFORMS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all-core)
            ALL_CORE=true
            shift
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
            PLATFORMS+=("$1")
            shift
            ;;
    esac
done

if $ALL_CORE; then
    PLATFORMS=("${DEFAULT_PLATFORMS[@]}")
fi

if [[ ${#PLATFORMS[@]} -eq 0 ]]; then
    echo "error: specify --all-core or one or more platform slugs" >&2
    usage >&2
    exit 1
fi

mkdir -p "$DUMP_ROOT" "$CACHE_DIR"

echo "==> Hasheous offline dumps → $DUMP_ROOT"

downloaded=0
failed=0
for platform in "${PLATFORMS[@]}"; do
    url="${API_BASE}/${platform}.zip"
    cache_zip="$CACHE_DIR/${platform}.zip"
    dest_zip="$DUMP_ROOT/${platform}.zip"
    dest_dir="$DUMP_ROOT/${platform}"
    mkdir -p "$dest_dir"

    if download_with_cache "$url" "$cache_zip" "$dest_zip" "$platform"; then
        if unzip -qo "$dest_zip" -d "$dest_dir" 2>/dev/null; then
            downloaded=$((downloaded + 1))
        else
            echo "  warning: failed to extract $dest_zip" >&2
            failed=$((failed + 1))
        fi
    else
        failed=$((failed + 1))
    fi
done

echo "==> Hasheous dumps: $downloaded extracted, $failed failed/skipped"
