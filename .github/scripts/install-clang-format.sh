#!/usr/bin/env bash
# Install a pinned clang-format for CI (must match .clang-format-version).
set -euo pipefail

CLANG_FORMAT_VERSION="${CLANG_FORMAT_VERSION:-22}"
CLANG_FORMAT_BIN="/usr/bin/clang-format-${CLANG_FORMAT_VERSION}"

sudo apt-get update
sudo apt-get install -y wget gnupg lsb-release software-properties-common

if ! apt-cache show "clang-format-${CLANG_FORMAT_VERSION}" >/dev/null 2>&1; then
  wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
  chmod +x /tmp/llvm.sh
  sudo /tmp/llvm.sh "${CLANG_FORMAT_VERSION}"
fi

sudo apt-get install -y "clang-format-${CLANG_FORMAT_VERSION}"

if [[ ! -x "${CLANG_FORMAT_BIN}" ]]; then
  echo "Expected formatter binary missing: ${CLANG_FORMAT_BIN}" >&2
  exit 1
fi

"${CLANG_FORMAT_BIN}" --version
