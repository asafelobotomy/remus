#!/usr/bin/env bash
# Install or validate LaunchBox Games Database Metadata.xml for compendium enrichment.
#
# Remus does not redistribute the database. By default this script downloads the
# official bulk export from gamesdb.launchbox-app.com into data/launchbox/.
#
# Usage:
#   scripts/update_launchbox_metadata.sh
#   scripts/update_launchbox_metadata.sh --source /path/to/Metadata.xml
#   scripts/update_launchbox_metadata.sh --force
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=compendium_offline_helpers.sh
source "$SCRIPT_DIR/compendium_offline_helpers.sh"

DEST_DIR="$ROOT_DIR/data/launchbox"
DEST_FILE="$DEST_DIR/Metadata.xml"
CACHE_DIR="${XDG_CACHE_HOME:-$ROOT_DIR/.cache}/remus/launchbox"
METADATA_ZIP_URL="https://gamesdb.launchbox-app.com/Metadata.zip"
CACHE_ZIP="$CACHE_DIR/Metadata.zip"
CACHE_ZIP_SHA="$CACHE_DIR/Metadata.zip.sha256"
CACHE_TTL_SECONDS="${LAUNCHBOX_METADATA_CACHE_TTL_SECONDS:-86400}"

SOURCE=""
OPTIONAL=false
FORCE=false
MIN_VALID_BYTES=1048576

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source)
            SOURCE="${2:-}"
            shift 2
            ;;
        --optional)
            OPTIONAL=true
            shift
            ;;
        --force)
            FORCE=true
            shift
            ;;
        -h|--help)
            sed -n '1,14p' "$0"
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

mkdir -p "$DEST_DIR"

launchbox_dest_valid() {
    [[ -f "$DEST_FILE" ]] || return 1
    local dest_size
    dest_size="$(stat -c%s "$DEST_FILE" 2>/dev/null || echo 0)"
    [[ "$dest_size" -gt "$MIN_VALID_BYTES" ]] || return 1
    head -c 200 "$DEST_FILE" | grep -q '<LaunchBox'
}

is_cache_fresh() {
    local file_path="$1"
    [[ -f "$file_path" ]] || return 1
    [[ "$CACHE_TTL_SECONDS" =~ ^[0-9]+$ ]] || return 1
    local file_mtime now age
    file_mtime=$(stat -c %Y "$file_path" 2>/dev/null || return 1)
    now=$(date +%s)
    age=$((now - file_mtime))
    [[ "$age" -lt "$CACHE_TTL_SECONDS" ]]
}

download_metadata_zip() {
    if ! command -v curl >/dev/null 2>&1; then
        echo "error: curl is required to download LaunchBox metadata" >&2
        return 1
    fi
    if ! command -v unzip >/dev/null 2>&1; then
        echo "error: unzip is required to extract LaunchBox metadata" >&2
        return 1
    fi

    mkdir -p "$CACHE_DIR"

    if is_cache_fresh "$CACHE_ZIP" && [[ -f "$CACHE_ZIP" ]]; then
        echo "==> Using cached LaunchBox Metadata.zip"
    else
        echo "==> Downloading LaunchBox Metadata.zip"
        local cache_tmp
        cache_tmp="$CACHE_DIR/.tmp.$$.Metadata.zip"
        if ! curl -fL --retry 2 --retry-delay 1 --max-time 900 --progress-bar \
            -o "$cache_tmp" "$METADATA_ZIP_URL"; then
            rm -f "$cache_tmp"
            echo "error: failed to download $METADATA_ZIP_URL" >&2
            return 1
        fi
        mv -- "$cache_tmp" "$CACHE_ZIP"
    fi

    local zip_hash
    zip_hash="$(compendium_sha256_of "$CACHE_ZIP")"
    if [[ -f "$CACHE_ZIP_SHA" ]] \
       && [[ "$(cat "$CACHE_ZIP_SHA")" == "$zip_hash" ]] \
       && launchbox_dest_valid \
       && ! $FORCE; then
        echo "==> LaunchBox metadata unchanged ($(du -sh "$DEST_FILE" | cut -f1))"
        return 0
    fi

    echo "==> Extracting Metadata.xml"
    local tmp_xml
    tmp_xml="$(mktemp)"
    if ! unzip -p "$CACHE_ZIP" Metadata.xml > "$tmp_xml" 2>/dev/null; then
        rm -f "$tmp_xml" "$CACHE_ZIP" "$CACHE_ZIP_SHA"
        echo "error: failed to extract Metadata.xml from LaunchBox zip (cache evicted)" >&2
        return 1
    fi

    local tmp_size
    tmp_size="$(stat -c%s "$tmp_xml" 2>/dev/null || echo 0)"
    if [[ "$tmp_size" -le "$MIN_VALID_BYTES" ]] \
       || ! head -c 200 "$tmp_xml" | grep -q '<LaunchBox'; then
        rm -f "$tmp_xml"
        echo "error: extracted Metadata.xml does not look like a valid LaunchBox export" >&2
        return 1
    fi

    case "$(install_file_if_changed "$tmp_xml" "$DEST_FILE")" in
        updated)
            echo "==> LaunchBox metadata installed: $DEST_FILE ($(du -sh "$DEST_FILE" | cut -f1))"
            ;;
        unchanged)
            echo "==> LaunchBox metadata unchanged: $DEST_FILE ($(du -sh "$DEST_FILE" | cut -f1))"
            ;;
    esac
    rm -f "$tmp_xml"
    printf '%s\n' "$zip_hash" > "$CACHE_ZIP_SHA"
    return 0
}

