#!/usr/bin/env bash
set -euo pipefail
# Ensure all offline compendium inputs are present before a build.
# Delegates to per-source updaters; each skips files when content is unchanged.
#
# Usage:
#   scripts/update_compendium_offline_sources.sh
#
# Called automatically by scripts/build_compendium_full.sh unless --skip-update.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Compendium offline sources (skip unchanged files)"
bash "$ROOT_DIR/scripts/update_dats.sh" --all

echo ""
echo "==> LaunchBox metadata (optional — manual install)"
if bash "$ROOT_DIR/scripts/update_launchbox_metadata.sh" --optional; then
    :
else
    echo "  warning: LaunchBox metadata check failed (enrichment pass will skip launchbox)" >&2
fi

echo ""
echo "==> libretro-thumbnails (optional — large download)"
if bash "$ROOT_DIR/scripts/update_libretro_thumbnails.sh" --all; then
    :
else
    echo "  warning: libretro-thumbnails sync failed (consolidate pass will skip missing systems)" >&2
fi

echo ""
echo "==> Offline source inventory"
count_glob() {
    local pattern="$1"
    find $pattern -type f 2>/dev/null | wc -l | tr -d ' '
}

echo "  DAT files (curated):     $(count_glob "$ROOT_DIR/data/databases/*.dat")"
echo "  no-intro DATs:           $(count_glob "$ROOT_DIR/data/databases/no-intro/*.dat")"
echo "  redump DATs:             $(count_glob "$ROOT_DIR/data/databases/redump/*.dat")"
echo "  metadata DATs:           $(find "$ROOT_DIR/data/metadata" -name '*.dat' 2>/dev/null | wc -l | tr -d ' ')"
echo "  GameTDB XMLs:            $(count_glob "$ROOT_DIR/data/gametdb/*.xml")"
echo "  patch/hack DATs:         $(count_glob "$ROOT_DIR/data/patches/hacks/*.dat")"
echo "  Hasheous JSON dumps:     $(find "$ROOT_DIR/data/hasheous/dumps" -type f -name '*.json' ! -name 'PlatformMapping.json' 2>/dev/null | wc -l | tr -d ' ')"
if [[ -f "$ROOT_DIR/data/openvgdb/openvgdb.sqlite" ]]; then
    echo "  OpenVGDB:                $(du -sh "$ROOT_DIR/data/openvgdb/openvgdb.sqlite" | cut -f1)"
else
    echo "  OpenVGDB:                missing"
fi
if [[ -f "$ROOT_DIR/data/mame/listxml.xml" ]]; then
    echo "  MAME listxml:            $(du -sh "$ROOT_DIR/data/mame/listxml.xml" | cut -f1)"
else
    echo "  MAME listxml:            missing"
fi
if [[ -f "$ROOT_DIR/data/mame/catver.ini" ]]; then
    echo "  MAME catver.ini:         present"
else
    echo "  MAME catver.ini:         missing"
fi
if [[ -f "$ROOT_DIR/data/launchbox/Metadata.xml" ]] \
   && [[ "$(stat -c%s "$ROOT_DIR/data/launchbox/Metadata.xml" 2>/dev/null || echo 0)" -gt 1048576 ]]; then
    echo "  LaunchBox Metadata.xml:  $(du -sh "$ROOT_DIR/data/launchbox/Metadata.xml" | cut -f1)"
else
    echo "  LaunchBox Metadata.xml:  not installed (optional)"
fi
if [[ -d "$ROOT_DIR/data/acquisition/libretro-thumbnails" ]]; then
    echo "  libretro-thumbnails:     $(find "$ROOT_DIR/data/acquisition/libretro-thumbnails" -type f -name '*.png' 2>/dev/null | wc -l | tr -d ' ') PNG files"
else
    echo "  libretro-thumbnails:     not acquired"
fi
if [[ -d "$ROOT_DIR/data/remus-thumbnails/blobs" ]]; then
    echo "  remus-thumbnails blobs:  $(find "$ROOT_DIR/data/remus-thumbnails/blobs" -type f -name '*.webp' 2>/dev/null | wc -l | tr -d ' ') WebP blobs"
else
    echo "  remus-thumbnails blobs:  not built"
fi

echo ""
echo "Offline sources ready for compendium build."
