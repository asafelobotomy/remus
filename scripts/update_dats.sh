#!/usr/bin/env bash
set -euo pipefail
# update_dats.sh — Download/update libretro-database DATs for Remus
#
# Usage:
#   scripts/update_dats.sh [--all]
#
# By default, copies the core DATs (no-intro + redump + dat/) for the top
# systems.  With --all, copies every DAT file found.
#
# Directory layout written to data/databases/:
#   data/databases/           ← libretro dat/ (curated, GameTDB-style)
#   data/databases/no-intro/  ← metadat/no-intro/ (No-Intro full catalogs)
#   data/databases/redump/    ← metadat/redump/   (Redump full catalogs)
#   data/databases/mame/      ← Pleasuredome MAME DAT (arcade ROMs)
#
# Storing the three sources in separate subdirectories avoids filename
# collisions (e.g. Sega - Saturn.dat exists in both dat/ and redump/)
# and lets both be ingested as distinct catalog sources.
#
# Requires: git
# License: CC-BY-SA-4.0 (libretro-database)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="$PROJECT_ROOT/data/databases"
NO_INTRO_DIR="$TARGET_DIR/no-intro"
REDUMP_DIR="$TARGET_DIR/redump"
MAME_DIR="$TARGET_DIR/mame"
CLONE_DIR="$(mktemp -d)"
trap 'rm -rf "$CLONE_DIR"' EXIT
REPO_URL="https://github.com/libretro/libretro-database.git"

# Core systems to include by default (filename stems as they appear in the repo)
CORE_SYSTEMS=(
    "Atari - 2600"
    "Atari - 7800"
    "Bandai - WonderSwan"
    "Bandai - WonderSwan Color"
    "Coleco - ColecoVision"
    "GCE - Vectrex"
    "NEC - PC Engine - TurboGrafx 16"
    "NEC - PC Engine CD - TurboGrafx-CD"
    "Nintendo - Family Computer Disk System"
    "Nintendo - Game Boy"
    "Nintendo - Game Boy Advance"
    "Nintendo - Game Boy Color"
    "Nintendo - GameCube"
    "Nintendo - Nintendo 3DS"
    "Nintendo - Nintendo 64"
    "Nintendo - Nintendo DS"
    "Nintendo - Nintendo Entertainment System"
    "Nintendo - Super Nintendo Entertainment System"
    "Nintendo - Virtual Boy"
    "Nintendo - Wii"
    "Sega - 32X"
    "Sega - Dreamcast"
    "Sega - Game Gear"
    "Sega - Master System - Mark III"
    "Sega - Mega-CD - Sega CD"
    "Sega - Mega Drive - Genesis"
    "Sega - Saturn"
    "Sega - SG-1000"
    "SNK - Neo Geo Pocket"
    "SNK - Neo Geo Pocket Color"
    "Sony - PlayStation"
    "Sony - PlayStation 2"
    "Sony - PlayStation Portable"
    "The 3DO Company - 3DO"
)

copy_all=false
if [[ "${1:-}" == "--all" ]]; then
    copy_all=true
fi

echo "=== Remus DAT Updater ==="
echo "Target: $TARGET_DIR"
echo "  dat/       → $TARGET_DIR"
echo "  no-intro/  → $NO_INTRO_DIR"
echo "  redump/    → $REDUMP_DIR"

# Clone or update the repo (shallow clone to save bandwidth)
if [[ -d "$CLONE_DIR/.git" ]]; then
    echo "Updating existing clone..."
    git -C "$CLONE_DIR" pull --ff-only --depth=1 2>/dev/null || \
        git -C "$CLONE_DIR" fetch --depth=1 origin master && \
        git -C "$CLONE_DIR" reset --hard origin/master
else
    echo "Cloning libretro-database (shallow)..."
    rm -rf "$CLONE_DIR"
    git clone --depth=1 "$REPO_URL" "$CLONE_DIR"
fi

mkdir -p "$TARGET_DIR" "$NO_INTRO_DIR" "$REDUMP_DIR"

copied=0
skipped=0

# Helper: copy a DAT file to a target directory
copy_dat() {
    local src="$1"
    local dest_dir="$2"
    local basename
    basename="$(basename "$src")"

    if [[ ! -f "$src" ]]; then
        return
    fi

    cp "$src" "$dest_dir/$basename"
    copied=$((copied + 1))
}

# Helper: check if a system name matches the core list
is_core_system() {
    local stem="$1"
    for sys in "${CORE_SYSTEMS[@]}"; do
        if [[ "$stem" == "$sys" ]]; then
            return 0
        fi
    done
    return 1
}

# Helper: should we include this DAT?
should_include() {
    local filepath="$1"
    if $copy_all; then
        return 0
    fi
    local basename
    basename="$(basename "$filepath" .dat)"
    is_core_system "$basename"
}

echo ""
echo "Copying DAT files..."

# 1. metadat/no-intro/ — full No-Intro catalogs for cartridge systems
if [[ -d "$CLONE_DIR/metadat/no-intro" ]]; then
    for dat in "$CLONE_DIR/metadat/no-intro/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            copy_dat "$dat" "$NO_INTRO_DIR"
        else
            skipped=$((skipped + 1))
        fi
    done