if [[ -n "$SOURCE" ]]; then
    if [[ ! -f "$SOURCE" ]]; then
        echo "error: source file not found: $SOURCE" >&2
        exit 1
    fi
    echo "==> Copying LaunchBox Metadata.xml"
    cp -- "$SOURCE" "$DEST_FILE"
    if ! head -c 200 "$DEST_FILE" | grep -q '<LaunchBox'; then
        echo "warning: $DEST_FILE does not look like a LaunchBox Metadata.xml export" >&2
    fi
    echo "==> LaunchBox metadata ready for --enrich-source launchbox"
    exit 0
elif launchbox_dest_valid && ! $FORCE; then
    echo "==> LaunchBox metadata already present: $DEST_FILE ($(du -sh "$DEST_FILE" | cut -f1))"
elif download_metadata_zip; then
    :
else
    if $OPTIONAL; then
        echo "warning: LaunchBox Metadata.xml not available (optional offline source)" >&2
        echo "  Expected at: $DEST_FILE" >&2
        echo "  Or download manually from: $METADATA_ZIP_URL" >&2
        exit 0
    fi
    cat >&2 <<EOF
error: LaunchBox Metadata.xml not found and auto-download failed.

Place the file at:
  $DEST_FILE

Or copy from a local export:
  $0 --source /path/to/Metadata.xml

Bulk download:
  $METADATA_ZIP_URL

See: https://gamesdb.launchbox-app.com/
EOF
    exit 1
fi

if ! launchbox_dest_valid; then
    dest_size="$(stat -c%s "$DEST_FILE" 2>/dev/null || echo 0)"
    if $OPTIONAL; then
        echo "warning: $DEST_FILE is present but too small ($dest_size bytes) — launchbox enrichment will be skipped" >&2
        exit 0
    fi
    cat >&2 <<EOF
error: $DEST_FILE looks like a stub ($dest_size bytes).

Run:
  $0 --force

Or provide a full export:
  $0 --source /path/to/Metadata.xml
EOF
    exit 1
fi

echo "==> LaunchBox metadata ready for --enrich-source launchbox"
if [[ -f "$DEST_FILE" && -x "$SCRIPT_DIR/update_enrichment_fingerprint_sidecar.sh" ]]; then
    xml_mtime="$(stat -c%Y "$DEST_FILE" 2>/dev/null || echo 0)"
    xml_sha256="$(compendium_sha256_of "$DEST_FILE")"
    bash "$SCRIPT_DIR/update_enrichment_fingerprint_sidecar.sh" launchbox \
        "{\"xml_mtime\":$xml_mtime,\"xml_sha256\":\"$xml_sha256\"}" || true
fi
