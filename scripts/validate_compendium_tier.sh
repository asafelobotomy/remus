#!/usr/bin/env bash
# Run a named validation tier against a compendium DB (single flock acquisition).
#
# Usage:
#   scripts/validate_compendium_tier.sh <tier> [db_path]
#
# Tiers:
#   bootstrap    — 0000 (schema+seeds)
#   phase1       — 0001 (strict)
#   quality      — 0002 (warn)
#   extended     — 0003 (warn)
#   ci           — 0001, 0004, 0006 (strict) + 0005 (warn-only)
#   artwork      — 0013 artwork coverage (warn-only)
#   disc_ingest  — 0005 (warn-only)
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TIER="${1:-}"
DB_PATH="${2:-$ROOT_DIR/data/compendium/remus_compendium.db}"
VALIDATION_DIR="$ROOT_DIR/data/compendium/validation"

usage() {
    cat <<EOF
usage: $(basename "$0") <tier> [db_path]

Tiers: bootstrap, phase1, quality, extended, ci, artwork, disc_ingest
EOF
}

if [[ -z "$TIER" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo "error: database not found: $DB_PATH" >&2
    exit 1
fi

run_validate() {
    local sql_file="$1"
    shift
    if [[ ! -f "$sql_file" ]]; then
        echo "error: missing validation SQL: $sql_file" >&2
        exit 1
    fi
    bash "$ROOT_DIR/scripts/run_compendium_job.sh" --db "$DB_PATH" -- \
        bash "$ROOT_DIR/.github/scripts/validate-compendium-db.sh" "$DB_PATH" "$sql_file" "$@"
}

# Post-build paths apply incremental migrations first (skip for bootstrap-only).
if [[ "$TIER" != "bootstrap" ]]; then
    bash "$ROOT_DIR/scripts/run_compendium_job.sh" --db "$DB_PATH" -- \
        bash "$ROOT_DIR/scripts/apply_compendium_migrations.sh" "$DB_PATH"
fi

case "$TIER" in
    bootstrap)
        run_validate "$VALIDATION_DIR/0000_bootstrap_checks.sql"
        ;;
    phase1)
        run_validate "$VALIDATION_DIR/0001_phase1_checks.sql"
        ;;
    quality)
        run_validate "$VALIDATION_DIR/0002_phase2_quality_checks.sql" \
            || echo "warning: one or more phase-2 quality checks failed (see above)" >&2
        ;;
    extended)
        run_validate "$VALIDATION_DIR/0003_phase2_extended_checks.sql" \
            || echo "warning: one or more phase-2 extended checks failed (see above)" >&2
        ;;
    ci)
        bash "$ROOT_DIR/scripts/run_compendium_job.sh" --db "$DB_PATH" -- bash -c "
            set -euo pipefail
            VALIDATE='$ROOT_DIR/.github/scripts/validate-compendium-db.sh'
            VD='$VALIDATION_DIR'
            bash \"\$VALIDATE\" '$DB_PATH' \"\$VD/0001_phase1_checks.sql\"
            bash \"\$VALIDATE\" '$DB_PATH' \"\$VD/0004_disc_set_checks.sql\"
            bash \"\$VALIDATE\" '$DB_PATH' \"\$VD/0006_enabled_source_gate.sql\"
            bash \"\$VALIDATE\" '$DB_PATH' \"\$VD/0005_disc_set_ingest_checks.sql\" --warn-only
        "
        ;;
    artwork)
        run_validate "$VALIDATION_DIR/0013_artwork_coverage.sql" --warn-only \
            || echo "warning: one or more artwork coverage checks failed (see above)" >&2
        ;;
    disc_ingest)
        run_validate "$VALIDATION_DIR/0005_disc_set_ingest_checks.sql" --warn-only
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        echo "error: unknown tier: $TIER" >&2
        usage >&2
        exit 1
        ;;
esac

echo "==> Validation tier '$TIER' completed for $DB_PATH"
