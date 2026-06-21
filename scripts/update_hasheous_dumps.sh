#!/usr/bin/env bash
# Verify Hasheous offline dump JSON files under data/hasheous/dumps/.
#
# Hasheous does not publish platform ZIP dumps on the public API. Offline enrichment
# uses JSON export files placed locally (for example from a self-hosted Gaseous/Hasheous
# server export). See data/compendium/README.md for layout.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DUMP_ROOT="$ROOT_DIR/data/hasheous/dumps"

usage() {
    cat <<'USAGE'
Usage:
  scripts/update_hasheous_dumps.sh [options]

Options:
  --output-dir <p>  Dump root to inspect (default: data/hasheous/dumps)
  -h, --help        Show this help

This script does not download from hasheous.org — place *.json dump files under
data/hasheous/dumps/ manually. Compendium builds use them for offline Hasheous
enrichment when present.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir)
            DUMP_ROOT="$2"
            shift 2
            ;;
        --all-core)
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            shift
            ;;
    esac
done

mkdir -p "$DUMP_ROOT"

json_count=0
while IFS= read -r -d '' f; do
    json_count=$((json_count + 1))
done < <(find "$DUMP_ROOT" -type f -name '*.json' -print0 2>/dev/null || true)

echo "==> Hasheous offline dumps → $DUMP_ROOT"
if [[ "$json_count" -eq 0 ]]; then
    echo "  No JSON dump files found."
    echo "  Offline Hasheous enrichment will be skipped during compendium builds."
    echo "  Place Hasheous export JSON under: $DUMP_ROOT"
    exit 0
fi

echo "  Found $json_count JSON dump file(s) — offline Hasheous enrichment available"
