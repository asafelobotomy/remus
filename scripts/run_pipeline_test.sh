#!/usr/bin/env bash
set -euo pipefail

# ──────────────────────────────────────────────────────────────────────
# run_pipeline_test.sh — Formalized pipeline test runner for Remus
#
# Creates a timestamped test directory under test_output/ with
# consistent naming for all artifacts (database, reports, logs).
#
# Usage:
#   ./scripts/run_pipeline_test.sh [OPTIONS]
#
# Options:
#   -i, --input <dir>       ROM input directory       (default: roms/)
#   -l, --label <name>      Human-readable test label (default: "pipeline")
#   -s, --steps <steps>     Pipeline steps to run, comma-separated
#                            (default: scan,stats,list,match,report,enrich,
#                                     verify,export,extract,space-report)
#                            Available: scan, stats, list, match, report,
#                                       enrich, verify, organize, m3u,
#                                       export, extract, space-report
#   -c, --confidence <pct>  Minimum match confidence  (default: 60)
#   -t, --template <tmpl>   Organize template         (default: {NoIntroName})
#   -f, --folder <scheme>   Folder naming scheme      (default: default)
#   --dry-run               Preview organize without moving files
#   -h, --help              Show this help message
#
# Environment variables:
#   REMUS_TMPDIR            Override temp directory used when extracting
#                            archives for hashing (default: system /tmp).
#                            Set to a path on a larger partition when hashing
#                            large disc images (e.g. multi-GB ISO/CHD files)
#                            that would otherwise exhaust /tmp space.
#                            Example: REMUS_TMPDIR=/mnt/data/tmp ./scripts/run_pipeline_test.sh
#
# Output:
#   test_output/full_test_DDMM_HHMM/
#   ├── test.db               Database
#   ├── match-report.txt      Matching confidence report
#   ├── export.json            Exported library (JSON)
#   ├── extracted/             Extracted archive test output
#   ├── pipeline.log          Full pipeline stdout/stderr log
#   ├── summary.txt           Human-readable result summary
#   └── organized/            Organized output (if organize step is run)
# ──────────────────────────────────────────────────────────────────────

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$ROOT_DIR/build/remus-cli"

# ── Defaults ─────────────────────────────────────────────────────────
INPUT_DIR="$ROOT_DIR/roms"
LABEL="pipeline"
STEPS="scan,stats,list,match,report,enrich,verify,export,extract,space-report"
MIN_CONFIDENCE=60
TEMPLATE="{NoIntroName}"
FOLDER_SCHEME="default"
DRY_RUN=""

# ── Parse arguments ──────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--input)       INPUT_DIR="$2"; shift 2 ;;
        -l|--label)       LABEL="$2"; shift 2 ;;
        -s|--steps)       STEPS="$2"; shift 2 ;;
        -c|--confidence)  MIN_CONFIDENCE="$2"; shift 2 ;;
        -t|--template)    TEMPLATE="$2"; shift 2 ;;
        -f|--folder)      FOLDER_SCHEME="$2"; shift 2 ;;
        --dry-run)        DRY_RUN="--dry-run"; shift ;;
        -h|--help)
            sed -n '2,/^# ─\{10,\}$/{ /^# ─\{10,\}$/d; s/^# \?//p; }' "$0"
            exit 0 ;;
        *)
            echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ── Validate prerequisites ───────────────────────────────────────────
if [[ ! -x "$CLI" ]]; then
    echo "ERROR: CLI binary not found at $CLI" >&2
    echo "       Run: cmake --build build -j\$(nproc)" >&2
    exit 1
fi

if [[ ! -d "$INPUT_DIR" ]]; then
    echo "ERROR: Input directory not found: $INPUT_DIR" >&2
    exit 1
fi

ROM_COUNT=$(find "$INPUT_DIR" -maxdepth 2 -type f \( -name "*.zip" -o -name "*.7z" -o -name "*.rar" -o -name "*.z64" -o -name "*.n64" -o -name "*.v64" -o -name "*.sfc" -o -name "*.smc" -o -name "*.md" -o -name "*.bin" -o -name "*.cue" -o -name "*.iso" -o -name "*.cdi" -o -name "*.gdi" -o -name "*.chd" -o -name "*.rvz" -o -name "*.cso" -o -name "*.gcm" -o -name "*.wbfs" -o -name "*.pbp" -o -name "*.img" -o -name "*.mdf" -o -name "*.mds" -o -name "*.ccd" \) 2>/dev/null | wc -l)
if [[ "$ROM_COUNT" -eq 0 ]]; then
    echo "ERROR: No ROM files found in $INPUT_DIR" >&2
    exit 1
fi

# ── Create timestamped output directory ──────────────────────────────
TIMESTAMP=$(date +%d%m_%H%M)
TEST_DIR="$ROOT_DIR/test_output/full_test_${TIMESTAMP}"

