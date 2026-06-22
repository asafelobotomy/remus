#!/usr/bin/env bash
# Configure the debug preset and refresh compile_commands.json for clangd.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

cmake --preset debug "$@"
bash .github/scripts/sanitize-compile-commands.sh build-debug
