#!/usr/bin/env bash
# Remove local build trees, generated databases, and prune disposable test output.
# Safe to run anytime — only deletes gitignored or explicitly disposable artifacts.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

usage() {
    cat <<'EOF'
Usage: ./scripts/clean-workspace.sh [options]

Options:
  --all-builds     Also remove ./build (default keeps the primary Release tree)
  --dry-run        Print what would be removed without deleting
  --help           Show this help text

Removes by default:
  - build-coverage/, build-asan/, build-tidy/
  - remus.db, build/remus.db, data/compendium/remus_compendium.db
  - transient files under test_output/ (via prune_test_output.sh)

Regenerate compendium bootstrap DB:
  ./scripts/setup_compendium_db.sh
EOF
}

DRY_RUN=0
CLEAN_ALL_BUILDS=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all-builds) CLEAN_ALL_BUILDS=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

remove_path() {
    local path="$1"
    if [[ ! -e "$path" ]]; then
        return 0
    fi
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "would remove: $path"
    else
        echo "Removing $path ..."
        rm -rf -- "$path"
    fi
}

echo "==> Workspace cleanup (root: $ROOT_DIR)"

for dir in build-coverage build-asan build-tidy; do
    remove_path "$dir"
done

if [[ "$CLEAN_ALL_BUILDS" -eq 1 ]]; then
    remove_path build
fi

for db in remus.db build/remus.db data/compendium/remus_compendium.db; do
    remove_path "$db"
done

if [[ "$DRY_RUN" -eq 0 ]] && [[ -x scripts/prune_test_output.sh ]]; then
    scripts/prune_test_output.sh --keep attention.log --apply
elif [[ "$DRY_RUN" -eq 1 ]]; then
    scripts/prune_test_output.sh --keep attention.log --dry-run 2>/dev/null || true
fi

if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "Dry run complete."
else
    echo "==> Done."
    if [[ "$CLEAN_ALL_BUILDS" -eq 1 ]]; then
        echo "All build trees removed."
    else
        echo "Primary build retained at: ./build"
    fi
    echo "Rebuild: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j\$(nproc)"
fi
