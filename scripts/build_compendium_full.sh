#!/usr/bin/env bash
# purpose: run the full compendium pipeline (update DATs, generate manifest, build DB, emit coverage report).
# when: use for full-catalogue refreshes; avoid for quick single-manifest experiments.
# inputs: --skip-update, --dat-dir <path>, --manifest <path>, --output-db <path>,
#         --coverage-report <path>, --disc-set-coverage-report <path>
# outputs: refreshed compendium DB, manifest JSON, per-source TSV coverage, and per-system disc-set TSV.
# risk: safe
# source: original

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Load provider credentials for compendium enrichment (REMUS_* → CredentialManager).
# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/load_env_local.sh"

SKIP_UPDATE=false
ALLOW_UNRESOLVED_CONFLICTS=false
SKIP_VALIDATION=false
ONLINE_ENRICHMENT=false
ONLINE_ENRICHMENT_ALL=false
DAT_DIR="$ROOT_DIR/data/databases"
MANIFEST_PATH="$ROOT_DIR/data/compendium/compendium-manifest-full.json"
OUTPUT_DB="$ROOT_DIR/data/compendium/remus_compendium.db"
PROGRESS_FILE="${OUTPUT_DB}.progress.json"
COVERAGE_REPORT="$ROOT_DIR/data/compendium/remus_compendium.coverage.tsv"
DISC_SET_COVERAGE_REPORT="$ROOT_DIR/data/compendium/remus_compendium.disc-set-coverage.tsv"
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

cleanup_lock() {
    if [[ -e "$LOCK_PATH" ]]; then
        : > "$LOCK_PATH"
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

        local now elapsed_seconds pct marker
        now=$(date +%s)
        elapsed_seconds=$((now - started_at))

        # Read structured progress from the JSON file written by remus-cli.
        # Query manually: cat "${OUTPUT_DB}.progress.json" (or pipe to jq)
        pct="?"
        marker="(progress file not yet written — starting up)"
        if [[ -f "$PROGRESS_FILE" ]]; then
            if command -v jq &>/dev/null; then
                pct=$(jq -r '"\(.overall_pct // "?")%"' "$PROGRESS_FILE" 2>/dev/null || echo "?%")
                marker=$(jq -r '
                    if .enrichment_pass_name != null and .enrichment_pass_name != "" then
                        "pass \(.enrichment_pass_current // "?")/\(.enrichment_pass_total // "?"): \(.enrichment_pass_name)"
                    elif .current != null and .total != null then
                        "[\(.current)/\(.total)] \(.status) \(.current_source) (\(.records_ingested // 0) records)"
                    else
                        "\(.status // "running")"
                    end' "$PROGRESS_FILE" 2>/dev/null || cat "$PROGRESS_FILE")
            else
                pct="?"
                marker=$(cat "$PROGRESS_FILE")
            fi
        fi

        echo "==> Build heartbeat: elapsed=${elapsed_seconds}s [${pct}]"
        echo "    $marker"
        if [[ -f "$BUILD_LOG" ]]; then
            echo "    Recent log:"
            tail -5 "$BUILD_LOG" | while IFS= read -r line; do echo "      $line"; done
        fi
    done
}

trap 'cleanup_monitor; cleanup_lock' EXIT

usage() {
    cat <<'USAGE'
Usage:
  scripts/build_compendium_full.sh [options]

Options:
  --skip-update             Skip `scripts/update_dats.sh --all`
  --allow-unresolved-conflicts
                            Treat exit code 2 (unresolved merge conflicts) as success
  --skip-validation         Skip post-build phase-1 validation SQL
  --online-enrichment         Enable bulk online enrichment (IGDB + RA; uses .env.local credentials)
  --online-enrichment-all     Also run Hasheous/PlayMatch/ZXInfo (very slow on full catalogues)
                              Default: offline-only local DAT/metadata (~90 min builds).
  --dat-dir <path>          DAT directory for manifest generation
  --manifest <path>         Manifest output/input path
  --output-db <path>        Compendium SQLite output path
  --coverage-report <path>  Per-source TSV coverage report path
  --disc-set-coverage-report <path>
                            Per-system disc set topology TSV (from --disc-set-coverage)
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
        --allow-unresolved-conflicts)
            ALLOW_UNRESOLVED_CONFLICTS=true
            shift
            ;;
        --skip-validation)
            SKIP_VALIDATION=true
            shift
            ;;
        --online-enrichment)
            ONLINE_ENRICHMENT=true
            shift
            ;;
        --online-enrichment-all)
            ONLINE_ENRICHMENT=true
            ONLINE_ENRICHMENT_ALL=true
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
        --disc-set-coverage-report)
            DISC_SET_COVERAGE_REPORT="$2"
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
mkdir -p "$(dirname "$DISC_SET_COVERAGE_REPORT")"

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
echo "    disc_set_coverage_report=$DISC_SET_COVERAGE_REPORT"
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