fi

# 2. metadat/redump/ — full Redump catalogs for disc systems
if [[ -d "$CLONE_DIR/metadat/redump" ]]; then
    for dat in "$CLONE_DIR/metadat/redump/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            copy_dat "$dat" "$REDUMP_DIR"
        else
            skipped=$((skipped + 1))
        fi
    done
fi

# 3. dat/ — libretro's curated DATs (GameTDB-style, supplemental metadata)
if [[ -d "$CLONE_DIR/dat" ]]; then
    for dat in "$CLONE_DIR/dat/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            copy_dat "$dat" "$TARGET_DIR"
        else
            skipped=$((skipped + 1))
        fi
    done
fi

# 4. Redump DATs not included in libretro-database — downloaded directly from
#    redump.org (public downloads, ZIP-compressed).  Each entry is:
#    "url_slug|dest_filename"
#    The slug is the URL path component after /datfile/.
#    redump.org ships a ZIP; we extract the first .dat inside it.
REDUMP_DIRECT_DBS=(
    "arch|Acorn - Archimedes.dat"
    "mac|Apple - Macintosh.dat"
    "qis|Bandai - Playdia Quick Interactive System.dat"
    "acd|Commodore - Amiga CD.dat"
    "fmt|Fujitsu - FM-Towns.dat"
    "pc-88|NEC - PC-88 series.dat"
    "chihiro|Arcade - Sega - Chihiro.dat"
    "lindbergh|Arcade - Sega - Lindbergh.dat"
    "trf|Arcade - Namco - Sega - Nintendo - Triforce.dat"
    "vis|Memorex - Visual Information System.dat"
)

echo ""
echo "Downloading Redump DATs (direct from redump.org)..."
redump_direct_tmp="$(mktemp -d)"
trap 'rm -rf "$redump_direct_tmp"' RETURN

for entry in "${REDUMP_DIRECT_DBS[@]}"; do
    IFS='|' read -r slug destname <<< "$entry"
    url="http://redump.org/datfile/${slug}/"
    zippath="$redump_direct_tmp/${slug}.zip"

    echo "  Fetching ${destname}..."
    if curl -fsSL -o "$zippath" --max-time 60 "$url"; then
        extracted=$(unzip -l "$zippath" 2>/dev/null | grep -o '[^ ]*\.dat$' | head -1)
        if [[ -n "$extracted" ]]; then
            if unzip -o -q "$zippath" "$extracted" -d "$redump_direct_tmp" 2>/dev/null; then
                mv "$redump_direct_tmp/$extracted" "$REDUMP_DIR/$destname"
                copied=$((copied + 1))
            else
                echo "    Warning: failed to extract DAT from $slug zip"
            fi
        else
            echo "    Warning: no .dat file found in $slug zip"
        fi
        rm -f "$zippath"
    else
        echo "    Warning: failed to download $slug from redump.org"
    fi
done

# 4b. MAME DAT from Pleasuredome (publicly hosted on GitHub gh-pages).
# Update MAME_VERSION when Pleasuredome publishes a new release.
MAME_VERSION="0.287"
MAME_DAT_NAME="MAME ${MAME_VERSION} ROMs (merged).dat"
MAME_URL="https://github.com/pleasuredome/pleasuredome/raw/gh-pages/mame/MAME%20${MAME_VERSION}%20ROMs%20(merged).zip"

mkdir -p "$MAME_DIR"
echo ""
echo "Downloading MAME ${MAME_VERSION} DAT from Pleasuredome..."
mame_tmp="$(mktemp /tmp/mame_dat_XXXXXX.zip)"
if curl -fsSL -o "$mame_tmp" --max-time 120 "$MAME_URL"; then
    extracted=$(unzip -l "$mame_tmp" 2>/dev/null | grep -o '[^ ]*\.xml$' | head -1)
    if [[ -n "$extracted" ]]; then
        if unzip -p "$mame_tmp" "$extracted" > "$MAME_DIR/$MAME_DAT_NAME" 2>/dev/null; then
            echo "  MAME ${MAME_VERSION} DAT written: $MAME_DIR/$MAME_DAT_NAME"
            copied=$((copied + 1))
        else
            echo "  Warning: failed to extract MAME DAT from zip"
        fi
    else
        echo "  Warning: no .xml file found in MAME zip"
    fi
    rm -f "$mame_tmp"
else
    echo "  Warning: failed to download MAME DAT from Pleasuredome"
    rm -f "$mame_tmp"
fi

# 5. Metadata DATs (genre, developer, publisher, maxusers, releaseyear)
METADATA_DIR="$PROJECT_ROOT/data/metadata"
METADAT_TYPES=("genre" "developer" "publisher" "maxusers" "releaseyear")
meta_copied=0

