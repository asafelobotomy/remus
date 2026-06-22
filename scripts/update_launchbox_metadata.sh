#!/usr/bin/env bash
# Install or validate LaunchBox Games Database Metadata.xml for compendium enrichment.
#
# LaunchBox distributes Metadata.xml inside the free LaunchBox Windows app.
# Remus does not redistribute the database; place or copy your export locally.
#
# Usage:
#   scripts/update_launchbox_metadata.sh
#   scripts/update_launchbox_metadata.sh --source /path/to/Metadata.xml
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="$ROOT_DIR/data/launchbox"
DEST_FILE="$DEST_DIR/Metadata.xml"
SOURCE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source)
            SOURCE="${2:-}"
            shift 2
            ;;
        -h|--help)
            sed -n '1,12p' "$0"
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

mkdir -p "$DEST_DIR"

if [[ -n "$SOURCE" ]]; then
    if [[ ! -f "$SOURCE" ]]; then
        echo "error: source file not found: $SOURCE" >&2
        exit 1
    fi
    echo "==> Copying LaunchBox Metadata.xml"
    cp -- "$SOURCE" "$DEST_FILE"
elif [[ -f "$DEST_FILE" ]]; then
    echo "==> LaunchBox metadata already present: $DEST_FILE ($(du -sh "$DEST_FILE" | cut -f1))"
else
    cat >&2 <<EOF
error: LaunchBox Metadata.xml not found.

Place the file at:
  $DEST_FILE

Or copy from your LaunchBox install / export:
  $0 --source /path/to/Metadata.xml

See: https://gamesdb.launchbox-app.com/
EOF
    exit 1
fi

if ! head -c 200 "$DEST_FILE" | grep -q '<LaunchBox'; then
    echo "warning: $DEST_FILE does not look like a LaunchBox Metadata.xml export" >&2
fi

echo "==> LaunchBox metadata ready for --enrich-source launchbox"