# Avoid collision if run twice in the same minute
if [[ -d "$TEST_DIR" ]]; then
    SUFFIX=1
    while [[ -d "${TEST_DIR}_${SUFFIX}" ]]; do
        ((SUFFIX++))
    done
    TEST_DIR="${TEST_DIR}_${SUFFIX}"
fi

mkdir -p "$TEST_DIR"

# ── Consistent artifact paths ────────────────────────────────────────
DB_FILE="$TEST_DIR/test.db"
MATCH_REPORT="$TEST_DIR/match-report.txt"
EXPORT_FILE="$TEST_DIR/export.json"
EXTRACT_DIR="$TEST_DIR/extracted"
PIPELINE_LOG="$TEST_DIR/pipeline.log"
SUMMARY_FILE="$TEST_DIR/summary.txt"
ORGANIZE_DIR="$TEST_DIR/organized"
M3U_DIR="$TEST_DIR/m3u"

# ── Helper: check if a step is enabled ───────────────────────────────
step_enabled() { [[ ",$STEPS," == *",$1,"* ]]; }

# ── Write header to summary + log ───────────────────────────────────
write_header() {
    local file="$1"
    cat >> "$file" <<EOF
═══════════════════════════════════════════════════════
  Remus Pipeline Test — ${LABEL}
  Timestamp: $(date -Iseconds)
  Input: ${INPUT_DIR}
  Steps: ${STEPS}
  Confidence: ${MIN_CONFIDENCE}%
  Output: ${TEST_DIR}
  ROMs: ${ROM_COUNT} file(s)
═══════════════════════════════════════════════════════

EOF
}

write_header "$SUMMARY_FILE"
write_header "$PIPELINE_LOG"

echo "╔════════════════════════════════════════════════════╗"
echo "║  Remus Pipeline Test Runner                       ║"
echo "╚════════════════════════════════════════════════════╝"
echo ""
echo "  Label:      $LABEL"
echo "  Input:      $INPUT_DIR ($ROM_COUNT ROMs)"
echo "  Steps:      $STEPS"
echo "  Confidence: ${MIN_CONFIDENCE}%"
echo "  Output:     $TEST_DIR"
echo ""

# ── Track overall timing ─────────────────────────────────────────────
PIPELINE_START=$(date +%s)
STEP_RESULTS=()

run_step() {
    local name="$1"
    shift
    echo "── Step: $name ──────────────────────────────"
    local step_start
    step_start=$(date +%s)

    echo "" >> "$PIPELINE_LOG"
    echo "── Step: $name ($(date -Iseconds)) ──" >> "$PIPELINE_LOG"

    if "$@" >> "$PIPELINE_LOG" 2>&1; then
        local elapsed=$(( $(date +%s) - step_start ))
        echo "  ✓ $name completed (${elapsed}s)"
        STEP_RESULTS+=("✓ $name (${elapsed}s)")
        echo "  ✓ Completed in ${elapsed}s" >> "$PIPELINE_LOG"
        return 0
    else
        local rc=$?
        local elapsed=$(( $(date +%s) - step_start ))
        echo "  ✗ $name FAILED (exit $rc, ${elapsed}s)"
        STEP_RESULTS+=("✗ $name FAILED (exit $rc, ${elapsed}s)")
        echo "  ✗ FAILED exit=$rc in ${elapsed}s" >> "$PIPELINE_LOG"
        return $rc
    fi
}

# ── Step: scan ───────────────────────────────────────────────────────
if step_enabled "scan"; then
    run_step "scan+hash" "$CLI" --scan "$INPUT_DIR" --db "$DB_FILE" --hash
fi

# ── Step: stats ──────────────────────────────────────────────────────
if step_enabled "stats"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    run_step "stats" "$CLI" --stats --db "$DB_FILE"
fi

# ── Step: list ───────────────────────────────────────────────────────
if step_enabled "list"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    run_step "list" "$CLI" --list --db "$DB_FILE"
fi

# ── Step: match ──────────────────────────────────────────────────────
if step_enabled "match"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    run_step "match" "$CLI" --match --db "$DB_FILE" --min-confidence "$MIN_CONFIDENCE"
fi

# ── Step: report ─────────────────────────────────────────────────────
if step_enabled "report"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    run_step "match-report" "$CLI" --match-report --db "$DB_FILE" \
        --report-file "$MATCH_REPORT"
fi

# ── Step: enrich ─────────────────────────────────────────────────────
if step_enabled "enrich"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    run_step "enrich" "$CLI" --enrich --db "$DB_FILE"
fi

