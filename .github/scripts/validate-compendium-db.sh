#!/usr/bin/env bash
# Run phase-1 compendium validation checks and fail when any gate reports FAIL.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DB_PATH="${1:-}"
VALIDATION_SQL="${2:-$ROOT_DIR/data/compendium/validation/0001_phase1_checks.sql}"

if [[ -z "$DB_PATH" ]]; then
    echo "Usage: validate-compendium-db.sh <compendium.db> [validation.sql]" >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

if [[ ! -f "$VALIDATION_SQL" ]]; then
    echo "error: validation SQL not found: $VALIDATION_SQL" >&2
    exit 1
fi

mapfile -t failures < <(
    sed '/^-- Diagnostic details/,$d' "$VALIDATION_SQL" \
        | sqlite3 -header -csv "$DB_PATH" \
        | awk -F',' 'NR > 1 && $2 == "FAIL" { print $1 }'
)

echo "==> Phase 1 validation: $DB_PATH"
sed '/^-- Diagnostic details/,$d' "$VALIDATION_SQL" | sqlite3 -header -column "$DB_PATH"

if ((${#failures[@]} > 0)); then
    echo "error: validation failed for ${#failures[@]} check(s):" >&2
    for check in "${failures[@]}"; do
        echo "  - $check" >&2
    done
    exit 1
fi

echo "==> Phase 1 validation passed"
