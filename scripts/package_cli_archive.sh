#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist}"
BIN_CLI="$BUILD_DIR/remus-cli"

if [[ ! -f "$BIN_CLI" ]]; then
    echo "Missing CLI binary: $BIN_CLI" >&2
    exit 1
fi

VERSION_HEADER="$ROOT_DIR/src/core/constants/api.h"
VERSION="$(sed -nE 's/^inline constexpr const char\* APP_VERSION = "([0-9]+\.[0-9]+\.[0-9]+)";$/\1/p' "$VERSION_HEADER" | head -n 1)"
if [[ -z "$VERSION" ]]; then
    echo "Failed to extract APP_VERSION from $VERSION_HEADER" >&2
    exit 1
fi

PACKAGE_ROOT="$BUILD_DIR/package-cli"
ARCHIVE_STEM="remus-cli-${VERSION}-linux-x64"
ARCHIVE_DIR="$PACKAGE_ROOT/$ARCHIVE_STEM"
ARCHIVE_PATH="$DIST_DIR/${ARCHIVE_STEM}.tar.gz"
CHECKSUM_PATH="$ARCHIVE_PATH.sha256"

rm -rf "$PACKAGE_ROOT"
mkdir -p "$ARCHIVE_DIR" "$DIST_DIR"

cp -a "$BIN_CLI" "$ARCHIVE_DIR/remus-cli"
cp -a "$ROOT_DIR/README.md" "$ARCHIVE_DIR/README.md"
cp -a "$ROOT_DIR/CHANGELOG.md" "$ARCHIVE_DIR/CHANGELOG.md"
cp -a "$ROOT_DIR/VERSION" "$ARCHIVE_DIR/VERSION"

# ── Bundle compendium database ───────────────────────────────────────────────
COMPENDIUM_SRC="$ROOT_DIR/data/compendium/remus_compendium.db"
if [[ -f "$COMPENDIUM_SRC" ]]; then
    mkdir -p "$ARCHIVE_DIR/data/compendium"
    cp -a "$COMPENDIUM_SRC" "$ARCHIVE_DIR/data/compendium/remus_compendium.db"
    echo "Bundled compendium DB: $COMPENDIUM_SRC"
else
    echo "WARNING: compendium DB not found at $COMPENDIUM_SRC — skipping" >&2
fi

if [[ -f "$ROOT_DIR/LICENSE" ]]; then
    cp -a "$ROOT_DIR/LICENSE" "$ARCHIVE_DIR/LICENSE"
fi

tar -C "$PACKAGE_ROOT" -czf "$ARCHIVE_PATH" "$ARCHIVE_STEM"
sha256sum "$ARCHIVE_PATH" > "$CHECKSUM_PATH"

ls -la "$ARCHIVE_PATH" "$CHECKSUM_PATH"