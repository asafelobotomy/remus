#!/usr/bin/env bash
# Install a pinned clang-format for CI (must match .clang-format-version).
set -euo pipefail

CLANG_FORMAT_VERSION="${CLANG_FORMAT_VERSION:-22}"

sudo apt-get update
sudo apt-get install -y wget gnupg lsb-release software-properties-common

if ! apt-cache show "clang-format-${CLANG_FORMAT_VERSION}" >/dev/null 2>&1; then
  wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
  chmod +x /tmp/llvm.sh
  sudo /tmp/llvm.sh "${CLANG_FORMAT_VERSION}"
fi

sudo apt-get install -y "clang-format-${CLANG_FORMAT_VERSION}"
sudo update-alternatives --install /usr/bin/clang-format clang-format \
  "/usr/bin/clang-format-${CLANG_FORMAT_VERSION}" 100

clang-format --version
