#!/usr/bin/env bash
# update_dats.sh — Download/update libretro-database DATs for Remus
#
# Usage:
#   scripts/update_dats.sh [--all]
#
# By default, copies the core DATs (no-intro + redump + dat/) for the top
# systems.  With --all, copies every DAT file found.
#
# Requires: git
# License: CC-BY-SA-4.0 (libretro-database)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="$PROJECT_ROOT/data/databases"
CLONE_DIR="${TMPDIR:-/tmp}/libretro-database"
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

mkdir -p "$TARGET_DIR"

copied=0
skipped=0

# Helper: copy a DAT file if it exists
copy_dat() {
    local src="$1"
    local basename
    basename="$(basename "$src")"

    if [[ ! -f "$src" ]]; then
        return
    fi

    cp "$src" "$TARGET_DIR/$basename"
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

# 1. metadat/no-intro/ — primary cartridge hash DATs
if [[ -d "$CLONE_DIR/metadat/no-intro" ]]; then
    for dat in "$CLONE_DIR/metadat/no-intro/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            copy_dat "$dat"
        else
            skipped=$((skipped + 1))
        fi
    done
fi

# 2. metadat/redump/ — primary disc hash DATs
if [[ -d "$CLONE_DIR/metadat/redump" ]]; then
    for dat in "$CLONE_DIR/metadat/redump/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            copy_dat "$dat"
        else
            skipped=$((skipped + 1))
        fi
    done
fi

# 3. dat/ — libretro's own DATs (smaller, curated)
if [[ -d "$CLONE_DIR/dat" ]]; then
    for dat in "$CLONE_DIR/dat/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            copy_dat "$dat"
        else
            skipped=$((skipped + 1))
        fi
    done
fi

# 4. Metadata DATs (genre, developer, publisher, maxusers, releaseyear)
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

# 5. GameTDB XML databases (Wii/GameCube, DS, 3DS, WiiU, Switch, PS3)
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
echo "Location: $TARGET_DIR"
ls -1 "$TARGET_DIR" | wc -l | xargs -I{} echo "Total DAT files: {}"
if [[ -d "$METADATA_DIR" ]]; then
    find "$METADATA_DIR" -name '*.dat' | wc -l | xargs -I{} echo "Total metadata DAT files: {}"
fi
if [[ -d "$GAMETDB_DIR" ]]; then
    find "$GAMETDB_DIR" -name '*.xml' | wc -l | xargs -I{} echo "Total GameTDB XML files: {}"
fi
