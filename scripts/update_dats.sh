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
#   data/databases/mame/      ← MAME ROMs DAT (local binary or mamedev/mame release)
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
mame_xml_tmp=""
mame_bin_tmp=""
trap 'rm -rf "${CLONE_DIR:-}" "${redump_direct_tmp:-}" "${mame_xml_tmp:-}" "${mame_bin_tmp:-}"' EXIT
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
#    Endpoints may be intermittently unavailable; we try an ordered mirror list
#    and keep the first valid ZIP payload containing a .dat file.
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

# Ordered Redump DAT mirror/base URLs.
# Priority order is based on observed availability and latency.
REDUMP_DIRECT_BASE_URLS=(
    "https://old.redump.info/datfile"
    "http://old.redump.info/datfile"
    "http://redump.org/datfile"
    "https://redump.org/datfile"
)

# Helper: download a Redump ZIP for a slug from the first reachable mirror.
# Prints the successful URL to stdout on success.
download_redump_zip_for_slug() {
    local slug="$1"
    local out_zip="$2"
    local base url

    for base in "${REDUMP_DIRECT_BASE_URLS[@]}"; do
        url="${base%/}/${slug}/"
        if curl -fsSL --retry 2 --retry-delay 1 --max-time 60 -o "$out_zip" "$url"; then
            # Require at least one .dat entry to guard against HTML/error payloads.
            if unzip -Z -1 "$out_zip" 2>/dev/null | grep -E '\.dat$' >/dev/null; then
                echo "$url"
                return 0
            fi
        fi
        rm -f "$out_zip"
    done

    return 1
}

# Optional Wii U Redump source support.
# Primary mechanism: set REDUMP_WIIU_DAT_URL to a ZIP URL containing a DAT.
# Fallback: try known/likely Redump endpoints and keep the first valid DAT.
REDUMP_WIIU_FALLBACK_URLS=(
    "http://old.redump.info/datfile/wiiu/"
    "https://old.redump.info/datfile/wiiu/"
    "https://redump.org/datfile/wiiu/"
    "https://redump.org/datfile/wii-u/"
    "https://redump.org/datfile/nintendo-wii-u/"
)

echo ""
echo "Downloading Redump DATs (direct with mirror fallback)..."
redump_direct_tmp="$(mktemp -d)"

for entry in "${REDUMP_DIRECT_DBS[@]}"; do
    IFS='|' read -r slug destname <<< "$entry"
    zippath="$redump_direct_tmp/${slug}.zip"

    echo "  Fetching ${destname}..."
    if source_url="$(download_redump_zip_for_slug "$slug" "$zippath")"; then
        extracted=$(unzip -Z -1 "$zippath" 2>/dev/null | grep -E '\.dat$' | head -1)
        if [[ -n "$extracted" ]]; then
            if unzip -o -q "$zippath" "$extracted" -d "$redump_direct_tmp" 2>/dev/null; then
                mv "$redump_direct_tmp/$extracted" "$REDUMP_DIR/$destname"
                copied=$((copied + 1))
                echo "    Added from $source_url"
            else
                echo "    Warning: failed to extract DAT from $slug zip"
            fi
        else
            echo "    Warning: no .dat file found in $slug zip"
        fi
        rm -f "$zippath"
    else
        echo "    Warning: failed to download $slug from all configured Redump mirrors"
    fi
done

# 4a. Wii U Redump DAT (signature-rich source for physical Wii U dumps).
# If already present, keep the local file.
wiiu_redump_dest="$REDUMP_DIR/Nintendo - Wii U.dat"
if [[ ! -f "$wiiu_redump_dest" ]]; then
    echo ""
    echo "Attempting Wii U Redump DAT..."

    wiiu_urls=()
    if [[ -n "${REDUMP_WIIU_DAT_URL:-}" ]]; then
        wiiu_urls+=("$REDUMP_WIIU_DAT_URL")
    fi
    for candidate in "${REDUMP_WIIU_FALLBACK_URLS[@]}"; do
        wiiu_urls+=("$candidate")
    done

    wiiu_added=0
    for wiiu_url in "${wiiu_urls[@]}"; do
        [[ -n "$wiiu_url" ]] || continue
        wiiu_zip="$redump_direct_tmp/wiiu-redump.zip"
        rm -f "$wiiu_zip"

        if ! curl -fsSL -o "$wiiu_zip" --max-time 120 "$wiiu_url"; then
            continue
        fi

        wiiu_extracted=$(unzip -Z -1 "$wiiu_zip" 2>/dev/null | grep -E '\.dat$' | head -1)
        if [[ -z "$wiiu_extracted" ]]; then
            continue
        fi

        if unzip -o -q "$wiiu_zip" "$wiiu_extracted" -d "$redump_direct_tmp" 2>/dev/null; then
            mv "$redump_direct_tmp/$wiiu_extracted" "$wiiu_redump_dest"
            copied=$((copied + 1))
            wiiu_added=1
            echo "  Added Wii U Redump DAT from $wiiu_url"
            break
        fi
    done

    if [[ "$wiiu_added" -eq 0 ]]; then
        echo "  Warning: failed to acquire Wii U Redump DAT (set REDUMP_WIIU_DAT_URL to provide a direct ZIP URL)."
    fi
