#!/usr/bin/env bash
# Spot-check core library headers with clang-tidy (informational; does not gate merge yet).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build-clang-tidy}"
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DREMUS_ENABLE_WARNINGS=ON \
    -Wno-dev

mapfile -t sources < <(
    find src/core -name '*.cpp' -type f | sort | head -20
)

if [[ "${#sources[@]}" -eq 0 ]]; then
    echo "No core sources found for clang-tidy." >&2
    exit 1
fi

clang-tidy -p "$BUILD_DIR" "${sources[@]}"
