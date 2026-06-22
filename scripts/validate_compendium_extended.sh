#!/usr/bin/env bash
# Extended compendium checks (~30s with migrations 0008/0009 applied). Informational thresholds.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"

bash "$ROOT_DIR/scripts/apply_compendium_migrations.sh" "$DB_PATH"

exec "$ROOT_DIR/scripts/run_compendium_job.sh" --db "$DB_PATH" --timeout 300 -- \
    bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" \
    "$DB_PATH" \
    "$ROOT_DIR/data/compendium/validation/0003_phase2_extended_checks.sql"