for meta_type in "${METADAT_TYPES[@]}"; do
    src_dir="$CLONE_DIR/metadat/$meta_type"
    dest_dir="$METADATA_DIR/$meta_type"
    if [[ ! -d "$src_dir" ]]; then
        echo "  Skipping $meta_type (not found in repo)"
        continue
    fi
    mkdir -p "$dest_dir"
    for dat in "$src_dir/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            cp "$dat" "$dest_dir/$(basename "$dat")"
            meta_copied=$((meta_copied + 1))
        fi
    done
done

# 6. GameTDB XML databases (Wii/GameCube, DS, 3DS, WiiU, Switch, PS3)
GAMETDB_DIR="$PROJECT_ROOT/data/gametdb"
GAMETDB_BASE="https://www.gametdb.com"
gametdb_copied=0

# Each entry: "zipname|query_params|xml_filename"
GAMETDB_DBS=(
    "wiitdb.zip|LANG=EN&GAMECUBE=1&WIIWARE=1|wiitdb.xml"
    "dstdb.zip|LANG=EN|dstdb.xml"
    "3dstdb.zip|LANG=EN|3dstdb.xml"
    "wiiutdb.zip|LANG=EN|wiiutdb.xml"
    "switchtdb.zip|LANG=EN|switchtdb.xml"
    "ps3tdb.zip|LANG=EN|ps3tdb.xml"
)

echo ""
echo "Downloading GameTDB databases..."
mkdir -p "$GAMETDB_DIR"

for entry in "${GAMETDB_DBS[@]}"; do
    IFS='|' read -r zipname params xmlname <<< "$entry"
    url="$GAMETDB_BASE/$zipname?$params"
    zippath="$GAMETDB_DIR/$zipname"

    echo "  Fetching $zipname..."
    if curl -fsSL -o "$zippath" "$url"; then
        if unzip -o -q "$zippath" "$xmlname" -d "$GAMETDB_DIR" 2>/dev/null; then
            gametdb_copied=$((gametdb_copied + 1))
            rm -f "$zippath"
        else
            echo "    Warning: failed to extract $xmlname from $zipname"
            rm -f "$zippath"
        fi
    else
        echo "    Warning: failed to download $zipname"
    fi
done

echo ""
echo "Done: $copied DATs copied, $skipped skipped, $meta_copied metadata DATs copied, $gametdb_copied GameTDB databases downloaded"

# ── OpenVGDB SQLite ────────────────────────────────────────────────────────────
# OpenVGDB v29.0 (MIT licence) — CRC/SHA1/MD5 ROM metadata with descriptions,
# genres, developers, publishers, and release dates for ~53 k ROM entries.
# Source: https://github.com/OpenVGDB/OpenVGDB/releases
OPENVGDB_DIR="$PROJECT_ROOT/data/openvgdb"
OPENVGDB_DEST="$OPENVGDB_DIR/openvgdb.sqlite"
OPENVGDB_URL="https://github.com/OpenVGDB/OpenVGDB/releases/latest/download/openvgdb.zip"

mkdir -p "$OPENVGDB_DIR"
echo ""
echo "Updating OpenVGDB..."
openvgdb_tmp="$(mktemp -d)"
trap 'rm -rf "$openvgdb_tmp"' RETURN
if curl -fsSL -o "$openvgdb_tmp/openvgdb.zip" "$OPENVGDB_URL"; then
    if unzip -o -q "$openvgdb_tmp/openvgdb.zip" "openvgdb.sqlite" -d "$openvgdb_tmp" 2>/dev/null; then
        mv "$openvgdb_tmp/openvgdb.sqlite" "$OPENVGDB_DEST"
        echo "  OpenVGDB updated: $OPENVGDB_DEST"
    else
        echo "  Warning: failed to extract openvgdb.sqlite from zip"
    fi
else
    echo "  Warning: failed to download OpenVGDB from $OPENVGDB_URL"
fi

echo ""
echo "DAT file locations:"
find "$TARGET_DIR" -maxdepth 1 -name '*.dat' 2>/dev/null | wc -l | xargs -I{} echo "  dat/ (curated):     {} files in $TARGET_DIR"
find "$NO_INTRO_DIR" -maxdepth 1 -name '*.dat' 2>/dev/null | wc -l | xargs -I{} echo "  no-intro/:          {} files in $NO_INTRO_DIR"
find "$REDUMP_DIR" -maxdepth 1 -name '*.dat' 2>/dev/null | wc -l | xargs -I{} echo "  redump/:            {} files in $REDUMP_DIR"
find "$MAME_DIR" -maxdepth 1 -name '*.dat' 2>/dev/null | wc -l | xargs -I{} echo "  mame/:              {} files in $MAME_DIR"
if [[ -d "$METADATA_DIR" ]]; then
    find "$METADATA_DIR" -name '*.dat' | wc -l | xargs -I{} echo "  metadata DATs:      {} files"
fi
if [[ -d "$GAMETDB_DIR" ]]; then
    find "$GAMETDB_DIR" -name '*.xml' | wc -l | xargs -I{} echo "  GameTDB XMLs:       {} files"
fi
if [[ -f "$OPENVGDB_DEST" ]]; then
    echo "  OpenVGDB:           $OPENVGDB_DEST"
fi
echo ""
echo "Next step: run scripts/generate_compendium_manifest.sh to update the manifest."
