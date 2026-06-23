#!/usr/bin/env bash
# Run shellcheck on repository shell scripts (audit: supply-chain hygiene).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

mapfile -t scripts < <(
    find .github/scripts scripts -name '*.sh' -type f | sort
)

if [[ "${#scripts[@]}" -eq 0 ]]; then
    echo "No shell scripts found to check." >&2
    exit 1
fi

shellcheck --severity=warning "${scripts[@]}"

echo "==> Syntax-check build_compendium_full.sh"
bash -n scripts/build_compendium_full.sh
