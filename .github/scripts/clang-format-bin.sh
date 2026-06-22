#!/usr/bin/env bash
# Resolve the pinned clang-format binary (see .clang-format-version).
# Debian/Ubuntu CI installs clang-format-22; Arch/CachyOS ship versioned clang-format.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PINNED="$(tr -d '[:space:]' < "$ROOT_DIR/.clang-format-version")"

clang_format_major() {
    local bin="$1"
    "$bin" --version 2>/dev/null | sed -n 's/.*version \([0-9][0-9]*\).*/\1/p' | head -1
}

resolve_clang_format() {
    local candidate major
    for candidate in \
        "/usr/bin/clang-format-${PINNED}" \
        "clang-format-${PINNED}" \
        "clang-format"; do
        if command -v "$candidate" &>/dev/null; then
            candidate="$(command -v "$candidate")"
            major="$(clang_format_major "$candidate")"
            if [[ "$major" == "$PINNED" ]]; then
                echo "$candidate"
                return 0
            fi
        fi
    done
    return 1
}

if ! CLANG_FORMAT_BIN="$(resolve_clang_format)"; then
    echo "clang-format ${PINNED} not found on PATH." >&2
    echo "  Debian/Ubuntu: bash .github/scripts/install-clang-format.sh" >&2
    echo "  Arch/CachyOS:  sudo pacman -S clang   # provides /usr/bin/clang-format" >&2
    exit 127
fi

exec "$CLANG_FORMAT_BIN" "$@"