CRED_TARGET="$(dirname "$OUTPUT_DB")/enrichment-credentials.json"
if [[ -x "$ROOT_DIR/scripts/sync_enrichment_credentials.sh" ]]; then
    echo "==> Syncing enrichment-credentials.json from REMUS_* env"
    "$ROOT_DIR/scripts/sync_enrichment_credentials.sh" "$CRED_TARGET" || true
elif [[ ! -f "$CRED_TARGET" ]]; then
    echo "==> Note: $CRED_TARGET not found — IGDB/RA bulk enrichment will use REMUS_* env vars only."
    CRED_EXAMPLE="$ROOT_DIR/data/compendium/enrichment-credentials.json.example"
    if [[ -f "$CRED_EXAMPLE" ]]; then
        echo "    Copy $CRED_EXAMPLE to $CRED_TARGET or run scripts/sync_enrichment_credentials.sh"
    fi
fi

echo "==> Building compendium DB"
if $ONLINE_ENRICHMENT_ALL; then
    echo "    enrichment=online-all (includes Hasheous/PlayMatch/ZXInfo — may take days)"
elif $ONLINE_ENRICHMENT; then
    echo "    enrichment=offline + bulk online (IGDB + RA from .env.local)"
else
    echo "    enrichment=offline-only (local DAT/metadata; add --online-enrichment for IGDB/RA)"
fi
build_started_at=$(date +%s)
set +e
build_cli_args=(
    --build-compendium
    --compendium-manifest "$MANIFEST_PATH"
    --compendium-output "$OUTPUT_DB"
)
if $ONLINE_ENRICHMENT_ALL; then
    build_cli_args+=(--online-enrichment-all)
elif $ONLINE_ENRICHMENT; then
    build_cli_args+=(--online-enrichment)
fi
"$ROOT_DIR/build/remus-cli" \
    "${build_cli_args[@]}" \
    --log-file "$BUILD_LOG" \
    >/dev/null 2>&1 &
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
    if $ALLOW_UNRESOLVED_CONFLICTS; then
        echo "==> Build completed with unresolved conflicts (exit code 2; --allow-unresolved-conflicts)"
    else
        echo "error: compendium build finished with unresolved merge conflicts (exit code 2)" >&2
        echo "hint: resolve conflicts, delete the DB, rebuild, or pass --allow-unresolved-conflicts" >&2
        echo "--- build log tail ---" >&2
        tail -40 "$BUILD_LOG" >&2
        exit 2
    fi
else
    echo "==> Build completed cleanly (exit code 0)"
fi

if [[ -x "$ROOT_DIR/build/remus-cli" ]]; then
    echo "==> Importing patch catalog (libretro hacks DATs)"
    bash "$ROOT_DIR/scripts/import_patch_catalog.sh" "$OUTPUT_DB" \
        || echo "warning: patch catalog import failed or skipped (see above)" >&2
