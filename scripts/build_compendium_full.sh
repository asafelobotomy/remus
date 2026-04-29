#!/usr/bin/env bash
# purpose: run the full compendium pipeline (update DATs, generate manifest, build DB, emit coverage report).
# when: use for full-catalogue refreshes; avoid for quick single-manifest experiments.
# inputs: --skip-update, --dat-dir <path>, --manifest <path>, --output-db <path>, --coverage-report <path>
# outputs: refreshed compendium DB, manifest JSON, and TSV coverage report by source.
# risk: safe
# source: original

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SKIP_UPDATE=false
DAT_DIR="$ROOT_DIR/data/databases"
MANIFEST_PATH="$ROOT_DIR/data/compendium/compendium-manifest-full.json"
OUTPUT_DB="$ROOT_DIR/data/compendium/remus_compendium.db"
COVERAGE_REPORT="$ROOT_DIR/data/compendium/remus_compendium.coverage.tsv"
BUILD_LOG="${TMPDIR:-/tmp}/remus_compendium_full_build.log"
LOCK_PATH="$ROOT_DIR/data/compendium/remus_compendium_full.lock"
HEARTBEAT_SECONDS=600

monitor_pid=""

cleanup_monitor() {
    if [[ -n "$monitor_pid" ]]; then
        kill "$monitor_pid" >/dev/null 2>&1 || true
        wait "$monitor_pid" 2>/dev/null || true
    fi
}

emit_build_heartbeat() {
    local build_pid="$1"
    local interval_seconds="$2"
    local started_at="$3"

    while kill -0 "$build_pid" >/dev/null 2>&1; do
        sleep "$interval_seconds"
        if ! kill -0 "$build_pid" >/dev/null 2>&1; then
            break
        fi

        local now elapsed_seconds log_size marker
        now=$(date +%s)
        elapsed_seconds=$((now - started_at))
        log_size=$(stat -c '%s' "$BUILD_LOG" 2>/dev/null || echo 0)

        marker=$(tail -n 300 "$BUILD_LOG" 2>/dev/null | grep -E "ClrMameProParser: Found|LibretroMetadataParser: Loaded metadata|=== Build Compendium ===|Build ID:|Sources recorded:|Unresolved conflicts:|Metadata enrichment failed|GameTDB enrichment failed|Compiler service failed|Integrity check failed" | tail -n 1 || true)
        if [[ -z "$marker" ]]; then
            marker="(no stage marker yet)"
        fi

        echo "==> Build heartbeat: elapsed=${elapsed_seconds}s log_size=${log_size} marker=${marker}"
    done
}

trap cleanup_monitor EXIT

usage() {
    cat <<'USAGE'
Usage:
  scripts/build_compendium_full.sh [options]

Options:
  --skip-update             Skip `scripts/update_dats.sh --all`
  --dat-dir <path>          DAT directory for manifest generation
  --manifest <path>         Manifest output/input path
  --output-db <path>        Compendium SQLite output path
  --coverage-report <path>  TSV coverage report path
    --heartbeat-seconds <n>   Wrapper progress heartbeat interval in seconds (default: 600)
  -h, --help                Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-update)
            SKIP_UPDATE=true
            shift
            ;;
        --dat-dir)
            DAT_DIR="$2"
            shift 2
            ;;
        --manifest)
            MANIFEST_PATH="$2"
            shift 2
            ;;
        --output-db)
            OUTPUT_DB="$2"
            shift 2
            ;;
        --coverage-report)
            COVERAGE_REPORT="$2"
            shift 2
            ;;
        --heartbeat-seconds)
            HEARTBEAT_SECONDS="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

if [[ ! -x "$ROOT_DIR/build/remus-cli" ]]; then
    echo "error: missing executable: $ROOT_DIR/build/remus-cli" >&2
    echo "hint: run 'cmake --build build' first" >&2
    exit 1
fi

if [[ ! -x "$ROOT_DIR/scripts/generate_compendium_manifest.sh" ]]; then
    echo "error: missing executable: $ROOT_DIR/scripts/generate_compendium_manifest.sh" >&2
    exit 1
fi

if ! command -v flock >/dev/null 2>&1; then
    echo "error: flock is required but not installed" >&2
    exit 1
fi

if ! [[ "$HEARTBEAT_SECONDS" =~ ^[0-9]+$ ]] || [[ "$HEARTBEAT_SECONDS" -lt 1 ]]; then
    echo "error: --heartbeat-seconds must be a positive integer" >&2
    exit 1
