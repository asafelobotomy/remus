#!/usr/bin/env bash
# Start build_compendium_full.sh in a new session so it survives IDE/terminal crashes.
# Usage: scripts/run_compendium_full_build_detached.sh [build_compendium_full.sh args...]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_FILE="${REMUS_COMPENDIUM_BUILD_LOG:-${TMPDIR:-/tmp}/remus_compendium_full_build.log}"
OUTPUT_DB="${REMUS_COMPENDIUM_DB:-$ROOT_DIR/data/compendium/remus_compendium.db}"

# Allow caller to override output DB via first --output-db pair in forwarded args.
forwarded_args=("$@")
for ((i = 0; i < ${#forwarded_args[@]}; i++)); do
    if [[ "${forwarded_args[$i]}" == "--output-db" && $((i + 1)) -lt ${#forwarded_args[@]} ]]; then
        OUTPUT_DB="${forwarded_args[$((i + 1))]}"
        break
    fi
done

# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/compendium_db_guard.sh"

LOCK_PATH="$(compendium_db_lock_path "$OUTPUT_DB")"

exec 9>>"$LOCK_PATH"
if ! flock -n 9; then
    holder="$(compendium_full_build_lock_holder "$LOCK_PATH")"
    echo "error: full compendium build already running (lock: $LOCK_PATH pid=${holder:-unknown})" >&2
    echo "hint: tail -f $LOG_FILE" >&2
    exit 1
fi

mkdir -p "$(dirname "$LOG_FILE")"
echo "==> Starting detached compendium build ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
echo "    log=$LOG_FILE"
echo "    lock=$LOCK_PATH"
echo "    args=$*"

export REMUS_COMPENDIUM_FULL_BUILD_LOCK_HELD=1
setsid bash -c "
  cd '$ROOT_DIR'
  source scripts/ensure_npm_build_tools.sh
  ensure_npm_build_tools
  exec scripts/build_compendium_full.sh \"\$@\"
" _ "$@" >>"$LOG_FILE" 2>&1 < /dev/null &
build_pid=$!
printf '%s\n' "$build_pid" >"$LOCK_PATH"
echo "==> Detached build pid=$build_pid"
echo "    monitor: tail -f $LOG_FILE"
echo "    progress: cat $ROOT_DIR/data/compendium/remus_compendium.db.progress.json"
wait "$build_pid"
exit $?
