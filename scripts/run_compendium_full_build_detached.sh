#!/usr/bin/env bash
# Start build_compendium_full.sh in a new session so it survives IDE/terminal crashes.
# Usage: scripts/run_compendium_full_build_detached.sh [build_compendium_full.sh args...]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_FILE="${REMUS_COMPENDIUM_BUILD_LOG:-/tmp/remus_compendium_full_build.log}"
PID_FILE="${REMUS_COMPENDIUM_BUILD_PID:-/tmp/remus_compendium_build.pid}"
LOCK_PATH="$ROOT_DIR/data/compendium/remus_compendium_full.lock"

# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/compendium_db_guard.sh"

if [[ -f "$PID_FILE" ]]; then
    old_pid="$(cat "$PID_FILE" 2>/dev/null || true)"
    if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
        echo "error: build already running (pid=$old_pid)" >&2
        echo "hint: tail -f $LOG_FILE" >&2
        exit 1
    fi
fi

if compendium_full_build_is_running "$LOCK_PATH"; then
    holder="$(compendium_full_build_lock_holder "$LOCK_PATH")"
    echo "error: full compendium build already running (pid=${holder:-unknown})" >&2
    echo "hint: tail -f $LOG_FILE" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG_FILE")"
echo "==> Starting detached compendium build ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
echo "    log=$LOG_FILE"
echo "    args=$*"

setsid bash -c "
  cd '$ROOT_DIR'
  source scripts/ensure_npm_build_tools.sh
  ensure_npm_build_tools
  exec scripts/build_compendium_full.sh \"\$@\"
" _ "$@" >>"$LOG_FILE" 2>&1 < /dev/null &
build_pid=$!
echo "$build_pid" >"$PID_FILE"
echo "==> Detached build pid=$build_pid"
echo "    monitor: tail -f $LOG_FILE"
echo "    progress: cat $ROOT_DIR/data/compendium/remus_compendium.db.progress.json"
