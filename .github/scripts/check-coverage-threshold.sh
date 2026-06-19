#!/usr/bin/env bash
# Fail when line coverage in an lcov .info file falls below COVERAGE_THRESHOLD (default 54).
# CI captures ~54–55% line coverage with the current test suite; local build-coverage
# totals can read higher due to a wider geninfo scope.
set -euo pipefail

COVERAGE_INFO="${1:-build/coverage.info}"
THRESHOLD="${COVERAGE_THRESHOLD:-54}"

if [[ ! -f "$COVERAGE_INFO" ]]; then
    echo "Coverage file not found: $COVERAGE_INFO" >&2
    exit 1
fi

SUMMARY="$(lcov --summary "$COVERAGE_INFO" --ignore-errors inconsistent 2>&1)"
LINE="$(printf '%s\n' "$SUMMARY" | grep -E '^[[:space:]]*lines\.*:' | head -1 || true)"
if [[ -z "$LINE" ]]; then
    echo "Could not parse line coverage from lcov summary:" >&2
    printf '%s\n' "$SUMMARY" >&2
    exit 1
fi

PCT="$(printf '%s\n' "$LINE" | sed -E 's/.*: ([0-9.]+)%.*/\1/')"
if [[ -z "$PCT" ]]; then
    echo "Could not extract coverage percentage from: $LINE" >&2
    exit 1
fi

awk -v pct="$PCT" -v threshold="$THRESHOLD" 'BEGIN {
    if (pct + 0 < threshold + 0) {
        printf("Line coverage %.1f%% is below threshold %d%%\n", pct, threshold) > "/dev/stderr"
        exit 1
    }
    printf("Line coverage %.1f%% meets threshold %d%%\n", pct, threshold)
}'
