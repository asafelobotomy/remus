#!/usr/bin/env bash
# Package Remus CLI + Qt Quick GUI as a Linux AppImage.
#
# Prerequisites (installed by CI or manually):
#   - linuxdeploy (https://github.com/linuxdeploy/linuxdeploy)
#   - linuxdeploy-plugin-qt (for Qt/QML bundling)
#
# Usage:
#   scripts/package_appimage.sh          # uses ./build as default
#   BUILD_DIR=mybuild scripts/package_appimage.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist}"
BIN_CLI="$BUILD_DIR/remus-cli"
BIN_GUI="$BUILD_DIR/src/gui/remus-gui"

if [[ ! -f "$BIN_CLI" ]]; then
    echo "Missing CLI binary: $BIN_CLI" >&2
    echo "Run: cmake --build build --config Release" >&2
    exit 1
fi

if [[ ! -f "$BIN_GUI" ]]; then
    echo "Missing GUI binary: $BIN_GUI" >&2
    echo "Run: cmake --build build --config Release" >&2
    exit 1
fi

# ── Extract version ──────────────────────────────────────────────────────────
VERSION="$(bash "$ROOT_DIR/.github/scripts/read-app-version.sh")"
export VERSION

# ── Prepare AppDir layout ───────────────────────────────────────────────────
APPDIR="$BUILD_DIR/AppDir"
trap 'rm -rf "${APPDIR:-}"' EXIT
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp -a "$BIN_CLI" "$APPDIR/usr/bin/remus-cli"
cp -a "$BIN_GUI" "$APPDIR/usr/bin/remus-gui"
cp -a "$ROOT_DIR/assets/remus.desktop" "$APPDIR/usr/share/applications/remus.desktop"
cp -a "$ROOT_DIR/assets/icon/remus_icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png"

# ── Bundle compendium database ───────────────────────────────────────────────
COMPENDIUM_SRC="$ROOT_DIR/data/compendium/remus_compendium.db"
if [[ -f "$COMPENDIUM_SRC" ]]; then
    mkdir -p "$APPDIR/usr/share/remus/data/compendium"
    cp -a "$COMPENDIUM_SRC" "$APPDIR/usr/share/remus/data/compendium/remus_compendium.db"
    echo "Bundled compendium DB: $COMPENDIUM_SRC"
else
    echo "WARNING: compendium DB not found at $COMPENDIUM_SRC — skipping" >&2
fi

# Symlink desktop and icon at AppDir root (required by linuxdeploy)
ln -sf usr/share/applications/remus.desktop "$APPDIR/remus.desktop"
ln -sf usr/share/icons/hicolor/256x256/apps/remus.png "$APPDIR/remus.png"

# ── Bundle shared libraries ─────────────────────────────────────────────────
LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"
if ! command -v "$LINUXDEPLOY" &>/dev/null; then
    if [[ -x "$BUILD_DIR/linuxdeploy-x86_64.AppImage" ]]; then
        LINUXDEPLOY="$BUILD_DIR/linuxdeploy-x86_64.AppImage"
    else
        echo "linuxdeploy not found. Install it or set LINUXDEPLOY variable." >&2
        exit 1
    fi
fi

LINUXDEPLOY_PLUGIN_QT="${LINUXDEPLOY_PLUGIN_QT:-linuxdeploy-plugin-qt}"
if ! command -v "$LINUXDEPLOY_PLUGIN_QT" &>/dev/null; then
    if [[ -x "$BUILD_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" ]]; then
        LINUXDEPLOY_PLUGIN_QT="$BUILD_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
    else
        echo "linuxdeploy-plugin-qt not found. Install it or set LINUXDEPLOY_PLUGIN_QT." >&2
        exit 1
    fi
fi

export QML_SOURCES_PATHS="$ROOT_DIR/src/gui/qml"
export PATH="$ROOT_DIR/build:${PATH}"

mkdir -p "$DIST_DIR"
OUTPUT="$DIST_DIR/Remus-${VERSION}-x86_64.AppImage"
export OUTPUT

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/remus.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png" \
    --executable "$APPDIR/usr/bin/remus-gui" \
    --plugin qt \
    --output appimage

# ── Checksum ────────────────────────────────────────────────────────────────
if [[ -f "$OUTPUT" ]]; then
    sha256sum "$OUTPUT" > "$OUTPUT.sha256"
    ls -lh "$OUTPUT" "$OUTPUT.sha256"
    echo "AppImage created: $OUTPUT"
    echo "Bundled binaries: remus-gui (desktop entry), remus-cli (terminal)"
else
    echo "AppImage creation failed" >&2
    exit 1
fi
