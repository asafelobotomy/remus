#!/usr/bin/env bash
set -euo pipefail

# Parse arguments
NO_STRIP="${NO_STRIP:-false}"
for arg in "$@"; do
    case $arg in
        --no-strip) NO_STRIP=true ;;
        --help|-h)
            echo "Usage: $0 [--no-strip]"
            echo "  --no-strip  Disable library stripping (for newer distros)"
            exit 0
            ;;
    esac
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist}"
APPDIR="$BUILD_DIR/AppDir"
TOOLS_DIR="$BUILD_DIR/tools"

ICON_SRC="$ROOT_DIR/assets/icon/remus_icon.png"
DESKTOP_SRC="$ROOT_DIR/assets/remus.desktop"

BIN_GUI="$BUILD_DIR/src/ui/remus-gui"
BIN_CLI="$BUILD_DIR/remus-cli"

if [[ ! -f "$BIN_GUI" ]]; then
    echo "Missing GUI binary: $BIN_GUI" >&2
    exit 1
fi

if [[ ! -f "$BIN_CLI" ]]; then
    echo "Missing CLI binary: $BIN_CLI" >&2
    exit 1
fi

if [[ ! -f "$ICON_SRC" ]]; then
    echo "Missing icon: $ICON_SRC" >&2
    exit 1
fi

if [[ ! -f "$DESKTOP_SRC" ]]; then
    echo "Missing desktop file: $DESKTOP_SRC" >&2
    exit 1
fi

VERSION="$(grep -E 'APP_VERSION' "$ROOT_DIR/src/core/constants/constants.h" | sed -E 's/.*"([0-9]+\.[0-9]+\.[0-9]+)".*/\1/' || true)"
if [[ -z "$VERSION" ]]; then
    VERSION="0.0.0"
fi

rm -rf "$APPDIR"
mkdir -p \
    "$APPDIR/usr/bin" \
    "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
    "$DIST_DIR" \
    "$TOOLS_DIR"

cp -a "$BIN_GUI" "$APPDIR/usr/bin/remus-gui"
cp -a "$BIN_CLI" "$APPDIR/usr/bin/remus-cli"
cp -a "$DESKTOP_SRC" "$APPDIR/usr/share/applications/remus.desktop"
cp -a "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png"
cp -a "$ICON_SRC" "$APPDIR/remus.png"

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
PLUGIN_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-x86_64.AppImage"

if [[ ! -x "$LINUXDEPLOY" ]]; then
    curl -L -o "$LINUXDEPLOY" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [[ ! -x "$PLUGIN_QT" ]]; then
    curl -L -o "$PLUGIN_QT" "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$PLUGIN_QT"
fi

if [[ ! -x "$APPIMAGETOOL" ]]; then
    curl -L -o "$APPIMAGETOOL" "https://github.com/probonopd/go-appimage/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "$APPIMAGETOOL"
fi

# Set up linuxdeploy options
if [[ "$NO_STRIP" == "true" ]]; then
    echo "Note: Library stripping disabled (--no-strip)"
    export NO_STRIP=1
fi

# Find Qt 6 qmake
QMAKE6="$(which qmake6 2>/dev/null || which qmake-qt6 2>/dev/null || which qmake 2>/dev/null || echo "")"
if [[ -n "$QMAKE6" ]]; then
    echo "Using qmake: $QMAKE6"
    export QMAKE="$QMAKE6"
fi

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    -e "$APPDIR/usr/bin/remus-gui" \
    -e "$APPDIR/usr/bin/remus-cli" \
    -d "$APPDIR/usr/share/applications/remus.desktop" \
    -i "$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png" \
    --plugin qt

REPO_OWNER="${REPO_OWNER:-}"
REPO_NAME="${REPO_NAME:-}"
UPDATE_INFO=""
if [[ -n "$REPO_OWNER" && -n "$REPO_NAME" ]]; then
    UPDATE_INFO="gh-releases-zsync|${REPO_OWNER}|${REPO_NAME}|latest|Remus-*-x86_64.AppImage.zsync"
fi

OUTPUT_APPIMAGE="$DIST_DIR/Remus-${VERSION}-x86_64.AppImage"

if [[ -n "$UPDATE_INFO" ]]; then
    "$APPIMAGETOOL" -u "$UPDATE_INFO" "$APPDIR" "$OUTPUT_APPIMAGE"
else
    "$APPIMAGETOOL" "$APPDIR" "$OUTPUT_APPIMAGE"
fi

ls -la "$OUTPUT_APPIMAGE" "$OUTPUT_APPIMAGE".zsync