else
    echo "warning: remus-cli not found — skipping patch catalog import" >&2
fi

if ! $SKIP_VALIDATION; then
    bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" "$OUTPUT_DB"
    if [[ -f "$ROOT_DIR/data/compendium/validation/0002_phase2_quality_checks.sql" ]]; then
        echo "==> Phase 2 quality checks (informational thresholds)"
        bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" \
            "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0002_phase2_quality_checks.sql" \
            || echo "warning: one or more phase-2 quality checks failed (see above)" >&2
    fi
    if [[ -f "$ROOT_DIR/data/compendium/validation/0003_phase2_extended_checks.sql" ]]; then
        echo "==> Phase 2 extended checks (informational thresholds)"
        bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" \
            "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0003_phase2_extended_checks.sql" \
            || echo "warning: one or more phase-2 extended checks failed (see above)" >&2
    fi
    if [[ -f "$ROOT_DIR/data/compendium/validation/0004_disc_set_checks.sql" ]]; then
        echo "==> Disc set schema checks (migration 0007)"
        bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" \
            "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0004_disc_set_checks.sql"
    fi
    if [[ -f "$ROOT_DIR/data/compendium/validation/0005_disc_set_ingest_checks.sql" ]]; then
        echo "==> Disc set ingest checks (strict: FAIL only; use --strict to fail on WARN)"
        bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" \
            "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0005_disc_set_ingest_checks.sql" \
            --warn-only
    fi
fi

# Emit a machine-friendly per-source coverage report via remus-cli --coverage-report.
# Columns: source_id, source_items, sigs_owned, games_covered, coverage_pct (TSV)
# sigs_owned    – signature rows attributed to this source (0 for shadowed sources)
# games_covered – games from this source that have any signature (honest ingest coverage)
"$ROOT_DIR/build/remus-cli" --coverage-report --compendium-output "$OUTPUT_DB" > "$COVERAGE_REPORT"

echo "==> Disc set coverage (per-system topology)"
"$ROOT_DIR/build/remus-cli" --disc-set-coverage --compendium-output "$OUTPUT_DB" > "$DISC_SET_COVERAGE_REPORT"
echo "==> Disc set coverage report written: $DISC_SET_COVERAGE_REPORT"
head -12 "$DISC_SET_COVERAGE_REPORT"

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

echo "==> Low-coverage / shadowed sources (top 15 by sig_yield_pct)"
head -17 "$COVERAGE_REPORT"

echo "==> Shadowed enabled sources (sigs_owned=0, items>100)"
awk -F'\t' 'NR>2 && $9==1 {print}' "$COVERAGE_REPORT" | head -15

echo "==> Coverage report written: $COVERAGE_REPORT"

SHADOWED_SUGGESTIONS="${COVERAGE_REPORT%.tsv}.shadowed-suggestions.txt"
if [[ -x "$ROOT_DIR/scripts/audit_shadowed_manifest_sources.sh" ]]; then
    bash "$ROOT_DIR/scripts/audit_shadowed_manifest_sources.sh" "$COVERAGE_REPORT" "$SHADOWED_SUGGESTIONS" \
        || echo "warning: shadowed-source audit failed (see above)" >&2
fi

# Warn about systems that have no games ingested (coverage gaps).
empty_systems=$(sqlite3 "$OUTPUT_DB" "
SELECT display_name FROM systems s
LEFT JOIN games g ON g.system_id = s.system_id
GROUP BY s.system_id
HAVING COUNT(g.game_id) = 0
ORDER BY display_name;
")
if [ -n "$empty_systems" ]; then
    echo "==> [WARNING] Systems with no games (no DAT coverage):"
    echo "$empty_systems" | while IFS= read -r line; do echo "    - $line"; done
fi

echo "==> Build log: $BUILD_LOG"
if [[ "$build_rc" -eq 2 ]] && $ALLOW_UNRESOLVED_CONFLICTS; then
    exit 0
fi
exit "$build_rc"