fi

mkdir -p "$(dirname "$MANIFEST_PATH")"
mkdir -p "$(dirname "$OUTPUT_DB")"
mkdir -p "$(dirname "$COVERAGE_REPORT")"

mapfile -t active_other_builds < <(pgrep -f "remus-cli --build-compendium" || true)

if (( ${#active_other_builds[@]} > 0 )); then
    echo "error: another compendium compiler process is already running (pids: ${active_other_builds[*]})" >&2
    exit 1
fi

exec 9>"$LOCK_PATH"
if ! flock -n 9; then
    echo "error: another full compendium build is already running (lock: $LOCK_PATH)" >&2
    exit 1
fi

printf '%s\n' "$BASHPID" 1>&9

echo "==> Full compendium pipeline"
echo "    dat_dir=$DAT_DIR"
echo "    manifest=$MANIFEST_PATH"
echo "    output_db=$OUTPUT_DB"
echo "    coverage_report=$COVERAGE_REPORT"
echo "    lock=$LOCK_PATH"
echo "    heartbeat_seconds=$HEARTBEAT_SECONDS"

if ! $SKIP_UPDATE; then
    echo "==> Updating all DAT sources"
    "$ROOT_DIR/scripts/update_dats.sh" --all
else
    echo "==> Skipping DAT update (--skip-update)"
fi

echo "==> Generating manifest"
"$ROOT_DIR/scripts/generate_compendium_manifest.sh" \
    --dat-dir "$DAT_DIR" \
    --output "$MANIFEST_PATH"

echo "==> Building compendium DB"
build_started_at=$(date +%s)
set +e
"$ROOT_DIR/build/remus-cli" \
    --build-compendium \
    --compendium-manifest "$MANIFEST_PATH" \
    --compendium-output "$OUTPUT_DB" \
    >"$BUILD_LOG" 2>&1 &
build_pid=$!

emit_build_heartbeat "$build_pid" "$HEARTBEAT_SECONDS" "$build_started_at" &
monitor_pid=$!

wait "$build_pid"
build_rc=$?

cleanup_monitor
monitor_pid=""
set -e

if [[ "$build_rc" -ne 0 && "$build_rc" -ne 2 ]]; then
    echo "error: compendium build failed with exit code $build_rc" >&2
    echo "--- build log tail ---" >&2
    tail -40 "$BUILD_LOG" >&2
    exit "$build_rc"
fi

if [[ "$build_rc" -eq 2 ]]; then
    echo "==> Build completed with unresolved conflicts (exit code 2)"
else
    echo "==> Build completed cleanly (exit code 0)"
fi

# Emit a machine-friendly per-source yield report.
# Columns: source_id,source_items,signatures,signature_yield_percent
sqlite3 -header -separator $'\t' "$OUTPUT_DB" "
WITH si AS (
  SELECT source_id, COUNT(*) AS source_items
  FROM source_items
  GROUP BY source_id
), gs AS (
  SELECT source_id, COUNT(*) AS signatures
  FROM game_signatures
  GROUP BY source_id
)
SELECT si.source_id,
       si.source_items,
       COALESCE(gs.signatures, 0) AS signatures,
       ROUND(COALESCE(gs.signatures, 0) * 100.0 / si.source_items, 2) AS signature_yield_percent
FROM si
LEFT JOIN gs ON gs.source_id = si.source_id
ORDER BY signature_yield_percent ASC, si.source_items DESC;
" > "$COVERAGE_REPORT"

summary_query="
SELECT 'games', COUNT(*) FROM games
UNION ALL SELECT 'game_signatures', COUNT(*) FROM game_signatures
UNION ALL SELECT 'source_items', COUNT(*) FROM source_items
UNION ALL SELECT 'sources', COUNT(*) FROM sources
UNION ALL SELECT 'source_snapshots', COUNT(*) FROM source_snapshots
UNION ALL SELECT 'unresolved_merge_conflicts', COUNT(*)
  FROM merge_conflicts WHERE resolution_status = 'unresolved';
"

echo "==> Build summary"
sqlite3 -header -column "$OUTPUT_DB" "$summary_query"

echo "==> Low-yield sources (top 15)"
head -16 "$COVERAGE_REPORT"

echo "==> Coverage report written: $COVERAGE_REPORT"
echo "==> Build log: $BUILD_LOG"
