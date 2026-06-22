#!/usr/bin/env bash
# Fast compendium quality gate (~1 min). Use validate_compendium_extended.sh for 0003 checks.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="${1:-$ROOT_DIR/data/compendium/remus_compendium.db}"

bash "$ROOT_DIR/scripts/apply_compendium_migrations.sh" "$DB_PATH"

exec "$ROOT_DIR/scripts/run_compendium_job.sh" --db "$DB_PATH" -- \
    bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" \
    "$DB_PATH" \
    "$ROOT_DIR/data/compendium/validation/0002_phase2_quality_checks.sql"
