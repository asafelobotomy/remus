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

# linuxdeploy validates icon pixel size against the hicolor directory name (256x256).
ICON_SRC="$ROOT_DIR/assets/icon/remus_icon.png"
ICON_DEST="$APPDIR/usr/share/icons/hicolor/256x256/apps/remus.png"
if command -v python3 &>/dev/null; then
    python3 - "$ICON_SRC" "$ICON_DEST" <<'PY'
import sys
from pathlib import Path
try:
    from PIL import Image
except ImportError:
    sys.exit(1)
src, dest = sys.argv[1], sys.argv[2]
Path(dest).parent.mkdir(parents=True, exist_ok=True)
Image.open(src).resize((256, 256), Image.Resampling.LANCZOS).save(dest)
PY
    if [[ ! -f "$ICON_DEST" ]]; then
        cp -a "$ICON_SRC" "$ICON_DEST"
    fi
else
    cp -a "$ICON_SRC" "$ICON_DEST"
fi

# ── Bundle compendium data, scripts, and CLI shim ─────────────────────────────
REMUS_SHARE="$APPDIR/usr/share/remus"
mkdir -p "$REMUS_SHARE"

if [[ -d "$ROOT_DIR/data/compendium" ]]; then
    mkdir -p "$REMUS_SHARE/data"
    cp -a "$ROOT_DIR/data/compendium" "$REMUS_SHARE/data/"
    echo "Bundled data/compendium"
fi

if [[ -d "$ROOT_DIR/scripts" ]]; then
    cp -a "$ROOT_DIR/scripts" "$REMUS_SHARE/"
    echo "Bundled scripts/"
fi

mkdir -p "$REMUS_SHARE/build"
ln -sf ../../bin/remus-cli "$REMUS_SHARE/build/remus-cli"

COMPENDIUM_SRC="$ROOT_DIR/data/compendium/remus_compendium.db"
if [[ -f "$COMPENDIUM_SRC" ]]; then
    mkdir -p "$REMUS_SHARE/data/compendium"
    cp -a "$COMPENDIUM_SRC" "$REMUS_SHARE/data/compendium/remus_compendium.db"
    echo "Bundled compendium DB: $COMPENDIUM_SRC"
else
    echo "NOTE: compendium DB not found at $COMPENDIUM_SRC — wizard will create on first build" >&2
fi

# AppImage launcher sets REMUS_DATA_DIR so findDataSubdir and wizard resolve bundled assets.
cat > "$APPDIR/usr/bin/remus-gui-launch" <<'LAUNCH'
#!/usr/bin/env bash
HERE="$(dirname "$(readlink -f "$0")")"
APPDIR="${APPDIR:-$(dirname "$HERE")}"
export REMUS_DATA_DIR="${APPDIR}/usr/share/remus"
export PATH="${HERE}:${PATH}"
exec "${HERE}/remus-gui" "$@"
LAUNCH
chmod +x "$APPDIR/usr/bin/remus-gui-launch"

# Patch desktop entry to use launcher (linuxdeploy reads this file).
sed -i 's|^Exec=remus-gui|Exec=remus-gui-launch|' "$APPDIR/usr/share/applications/remus.desktop"

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

# Qt6 builds need qmake6; optional SQL drivers (ibase/mysql/odbc) are not bundled.
if command -v qmake6 &>/dev/null; then
    export QMAKE="${QMAKE:-qmake6}"
fi
export NO_STRIP="${NO_STRIP:-1}"
export LINUXDEPLOY_EXCLUDED_LIBRARIES="${LINUXDEPLOY_EXCLUDED_LIBRARIES:-libqsqlibase.so;libqsqlmysql.so;libqsqlodbc.so;libqsqlmimer.so;libqsqlpsql.so}"

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