# ── Step: verify ─────────────────────────────────────────────────────
if step_enabled "verify"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    # Get list of system names that have scanned files
    SCANNED_SYSTEMS=$(sqlite3 "$DB_FILE" "
        SELECT DISTINCT s.name FROM files f
        JOIN systems s ON f.system_id = s.id
        WHERE f.is_primary = 1;" 2>/dev/null)

    DAT_DIR="$ROOT_DIR/data/databases"
    VERIFIED_ANY=false
    for dat_file in "$DAT_DIR"/*.dat; do
        [[ -f "$dat_file" ]] || continue
        dat_basename="$(basename "$dat_file" .dat)"
        # Only verify DATs for systems that have scanned files
        matched_system=false
        while IFS= read -r sys; do
            if [[ "$dat_basename" == *"$sys"* ]] || [[ "$sys" == *"$dat_basename"* ]]; then
                matched_system=true
                break
            fi
        done <<< "$SCANNED_SYSTEMS"
        if [[ "$matched_system" == true ]]; then
            run_step "verify [$(basename "$dat_file")]" "$CLI" --verify "$dat_file" \
                --db "$DB_FILE" --verify-report || true
            VERIFIED_ANY=true
        fi
    done
    if [[ "$VERIFIED_ANY" == false ]]; then
        echo "  ⚠ No DATs matched scanned systems — trying all"
        # Fallback: try all DATs but don't fail
        for dat_file in "$DAT_DIR"/*.dat; do
            [[ -f "$dat_file" ]] || continue
            run_step "verify [$(basename "$dat_file")]" "$CLI" --verify "$dat_file" \
                --db "$DB_FILE" --verify-report || true
        done
    fi
fi

# ── Step: export ─────────────────────────────────────────────────────
if step_enabled "export"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    run_step "export-json" "$CLI" --export json --export-path "$EXPORT_FILE" --db "$DB_FILE"
fi

# ── Step: extract (test archive extraction) ──────────────────────────
if step_enabled "extract"; then
    # Pick the first archive file in the input dir and test extraction
    FIRST_ARCHIVE=$(find "$INPUT_DIR" -maxdepth 1 -type f \( -name "*.zip" -o -name "*.7z" \) | head -1)
    if [[ -n "$FIRST_ARCHIVE" ]]; then
        mkdir -p "$EXTRACT_DIR"
        run_step "extract-archive" "$CLI" --extract-archive "$FIRST_ARCHIVE" \
            --output-dir "$EXTRACT_DIR" || true
    else
        echo "  ⚠ No archives found — skipping extract step"
    fi
fi

# ── Step: space-report ───────────────────────────────────────────────
if step_enabled "space-report"; then
    run_step "space-report" "$CLI" --space-report "$INPUT_DIR" --db "$DB_FILE" || true
fi

# ── Step: organize ───────────────────────────────────────────────────
if step_enabled "organize"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    mkdir -p "$ORGANIZE_DIR"
    run_step "organize" "$CLI" --organize "$ORGANIZE_DIR" --db "$DB_FILE" \
        --template "$TEMPLATE" --folder-naming "$FOLDER_SCHEME" $DRY_RUN
fi

# ── Step: m3u (multi-disc playlists) ─────────────────────────────────
if step_enabled "m3u"; then
    if [[ ! -f "$DB_FILE" ]]; then
        echo "ERROR: Database not found — scan step must run first" >&2
        exit 1
    fi
    mkdir -p "$M3U_DIR"
    run_step "generate-m3u" "$CLI" --generate-m3u --m3u-dir "$M3U_DIR" --db "$DB_FILE" || true
fi

# ── Generate summary ─────────────────────────────────────────────────
PIPELINE_ELAPSED=$(( $(date +%s) - PIPELINE_START ))

{
    echo "── Step Results ──"
    for r in "${STEP_RESULTS[@]}"; do
        echo "  $r"
    done
    echo ""
    echo "Total time: ${PIPELINE_ELAPSED}s"
    echo ""

    if [[ -f "$DB_FILE" ]]; then
        echo "── Database Stats ──"
        sqlite3 "$DB_FILE" "SELECT COUNT(*) || ' files scanned' FROM files;"
        sqlite3 "$DB_FILE" "SELECT COUNT(*) || ' files with hashes' FROM files WHERE hash_calculated = 1;"
        sqlite3 "$DB_FILE" "SELECT COUNT(*) || ' matches found' FROM matches;"
        echo ""

        echo "── Files by System ──"
        sqlite3 "$DB_FILE" "
            SELECT COALESCE(s.name, 'Unknown') || ': ' || COUNT(*)
            FROM files f
            LEFT JOIN systems s ON f.system_id = s.id
            WHERE f.is_primary = 1
            GROUP BY s.name
            ORDER BY COUNT(*) DESC;"
        echo ""

        echo "── Match Details ──"
        sqlite3 "$DB_FILE" "
            SELECT f.filename || ' → ' || g.title || ' (' || CAST(m.confidence AS INTEGER) || '%, ' || m.match_method || ')'
            FROM matches m
            JOIN files f ON m.file_id = f.id
            JOIN games g ON m.game_id = g.id
            ORDER BY f.filename;" 2>/dev/null || echo "  (no matches or schema mismatch)"
        echo ""

        echo "── Per-Game Metadata ──"
        sqlite3 -header -column "$DB_FILE" "
            SELECT
                g.title AS 'Game',
                COALESCE(s.name, '') AS 'System',
                COALESCE(g.publisher, '') AS 'Publisher',
                COALESCE(g.developer, '') AS 'Developer',
                COALESCE(g.release_date, '') AS 'Year',
                COALESCE(g.players, '') AS 'Players',
                COALESCE(g.genres, '') AS 'Genres',
                COALESCE(g.region, '') AS 'Region',
                m.match_method AS 'Method',
                CAST(m.confidence AS INTEGER) || '%' AS 'Score'
            FROM matches m
            JOIN games g ON m.game_id = g.id
            LEFT JOIN systems s ON g.system_id = s.id
            ORDER BY g.title;" 2>/dev/null || echo "  (no game metadata or schema mismatch)"
        echo ""

        echo "── Metadata Coverage ──"
        sqlite3 "$DB_FILE" "
            SELECT
                COUNT(*) || ' games matched' AS stat
            FROM matches m JOIN games g ON m.game_id = g.id
            UNION ALL SELECT
                COUNT(*) || ' with publisher' FROM matches m JOIN games g ON m.game_id = g.id WHERE g.publisher != ''
            UNION ALL SELECT
                COUNT(*) || ' with developer' FROM matches m JOIN games g ON m.game_id = g.id WHERE g.developer != ''
            UNION ALL SELECT
                COUNT(*) || ' with release year' FROM matches m JOIN games g ON m.game_id = g.id WHERE g.release_date != ''
            UNION ALL SELECT
                COUNT(*) || ' with players' FROM matches m JOIN games g ON m.game_id = g.id WHERE g.players != '' AND g.players IS NOT NULL
            UNION ALL SELECT
                COUNT(*) || ' with genres' FROM matches m JOIN games g ON m.game_id = g.id WHERE g.genres != ''
            ;" 2>/dev/null || echo "  (schema mismatch)"
        echo ""

        echo "── File Hashing ──"
        sqlite3 "$DB_FILE" "
            SELECT f.filename || ' (' || f.extension || ', ' ||
                CASE WHEN f.file_size > 1048576 THEN (f.file_size / 1048576) || ' MB'
                     WHEN f.file_size > 1024 THEN (f.file_size / 1024) || ' KB'
                     ELSE f.file_size || ' B' END ||
                ', primary=' || f.is_primary ||
                ', crc=' || COALESCE(f.crc32, 'NONE') || ')'
            FROM files f
            ORDER BY f.filename;" 2>/dev/null || echo "  (no files)"
        echo ""
    fi

    if [[ -f "$MATCH_REPORT" ]]; then
        echo "── Match Report ──"
        cat "$MATCH_REPORT"
        echo ""
    fi

    if [[ -f "$EXPORT_FILE" ]]; then
        echo "── Export Preview (first 50 lines) ──"
        head -50 "$EXPORT_FILE"
        echo ""
    fi

    if [[ -d "$EXTRACT_DIR" ]] && [[ "$(find "$EXTRACT_DIR" -type f | wc -l)" -gt 0 ]]; then
        echo "── Extracted Files ──"
        find "$EXTRACT_DIR" -type f -printf '  %P (%s bytes)\n' | sort
        echo ""
    fi

    echo "── Artifacts ──"
    find "$TEST_DIR" -type f -printf '  %P (%s bytes)\n' | sort
} >> "$SUMMARY_FILE"

# ── Print summary to terminal ────────────────────────────────────────
echo ""
echo "╔════════════════════════════════════════════════════╗"
echo "║  Test Complete                                    ║"
echo "╚════════════════════════════════════════════════════╝"
echo ""
for r in "${STEP_RESULTS[@]}"; do
    echo "  $r"
done
echo ""
echo "  Total time: ${PIPELINE_ELAPSED}s"
echo "  Output:     $TEST_DIR"
echo ""
echo "  Artifacts:"
find "$TEST_DIR" -type f -printf '    %P\n' | sort
echo ""

# ── Prune old test runs (keep most recent 5) ─────────────────────────
OLD_RUNS=$(find "$ROOT_DIR/test_output" -maxdepth 1 -type d -name "full_test_*" | sort -r | tail -n +6)
if [[ -n "$OLD_RUNS" ]]; then
    echo "  Pruning old test runs (keeping 5 most recent):"
    echo "$OLD_RUNS" | while read -r dir; do
        echo "    Removing: $(basename "$dir")"
        rm -rf "$dir"
    done
    echo ""
fi