fi

# 4b. MAME DAT — generated from a MAME binary via -listxml, then converted to
#     Logiqx XML (the format Remus's DatParser expects).  Two sources tried in
#     order:
#       1. A locally installed mame/mame64/mame-arcade binary (fastest, no
#          extra download — install via "pacman -S mame" or "apt install mame").
#       2. The official Linux release binary from mamedev/mame GitHub releases
#          (~19 MB download, extracted to a temp dir and removed afterwards).
#     Pleasuredome requires torrent/membership access and is not used here.
#     Requires: python3  (for listxml → Logiqx XML conversion)

mkdir -p "$MAME_DIR"
# Remove any empty/corrupt DAT files left by previous failed runs
find "$MAME_DIR" -maxdepth 1 -name "*.dat" -empty -delete 2>/dev/null || true

echo ""
echo "Fetching MAME DAT..."

mame_bin=""

# Step 1: look for a locally installed MAME binary
for mame_candidate in mame mame64 mame-arcade; do
    if command -v "$mame_candidate" &>/dev/null; then
        mame_bin="$(command -v "$mame_candidate")"
        echo "  Found local MAME: $mame_bin"
        break
    fi
done

# Step 2: no local binary — download the Linux release from mamedev/mame.
#   Strategy A: the release zip bundles a pre-built mame*.xml listing — use it
#               directly (no binary execution required).
#   Strategy B: extract the MAME binary from the zip and run -listxml.
if [[ -z "$mame_bin" ]]; then
    echo "  No local MAME binary — fetching from mamedev/mame GitHub releases..."
    mame_api_url="https://api.github.com/repos/mamedev/mame/releases/latest"
    mame_api_headers=(
        -H "Accept: application/vnd.github+json"
    )
    if [[ -n "${GITHUB_TOKEN:-}" ]]; then
        mame_api_headers+=( -H "Authorization: Bearer ${GITHUB_TOKEN}" )
    fi

    mame_lx_url=$(curl -fsSL --max-time 30 \
        "${mame_api_headers[@]}" \
        "$mame_api_url" 2>/dev/null \
        | python3 -c "
import sys, json
d = json.load(sys.stdin)
url = next((a['browser_download_url'] for a in d.get('assets', [])
            if a['name'].endswith('lx.zip')), '')
print(url)
" 2>/dev/null || true)

    if [[ -n "$mame_lx_url" ]]; then
        mame_bin_tmp="$(mktemp -d)"
        mame_lx_zip="$mame_bin_tmp/mame_lx.zip"
        echo "  Downloading $(basename "$mame_lx_url")..."
        if curl -fsSL -o "$mame_lx_zip" --max-time 600 "$mame_lx_url"; then
            # Strategy A: prefer a pre-built XML bundled in the release zip
            mame_xml_inner=$(unzip -l "$mame_lx_zip" 2>/dev/null \
                | awk '/[[:space:]]mame[^\/]*\.xml$/ { print $NF; exit }')
            if [[ -n "$mame_xml_inner" ]] \
               && unzip -o -q "$mame_lx_zip" "$mame_xml_inner" -d "$mame_bin_tmp"; then
                mame_xml_tmp="$mame_bin_tmp/$mame_xml_inner"
                echo "  Using bundled XML: $mame_xml_inner"
            else
                # Strategy B: extract and run the MAME binary
                # Explicitly exclude .xml files so we match only the binary.
                mame_inner=$(unzip -l "$mame_lx_zip" 2>/dev/null \
                    | awk '!/\.xml$/ && /[[:space:]]mame[^\/]*$/ { print $NF; exit }')
                if [[ -n "$mame_inner" ]] \
                   && unzip -o -q "$mame_lx_zip" "$mame_inner" -d "$mame_bin_tmp"; then
                    chmod +x "$mame_bin_tmp/$mame_inner"
                    mame_bin="$mame_bin_tmp/$mame_inner"
                    echo "  Extracted binary: $mame_inner"
                else
                    echo "  Warning: could not extract MAME binary or XML from zip"
                fi
            fi
        else
            echo "  Warning: failed to download MAME Linux release"
        fi
    else
        echo "  Warning: could not resolve MAME release asset URL via GitHub API"
    fi
fi

# If we have a binary but no pre-built XML, run -listxml to produce the XML.
mame_detected_ver=""
if [[ -z "$mame_xml_tmp" ]] && [[ -n "$mame_bin" && -x "$mame_bin" ]]; then
    mame_detected_ver=$("$mame_bin" -version 2>/dev/null \
        | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "")
    echo "  MAME version: ${mame_detected_ver:-unknown}"
    echo "  Running: mame -listxml  (may take ~30 s, uses ~400 MB temp space)..."
    mame_xml_tmp="$(mktemp -t mame_listxml_XXXXXX.xml)"
    if ! "$mame_bin" -listxml > "$mame_xml_tmp" 2>/dev/null \
       || [[ ! -s "$mame_xml_tmp" ]]; then
        echo "  Warning: mame -listxml produced no output"
        rm -f "$mame_xml_tmp"
        mame_xml_tmp=""
    fi
fi

if [[ -n "$mame_xml_tmp" && -s "$mame_xml_tmp" ]]; then
    # Version for the output filename: prefer the value obtained from -version;
    # fall back to the number embedded in the XML filename (e.g. mame0287.xml → 0287).
    if [[ -z "$mame_detected_ver" ]]; then
        mame_detected_ver=$(basename "$mame_xml_tmp" .xml \
            | grep -oE '[0-9]+' | head -1 || echo "")
    fi
    mame_out_dat="$MAME_DIR/MAME${mame_detected_ver:+ $mame_detected_ver} ROMs.dat"
    echo "  Converting MAME XML → Logiqx XML DAT..."
    if python3 - "$mame_xml_tmp" "$mame_out_dat" <<'PYEOF'
import sys
import xml.etree.ElementTree as ET

def esc(s):
    """XML-escape a string for use in text content and attribute values."""
    return (s or "").replace("&", "&amp;").replace("<", "&lt;") \
                    .replace(">", "&gt;").replace('"', "&quot;")

src, dst = sys.argv[1], sys.argv[2]
root_elem = None
version   = ""
count     = 0

with open(dst, "w", encoding="utf-8") as f:
    for event, elem in ET.iterparse(src, events=("start", "end")):
        # Capture root <mame build="..."> on first start event
        if event == "start" and elem.tag == "mame" and root_elem is None:
            root_elem = elem
            build     = elem.get("build", "")
            version   = build.split("(")[0].strip() if build else ""
            f.write("<?xml version='1.0' encoding='utf-8'?>\n<datafile>\n")
            f.write("  <header>\n")
            f.write(f"    <name>MAME {esc(version)}</name>\n")
            f.write(f"    <description>MAME {esc(version)}</description>\n")
            f.write( "    <category>Standard DatFile</category>\n")
            f.write(f"    <version>{esc(version)}</version>\n")
            f.write( "    <author>MAME team</author>\n")
            f.write( "    <url>https://www.mamedev.org</url>\n")
            f.write( "  </header>\n")
            continue
        # Process each <machine> once all its children are available
        if event != "end" or elem.tag != "machine":
            continue
        if (elem.get("isdevice") == "yes"
                or elem.get("isbios")   == "yes"
                or elem.get("runnable") == "no"):
            if root_elem is not None:
                root_elem.clear()
            continue
        name  = esc(elem.get("name", ""))
        clone = elem.get("cloneof", "")
        ca    = f' cloneof="{esc(clone)}"' if clone else ""
        f.write(f'  <machine name="{name}"{ca}>\n')
        for tag in ("description", "year", "manufacturer"):
            el = elem.find(tag)
            if el is not None and el.text:
                f.write(f"    <{tag}>{esc(el.text)}</{tag}>\n")
        for rom in elem.findall("rom"):
            if rom.get("status") == "nodump":
                continue
            parts = [f'{a}="{esc(rom.get(a))}"'
                     for a in ("name", "size", "crc", "sha1", "md5")
                     if rom.get(a)]
            if parts:
                f.write(f'    <rom {" ".join(parts)}/>\n')
        f.write("  </machine>\n")
        count += 1
        if root_elem is not None:
            root_elem.clear()
    f.write("</datafile>\n")

print(f"  MAME DAT: {dst} ({count} machines)")
PYEOF
    then
        copied=$((copied + 1))
    else
        echo "  Warning: MAME XML → DAT conversion failed"
        rm -f "$mame_out_dat"
    fi
    rm -f "$mame_xml_tmp"
    mame_xml_tmp=""
    if [[ -n "$mame_bin_tmp" ]]; then
        rm -rf "$mame_bin_tmp"
        mame_bin_tmp=""
    fi
else
    echo "  Warning: no usable MAME binary or bundled XML found."
    echo "  To add arcade/MAME coverage, either:"
    echo "    1. Install MAME and re-run:  pacman -S mame       (Arch/Manjaro)"
    echo "                                 apt install mame     (Debian/Ubuntu)"
    echo "    2. Place a merged MAME .dat file manually in: $MAME_DIR/"
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
