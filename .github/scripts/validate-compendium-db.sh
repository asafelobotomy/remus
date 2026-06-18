#!/usr/bin/env bash
# Run compendium validation checks and fail when any gate reports FAIL.
# WARN rows are printed and counted; pass --strict to treat WARN as failure.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DB_PATH="${1:-}"
VALIDATION_SQL="${2:-$ROOT_DIR/data/compendium/validation/0001_phase1_checks.sql}"
STRICT_WARN=0

shift $(( $# > 0 ? 1 : 0 )) || true
shift $(( $# > 0 ? 1 : 0 )) || true

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict)
            STRICT_WARN=1
            ;;
        --warn-only)
            STRICT_WARN=0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            echo "Usage: validate-compendium-db.sh <compendium.db> [validation.sql] [--strict|--warn-only]" >&2
            exit 1
            ;;
    esac
    shift
done

if [[ -z "$DB_PATH" ]]; then
    echo "Usage: validate-compendium-db.sh <compendium.db> [validation.sql] [--strict|--warn-only]" >&2
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

VALIDATION_LABEL="$(basename "$VALIDATION_SQL" .sql)"

mapfile -t failures < <(
    sed '/^-- Diagnostic details/,$d' "$VALIDATION_SQL" \
        | sqlite3 -header -csv "$DB_PATH" \
        | awk -F',' 'NR > 1 && $2 == "FAIL" { print $1 }'
)

mapfile -t warnings < <(
    sed '/^-- Diagnostic details/,$d' "$VALIDATION_SQL" \
        | sqlite3 -header -csv "$DB_PATH" \
        | awk -F',' 'NR > 1 && $2 == "WARN" { print $1 }'
)

echo "==> Compendium validation ($VALIDATION_LABEL): $DB_PATH"
sed '/^-- Diagnostic details/,$d' "$VALIDATION_SQL" | sqlite3 -header -column "$DB_PATH"

if ((${#warnings[@]} > 0)); then
    echo "warning: ${#warnings[@]} check(s) reported WARN:" >&2
    for check in "${warnings[@]}"; do
        echo "  - $check" >&2
    done
fi

if ((${#failures[@]} > 0)); then
    echo "error: validation failed for ${#failures[@]} check(s):" >&2
    for check in "${failures[@]}"; do
        echo "  - $check" >&2
    done
    exit 1
fi

if ((${#warnings[@]} > 0)) && (( STRICT_WARN == 1 )); then
    echo "error: strict mode treats ${#warnings[@]} WARN check(s) as failure" >&2
    exit 1
fi

echo "==> Compendium validation passed ($VALIDATION_LABEL)"
