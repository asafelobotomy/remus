#!/usr/bin/env bash
# First-use compendium bootstrap: schema (if missing) + full offline catalog build.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

OUTPUT_DB="${REMUS_COMPENDIUM_DB:-$ROOT_DIR/data/compendium/remus_compendium.db}"

echo "== Remus compendium init =="
echo "Output DB: $OUTPUT_DB"

if [[ ! -f "$OUTPUT_DB" ]]; then
    echo "Creating bootstrap schema..."
    bash "$ROOT_DIR/scripts/setup_compendium_db.sh"
fi

echo "Running full compendium build (this may take a long time)..."
bash "$ROOT_DIR/scripts/build_compendium_full.sh" --output-db "$OUTPUT_DB" "$@"

echo ""
echo "Compendium ready for offline use at: $OUTPUT_DB"
echo "Extend later with: remus-cli --enrich-compendium --enrich-source <name>"
