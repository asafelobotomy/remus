#!/usr/bin/env bash
# Print APP_VERSION from src/core/constants/api.h (single source of truth).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HEADER="${ROOT}/src/core/constants/api.h"

VERSION="$(
  sed -nE 's/^[[:space:]]*inline constexpr const char \*APP_VERSION = "([0-9]+\.[0-9]+\.[0-9]+)";/\1/p' \
    "$HEADER" | head -n1
)"

if [[ -z "$VERSION" ]]; then
  echo "Failed to extract APP_VERSION from $HEADER" >&2
  exit 1
fi

echo "$VERSION"
