#!/usr/bin/env bash
# Download the official RAHasher CLI from RetroAchievements/RALibretro releases.
#
# RAHasher is not published as a linkable library — Remus shells out to this binary
# for disc-based systems (PS1/PS2/NDS/GC/Wii/PSP/…). Cartridge systems use native
# rules in src/core/ra_hasher.cpp.
#
# Usage:
#   scripts/install_rahasher.sh [--version 1.8.2] [--force]
#
# Installs to: data/tools/rahasher/RAHasher
# Optional: export REMUS_RAHASHER_PATH="$PWD/data/tools/rahasher/RAHasher"
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="1.8.2"
FORCE=0
DEST_DIR="$ROOT_DIR/data/tools/rahasher"
BINARY="$DEST_DIR/RAHasher"

usage() {
    cat <<EOF
usage: $(basename "$0") [--version <tag>] [--force]

Downloads RAHasher from RetroAchievements/RALibretro GitHub releases.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --force)
            FORCE=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -x "$BINARY" && "$FORCE" -eq 0 ]]; then
    echo "RAHasher already installed: $BINARY"
  "$BINARY" 2>&1 | head -3 || true
    exit 0
fi

arch="$(uname -m)"
case "$arch" in
    x86_64 | amd64) RA_ARCH="x64" ;;
    i686 | i386) RA_ARCH="x86" ;;
    *)
        echo "error: unsupported CPU architecture for prebuilt RAHasher: $arch" >&2
        echo "hint: build from https://github.com/RetroAchievements/RALibretro" >&2
        exit 1
        ;;
esac

asset="RAHasher-${RA_ARCH}-Linux-${VERSION}.zip"
url="https://github.com/RetroAchievements/RALibretro/releases/download/${VERSION}/${asset}"
cache_dir="${XDG_CACHE_HOME:-$ROOT_DIR/.cache}/remus/rahasher"
mkdir -p "$cache_dir" "$DEST_DIR"
zip_path="$cache_dir/$asset"

echo "==> Downloading RAHasher ${VERSION} (${RA_ARCH})"
if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 3 -o "$zip_path" "$url"
elif command -v wget >/dev/null 2>&1; then
    wget -O "$zip_path" "$url"
else
    echo "error: curl or wget required" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
unzip -o -q "$zip_path" -d "$tmpdir"

found="$(find "$tmpdir" -type f \( -name RAHasher -o -name rahasher \) | head -1)"
if [[ -z "$found" ]]; then
    echo "error: RAHasher binary not found inside $asset" >&2
    exit 1
fi

install -m 755 "$found" "$BINARY"

echo "==> Installed: $BINARY"
if "$BINARY" 2>&1 | head -5; then
    :
fi
echo ""
echo "Add to .env.local (optional — Remus also auto-detects data/tools/rahasher/RAHasher):"
echo "  export REMUS_RAHASHER_PATH=\"$BINARY\""
