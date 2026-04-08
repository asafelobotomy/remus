#!/usr/bin/env bash
# purpose:  Prune a test output directory back to a minimal kept set of top-level items.
# when:     Invoke after validation runs to remove transient artifacts; do not invoke when you still need raw logs, databases, or extracted media.
# inputs:   --target <path> optional target directory, --keep <name> repeatable top-level names to preserve, --apply to delete, --dry-run to preview, --help for usage.
# outputs:  Prints the kept items, removal candidates, and whether anything was deleted.
# risk:     destructive
# source:   original

set -euo pipefail

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TARGET_DIR="$ROOT_DIR/test_output"
MODE="dry-run"

KEEP_NAMES=("README.md")

usage() {
    cat <<'EOF'
Usage: ./scripts/prune_test_output.sh [options]

Prune a test output directory back to a minimal kept set of top-level items.

Options:
  --target <path>   Target directory to prune (default: ./test_output)
  --keep <name>     Preserve an additional top-level file or directory name
  --apply           Delete matching items
  --dry-run         Preview changes only (default)
  --help            Show this help text

Examples:
  ./scripts/prune_test_output.sh --dry-run
  ./scripts/prune_test_output.sh --keep VALIDATION_SUMMARY_2026-04-05.md --dry-run
  ./scripts/prune_test_output.sh --apply
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)
            [[ $# -ge 2 ]] || { echo "Missing value for --target" >&2; exit 1; }
            TARGET_DIR="$2"
            shift 2
            ;;
        --keep)
            [[ $# -ge 2 ]] || { echo "Missing value for --keep" >&2; exit 1; }
            KEEP_NAMES+=("$2")
            shift 2
            ;;
        --apply)
            MODE="apply"
            shift
            ;;
        --dry-run)
            MODE="dry-run"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ ! -d "$TARGET_DIR" ]]; then
    echo "Target directory does not exist: $TARGET_DIR" >&2
    exit 1
fi

declare -A KEEP_SET=()
for name in "${KEEP_NAMES[@]}"; do
    KEEP_SET["$name"]=1
done

mapfile -t ALL_ENTRIES < <(find "$TARGET_DIR" -mindepth 1 -maxdepth 1 -printf '%f\n' | sort)
REMOVALS=()

for entry in "${ALL_ENTRIES[@]}"; do
    if [[ -n "${KEEP_SET[$entry]:-}" ]]; then
        continue
    fi
    REMOVALS+=("$entry")
done

echo "Target: $TARGET_DIR"
echo "Mode: $MODE"
echo "Keep:"
for name in "${KEEP_NAMES[@]}"; do
    echo "  - $name"
done

if [[ ${#REMOVALS[@]} -eq 0 ]]; then
    echo "Nothing to remove."
    exit 0
fi

echo "Candidates:"
for entry in "${REMOVALS[@]}"; do
    echo "  - $entry"
done

if [[ "$MODE" == "dry-run" ]]; then
    echo "Dry run only. Re-run with --apply to delete these items."
    exit 0
fi

for entry in "${REMOVALS[@]}"; do
    rm -rf -- "${TARGET_DIR:?}/$entry"
done

echo "Removed ${#REMOVALS[@]} item(s)."
