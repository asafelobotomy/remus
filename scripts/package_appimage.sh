#!/usr/bin/env bash
# Package Remus CLI as a Linux AppImage.
#
# Prerequisites (installed by CI or manually):
#   - linuxdeploy (https://github.com/linuxdeploy/linuxdeploy)
#   - linuxdeploy-plugin-qt (for Qt library bundling)
#
# Usage:
#   scripts/package_appimage.sh          # uses ./build as default
#   BUILD_DIR=mybuild scripts/package_appimage.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist}"
BIN_CLI="$BUILD_DIR/remus-cli"

if [[ ! -f "$BIN_CLI" ]]; then
    echo "Missing CLI binary: $BIN_CLI" >&2
    echo "Run: cmake --build build --config Release" >&2
    exit 1
fi

# ── Extract version ──────────────────────────────────────────────────────────
VERSION_HEADER="$ROOT_DIR/src/core/constants/api.h"
VERSION="$(sed -nE 's/^inline constexpr const char\* APP_VERSION = "([0-9]+\.[0-9]+\.[0-9]+)";$/\1/p' "$VERSION_HEADER" | head -n 1)"
if [[ -z "$VERSION" ]]; then
    echo "Failed to extract APP_VERSION from $VERSION_HEADER" >&2
    exit 1
fi
export VERSION

# ── Prepare AppDir layout ───────────────────────────────────────────────────
APPDIR="$BUILD_DIR/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp -a "$BIN_CLI" "$APPDIR/usr/bin/remus-cli"
cp -a "$ROOT_DIR/assets/remus.desktop" "$APPDIR/usr/share/applications/remus.desktop"
cp -a "$ROOT_DIR/assets/icon/remus_icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png"

# Symlink desktop and icon at AppDir root (required by linuxdeploy)
ln -sf usr/share/applications/remus.desktop "$APPDIR/remus.desktop"
ln -sf usr/share/icons/hicolor/256x256/apps/remus.png "$APPDIR/remus.png"

# ── Bundle shared libraries ─────────────────────────────────────────────────
LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"
if ! command -v "$LINUXDEPLOY" &>/dev/null; then
    # Try the downloaded AppImage in BUILD_DIR
    if [[ -x "$BUILD_DIR/linuxdeploy-x86_64.AppImage" ]]; then
        LINUXDEPLOY="$BUILD_DIR/linuxdeploy-x86_64.AppImage"
    else
        echo "linuxdeploy not found. Install it or set LINUXDEPLOY variable." >&2
        exit 1
    fi
fi

mkdir -p "$DIST_DIR"
OUTPUT="$DIST_DIR/Remus-${VERSION}-x86_64.AppImage"
export OUTPUT

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/remus.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png" \
    --output appimage

# ── Checksum ────────────────────────────────────────────────────────────────
if [[ -f "$OUTPUT" ]]; then
    sha256sum "$OUTPUT" > "$OUTPUT.sha256"
    ls -lh "$OUTPUT" "$OUTPUT.sha256"
    echo "AppImage created: $OUTPUT"
else
    echo "AppImage creation failed" >&2
    exit 1
fi
