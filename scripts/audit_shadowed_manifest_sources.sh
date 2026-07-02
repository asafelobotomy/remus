#!/usr/bin/env bash
# purpose: list enabled manifest sources that ingest items but own zero signatures (shadowed).
# when: run after build_compendium_full.sh coverage report generation.
# inputs: coverage TSV from remus-cli --coverage-report
# outputs: human-readable audit on stdout; optional suggestions file for manifest exclusions.
# risk: safe (read-only)

set -euo pipefail

COVERAGE_TSV="${1:-}"
SUGGESTIONS_OUT="${2:-}"

if [[ -z "$COVERAGE_TSV" || ! -f "$COVERAGE_TSV" ]]; then
    echo "usage: $0 <coverage.tsv> [suggestions.txt]" >&2
    exit 1
fi

# Columns (TSV): source_id, enabled, priority, source_items, sigs_owned, games_covered,
# coverage_pct, sig_yield_pct, shadowed
shadowed="$(awk -F'\t' 'NR>2 && $9==1 {print $1 "\t" $4 "\t" $5}' "$COVERAGE_TSV" | sort -u)"

if [[ -z "$shadowed" ]]; then
    echo "No persistently shadowed enabled sources (sigs_owned=0, items>100)."
    exit 0
fi

echo "==> Shadowed enabled sources (consider disabling in generate_compendium_manifest.sh EXCLUDED_SOURCE_IDS):"
echo "$shadowed" | while IFS=$'\t' read -r source_id items sigs_owned; do
    echo "  - $source_id  items=$items  sigs_owned=$sigs_owned"
done

if [[ -n "$SUGGESTIONS_OUT" ]]; then
    {
        echo "# Suggested additions to EXCLUDED_SOURCE_IDS in scripts/generate_compendium_manifest.sh"
        echo "# Generated from $(basename "$COVERAGE_TSV") on $(date -u +%Y-%m-%dT%H:%MZ)"
        echo "$shadowed" | while IFS=$'\t' read -r source_id _ _; do
            printf '    "%s"  # shadowed: sigs_owned=0\n' "$source_id"
        done
    } >"$SUGGESTIONS_OUT"
    echo "==> Suggestions written: $SUGGESTIONS_OUT"
fi
