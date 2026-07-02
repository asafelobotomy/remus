#!/usr/bin/env bash
# Serialize compendium DB jobs (validation, enrichment, migrations) with flock + busy_timeout.
# Prevents parallel sqlite3 readers/writers from blocking each other for hours.
#
# Usage:
#   scripts/run_compendium_job.sh [--timeout SEC] [--db PATH] -- <command...>
#
# Examples:
#   scripts/run_compendium_job.sh -- bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db
#   scripts/run_compendium_job.sh --timeout 600 -- ./build/remus-cli --enrich-compendium ...
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="$ROOT_DIR/data/compendium/remus_compendium.db"
TIMEOUT_SEC=0
NO_LOCK=false
LOCK_DIR="${XDG_RUNTIME_DIR:-/tmp}"
if ! mkdir -p "$LOCK_DIR" 2>/dev/null; then
    LOCK_DIR="/tmp"
fi
LOCK_FILE="$LOCK_DIR/$(basename "$DB_PATH").lock"

usage() {
    cat <<EOF
usage: $(basename "$0") [options] -- <command...>

Options:
  --db <path>       Compendium DB path for health warnings (default: data/compendium/remus_compendium.db)
  --timeout <sec>   Kill command after SEC seconds (0 = no limit; default: 0)
  --no-lock         Skip flock (caller already holds the per-DB lock)
  -h, --help        Show this help

Holds an exclusive flock on \$LOCK_FILE ($LOCK_FILE) for the duration of the command.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --db)
            DB_PATH="$2"
            shift 2
            ;;
        --timeout)
            TIMEOUT_SEC="$2"
            shift 2
            ;;
        --no-lock)
            NO_LOCK=true
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "error: unexpected argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ $# -eq 0 ]]; then
    echo "error: command required after --" >&2
    usage >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "warning: compendium DB not found (lock still applies): $DB_PATH" >&2
else
    db_pattern="$(printf '%s' "$DB_PATH" | sed "s/[.[\*^$()+?{|]/\\\\&/g")"
    if pgrep -af "sqlite3.*${db_pattern}" >/dev/null 2>&1 \
        || pgrep -af "remus-cli.*--enrich-compendium" >/dev/null 2>&1 \
        || pgrep -af "remus-cli.*--build-compendium" >/dev/null 2>&1; then
        echo "warning: other compendium processes may be running on this DB:" >&2
        pgrep -af "sqlite3.*${db_pattern}" 2>/dev/null | head -3 >&2 || true
        pgrep -af "remus-cli.*compendium" 2>/dev/null | head -3 >&2 || true
    fi
fi

if $NO_LOCK || [[ "${REMUS_COMPENDIUM_JOB_NO_LOCK:-}" == "1" ]]; then
    echo "==> Compendium job starting (lock skipped; caller holds flock) ($(date -u +%H:%M:%SZ))"
else
    exec 9>"$LOCK_FILE"
    if ! flock -n 9; then
        echo "error: another compendium job holds $LOCK_FILE" >&2
        echo "hint: pgrep -af 'sqlite3|remus-cli.*compendium'" >&2
        exit 1
    fi
    echo "==> Compendium job lock acquired ($(date -u +%H:%M:%SZ))"
fi

run_cmd() {
    if (( TIMEOUT_SEC > 0 )); then
        if command -v timeout >/dev/null 2>&1; then
            timeout --foreground "${TIMEOUT_SEC}s" "$@"
        else
            echo "warning: timeout(1) not found; running without time limit" >&2
            "$@"
        fi
    else
        "$@"
    fi
}

run_cmd "$@"
exit_code=$?
echo "==> Compendium job finished (exit $exit_code)"
exit "$exit_code"
