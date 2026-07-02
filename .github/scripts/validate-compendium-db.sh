#!/usr/bin/env bash
# Run compendium validation checks and fail when any gate reports FAIL.
# WARN rows are printed and counted; pass --strict to treat WARN as failure.
#
# Runs the validation SQL once (CSV), then derives failures/warnings and pretty-prints.
# Use --verbose to run each check statement separately with progress lines (slower).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DB_PATH="${1:-}"
VALIDATION_SQL="${2:-$ROOT_DIR/data/compendium/validation/0001_phase1_checks.sql}"
STRICT_WARN=0
VERBOSE=0

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
        --verbose)
            VERBOSE=1
            ;;
        *)
            echo "error: unknown option: $1" >&2
            echo "Usage: validate-compendium-db.sh <compendium.db> [validation.sql] [--strict|--warn-only] [--verbose]" >&2
            exit 1
            ;;
    esac
    shift
done

if [[ -z "$DB_PATH" ]]; then
    echo "Usage: validate-compendium-db.sh <compendium.db> [validation.sql] [--strict|--warn-only] [--verbose]" >&2
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
CHECK_SQL="$(sed '/^-- Diagnostic details/,$d' "$VALIDATION_SQL")"

run_sqlite_csv() {
    {
        echo "PRAGMA busy_timeout=60000;"
        cat
    } | sqlite3 -header -csv "$1"
    shift
}

print_csv_as_columns() {
    awk -F',' '
        function flush_block() {
            if (n > 0) {
                for (i = 1; i <= n; i++) printf "  %s\n", buf[i]
                print ""
            }
            n = 0
        }
        NR == 1 || ($1 ~ /^check_name$/ && NR > 1) {
            flush_block()
        }
        {
            buf[++n] = $0
        }
        END {
            flush_block()
        }
    ' | column -t -s','
}

declare -a failures=()
declare -a warnings=()

if (( VERBOSE == 1 )); then
    echo "==> Compendium validation ($VALIDATION_LABEL, verbose): $DB_PATH"
    csv_tmp="$(mktemp)"
    trap 'rm -f "$csv_tmp"' EXIT

    while IFS= read -r stmt; do
        [[ -z "$stmt" ]] && continue
        check_hint="$(printf '%s\n' "$stmt" | grep -oE "'[^']+' AS check_name" | head -1 | tr -d "'" || true)"
        if [[ -n "$check_hint" ]]; then
            echo "==> check: $check_hint"
        fi
        run_sqlite_csv "$DB_PATH" <<SQL >>"$csv_tmp"
$stmt
SQL
        printf '\n' >>"$csv_tmp"
    done < <(printf '%s\n' "$CHECK_SQL" | awk '
        BEGIN { stmt = "" }
        /^--/ { next }
        /^[[:space:]]*$/ {
            if (stmt != "") { print stmt; stmt = "" }
            next
        }
        {
            if (stmt == "") stmt = $0
            else stmt = stmt " " $0
            if ($0 ~ /;[[:space:]]*$/) { print stmt; stmt = "" }
        }
        END { if (stmt != "") print stmt }
    ')

    while IFS=',' read -r check_name status _rest; do
        [[ "$check_name" == "check_name" ]] && continue
        [[ "$check_name" =~ ^[0-9]+$ ]] && continue
        check_name="${check_name#\"}"
        check_name="${check_name%\"}"
        status="${status#\"}"
        status="${status%\"}"
        [[ -z "$check_name" || "$check_name" != *.* ]] && continue
        case "$status" in
            FAIL) failures+=("$check_name") ;;
            WARN) warnings+=("$check_name") ;;
        esac
    done < "$csv_tmp"

    print_csv_as_columns < "$csv_tmp"
else
    csv_tmp="$(mktemp)"
    trap 'rm -f "$csv_tmp"' EXIT

    run_sqlite_csv "$DB_PATH" <<SQL >"$csv_tmp"
$CHECK_SQL
SQL

    while IFS=',' read -r check_name status _rest; do
        [[ "$check_name" == "check_name" ]] && continue
        [[ "$check_name" =~ ^[0-9]+$ ]] && continue
        check_name="${check_name#\"}"
        check_name="${check_name%\"}"
        status="${status#\"}"
        status="${status%\"}"
        [[ -z "$check_name" || "$check_name" != *.* ]] && continue
        case "$status" in
            FAIL) failures+=("$check_name") ;;
            WARN) warnings+=("$check_name") ;;
        esac
    done < "$csv_tmp"

    echo "==> Compendium validation ($VALIDATION_LABEL): $DB_PATH"
    print_csv_as_columns < "$csv_tmp"
fi

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
