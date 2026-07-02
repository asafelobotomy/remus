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
#   data/databases/mame-redump-chd/ ← MAME Redump CHD DATs (header SHA1 index)
#
# Storing the three sources in separate subdirectories avoids filename
# collisions (e.g. Sega - Saturn.dat exists in both dat/ and redump/)
# and lets both be ingested as distinct catalog sources.
#
# Requires: git
# License: CC-BY-SA-4.0 (libretro-database)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=gh_git_env.sh
source "${SCRIPT_DIR}/gh_git_env.sh"
# shellcheck source=compendium_offline_helpers.sh
source "${SCRIPT_DIR}/compendium_offline_helpers.sh"
TARGET_DIR="$PROJECT_ROOT/data/databases"
NO_INTRO_DIR="$TARGET_DIR/no-intro"
REDUMP_DIR="$TARGET_DIR/redump"
MAME_DIR="$TARGET_DIR/mame"
MAME_REDUMP_CHD_DIR="$TARGET_DIR/mame-redump-chd"
UPDATE_DATS_CACHE_DIR="${UPDATE_DATS_CACHE_DIR:-${XDG_CACHE_HOME:-$PROJECT_ROOT/.cache}/remus/update_dats}"
DOWNLOAD_CACHE_DIR="$UPDATE_DATS_CACHE_DIR/downloads"
CLONE_DIR="$UPDATE_DATS_CACHE_DIR/libretro-database"
CACHE_TTL_SECONDS="${UPDATE_DATS_CACHE_TTL_SECONDS:-86400}"
mame_xml_tmp=""
mame_bin_tmp=""
trap 'rm -rf "${redump_direct_tmp:-}" "${mame_xml_tmp:-}" "${mame_bin_tmp:-}" "${openvgdb_tmp:-}"' EXIT
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
echo "  cache/     → $UPDATE_DATS_CACHE_DIR"

mkdir -p "$TARGET_DIR" "$NO_INTRO_DIR" "$REDUMP_DIR" "$MAME_DIR" "$MAME_REDUMP_CHD_DIR" "$DOWNLOAD_CACHE_DIR"

is_cache_fresh() {
    local file_path="$1"
    [[ -f "$file_path" ]] || return 1
    [[ "$CACHE_TTL_SECONDS" =~ ^[0-9]+$ ]] || return 1
    local file_mtime now age
    file_mtime=$(stat -c %Y "$file_path" 2>/dev/null || return 1)
    now=$(date +%s)
    age=$((now - file_mtime))
    [[ "$age" -lt "$CACHE_TTL_SECONDS" ]]
}

download_with_cache() {
    local url="$1"
    local cache_path="$2"
    local max_time="$3"
    local label="$4"
    local -a curl_args=()
    shift 4
    if is_cache_fresh "$cache_path"; then
        echo "  Using cached $label: $(basename "$cache_path")" >&2
        return 0
    fi

    local cache_tmp
    mkdir -p "$(dirname "$cache_path")"
    cache_tmp="$(dirname "$cache_path")/.tmp.$$.$(basename "$cache_path")"
    curl_args=( -fsSL --retry 2 --retry-delay 1 --max-time "$max_time" -o "$cache_tmp" )
    if [[ "$#" -gt 0 ]]; then
        curl_args+=( "$@" )
    fi
    curl_args+=( "$url" )
    if curl "${curl_args[@]}"; then
        mv "$cache_tmp" "$cache_path"
    else
        rm -f "$cache_tmp"
        return 1
    fi
}

gametdb_payload_matches() {
    local existing_path="$1"
    local extracted_path="$2"
    [[ -f "$existing_path" && -f "$extracted_path" ]] || return 1

    python3 - "$existing_path" "$extracted_path" <<'PYEOF' >/dev/null
import pathlib
import re
import sys

ROOT_VERSION_RE = re.compile(r'\bversion="[^"]*"', re.IGNORECASE)

def normalized_text(path_str: str) -> str:
    text = pathlib.Path(path_str).read_text(encoding="utf-8")
    return ROOT_VERSION_RE.sub('version="__IGNORED__"', text)

sys.exit(0 if normalized_text(sys.argv[1]) == normalized_text(sys.argv[2]) else 1)
PYEOF
}

install_gametdb_xml() {
    local zip_path="$1"
    local xml_name="$2"
    local dest_path="$3"
    local tmp_dir tmp_path

    tmp_dir="$(mktemp -d)"
    if ! unzip -o -q "$zip_path" "$xml_name" -d "$tmp_dir" 2>/dev/null; then
        rm -rf "$tmp_dir"
        return 1
    fi

    tmp_path="$tmp_dir/$xml_name"
    if gametdb_payload_matches "$dest_path" "$tmp_path"; then
        rm -rf "$tmp_dir"
        printf 'unchanged\n'
        return 0
    fi

    mkdir -p "$(dirname "$dest_path")"
    mv "$tmp_path" "$dest_path"
    rm -rf "$tmp_dir"
    printf 'updated\n'
}

# Clone or update the repo (shallow clone to save bandwidth)
if [[ -d "$CLONE_DIR/.git" ]]; then
    echo "Updating cached libretro-database clone..."
    git -C "$CLONE_DIR" pull --ff-only --depth=1 2>/dev/null || \
        git -C "$CLONE_DIR" fetch --depth=1 origin master && \
        git -C "$CLONE_DIR" reset --hard origin/master
else
    echo "Cloning libretro-database into cache (shallow)..."
    git clone --depth=1 "$REPO_URL" "$CLONE_DIR"
fi

copied=0
skipped=0

# Helper: copy a DAT file to a target directory (skip when content unchanged)
copy_dat() {
    local src="$1"
    local dest_dir="$2"
    local basename
    basename="$(basename "$src")"
    local dest_path="$dest_dir/$basename"

    if [[ ! -f "$src" ]]; then
        return
    fi

    if [[ -f "$dest_path" ]]; then
        local src_hash dest_hash
        src_hash="$(compendium_sha256_of "$src")"
        dest_hash="$(compendium_sha256_of "$dest_path")"
        if [[ "$src_hash" == "$dest_hash" ]]; then
            skipped=$((skipped + 1))
            return
        fi
    fi

    cp "$src" "$dest_path"
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
    local cache_zip="$DOWNLOAD_CACHE_DIR/redump/${slug}.zip"
    local cache_source="$DOWNLOAD_CACHE_DIR/redump/${slug}.source"
    local base url source_url

    if is_cache_fresh "$cache_zip" \
        && unzip -Z -1 "$cache_zip" 2>/dev/null | grep -E '\.dat$' >/dev/null; then
        cp "$cache_zip" "$out_zip"
        if [[ -f "$cache_source" ]]; then
            source_url="$(<"$cache_source")"
        else
            source_url="unknown mirror"
        fi
        printf 'cached|%s\n' "$source_url"
        return 0
    fi

    for base in "${REDUMP_DIRECT_BASE_URLS[@]}"; do
        url="${base%/}/${slug}/"
        if download_with_cache "$url" "$cache_zip" 60 "Redump $slug ZIP"; then
            # Require at least one .dat entry to guard against HTML/error payloads.
            if unzip -Z -1 "$cache_zip" 2>/dev/null | grep -E '\.dat$' >/dev/null; then
                cp "$cache_zip" "$out_zip"
                printf '%s\n' "$url" > "$cache_source"
                printf 'downloaded|%s\n' "$url"
                return 0
            fi
            # Downloaded but invalid payload — evict so next run redownloads.
            rm -f "$cache_zip"
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
    if source_result="$(download_redump_zip_for_slug "$slug" "$zippath")"; then
        IFS='|' read -r source_kind source_url <<< "$source_result"
        extracted=$(unzip -Z -1 "$zippath" 2>/dev/null | grep -E '\.dat$' | head -1)
        if [[ -n "$extracted" ]]; then
            if unzip -o -q "$zippath" "$extracted" -d "$redump_direct_tmp" 2>/dev/null; then
                extracted_dat="$redump_direct_tmp/$extracted"
                dest_dat="$REDUMP_DIR/$destname"
                case "$(install_file_if_changed "$extracted_dat" "$dest_dat" 2>/dev/null || echo failed)" in
                    updated)
                        copied=$((copied + 1))
                        if [[ "$source_kind" == "cached" ]]; then
                            echo "    Updated from cached ZIP ($source_url)"
                        else
                            echo "    Updated from $source_url"
                        fi
                        ;;
                    unchanged)
                        echo "    Unchanged ($destname)"
                        ;;
                    *)
                        echo "    Warning: failed to install $destname"
                        ;;
                esac
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
        wiiu_cache_zip="$DOWNLOAD_CACHE_DIR/redump/wiiu.zip"
        rm -f "$wiiu_zip"

        if ! download_with_cache "$wiiu_url" "$wiiu_cache_zip" 120 "Wii U Redump ZIP"; then
            continue
        fi
        cp "$wiiu_cache_zip" "$wiiu_zip"

        wiiu_extracted=$(unzip -Z -1 "$wiiu_zip" 2>/dev/null | grep -E '\.dat$' | head -1)
        if [[ -z "$wiiu_extracted" ]]; then
            rm -f "$wiiu_cache_zip"
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
        mame_cache_zip="$DOWNLOAD_CACHE_DIR/mame/$(basename "$mame_lx_url")"
        echo "  Downloading $(basename "$mame_lx_url")..."
        if download_with_cache "$mame_lx_url" "$mame_cache_zip" 600 "MAME release ZIP"; then
            cp "$mame_cache_zip" "$mame_lx_zip"
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
        shopt -s nullglob
        for stale_dat in "$MAME_DIR"/MAME*.dat; do
            if [[ "$stale_dat" != "$mame_out_dat" ]]; then
                echo "  Removing stale MAME DAT: $(basename "$stale_dat")"
                rm -f -- "$stale_dat"
            fi
        done
        shopt -u nullglob
    else
        echo "  Warning: MAME XML → DAT conversion failed"
        rm -f "$mame_out_dat"
    fi
    # Also keep the raw listxml for the MAME listxml enrichment pass.
    mame_listxml_dest="$PROJECT_ROOT/data/mame/listxml.xml"
    mkdir -p "$(dirname "$mame_listxml_dest")"
    case "$(install_file_if_changed "$mame_xml_tmp" "$mame_listxml_dest" 2>/dev/null || echo failed)" in
        updated)
            echo "  MAME listxml updated: $mame_listxml_dest ($(du -sh "$mame_listxml_dest" | cut -f1))"
            ;;
        unchanged)
            echo "  MAME listxml unchanged: $mame_listxml_dest ($(du -sh "$mame_listxml_dest" | cut -f1))"
            ;;
        *)
            echo "  Warning: failed to install MAME listxml at $mame_listxml_dest"
            ;;
    esac
    # catver.ini — progetto-SNAPS category database for MAME arcade genre enrichment.
    mame_catver_dest="$PROJECT_ROOT/data/mame/catver.ini"
    mame_catver_url="https://raw.githubusercontent.com/AntoPISA/MAME_SupportFiles/main/catver.ini/catver.ini"
    mame_catver_cache="$DOWNLOAD_CACHE_DIR/mame/catver.ini"
    echo "  Updating MAME catver.ini..."
    if download_with_cache "$mame_catver_url" "$mame_catver_cache" 120 "MAME catver.ini"; then
        case "$(install_file_if_changed "$mame_catver_cache" "$mame_catver_dest")" in
            updated)
                echo "  MAME catver updated: $mame_catver_dest ($(du -sh "$mame_catver_dest" | cut -f1))"
                ;;
            unchanged)
                echo "  MAME catver unchanged: $mame_catver_dest"
                ;;
        esac
    else
        echo "  Warning: failed to download catver.ini from $mame_catver_url"
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
    # catver.ini can still be fetched without a local MAME binary.
    mame_catver_dest="$PROJECT_ROOT/data/mame/catver.ini"
    mame_catver_url="https://raw.githubusercontent.com/AntoPISA/MAME_SupportFiles/main/catver.ini/catver.ini"
    mame_catver_cache="$DOWNLOAD_CACHE_DIR/mame/catver.ini"
    echo ""
    echo "Updating MAME catver.ini..."
    if download_with_cache "$mame_catver_url" "$mame_catver_cache" 120 "MAME catver.ini"; then
        case "$(install_file_if_changed "$mame_catver_cache" "$mame_catver_dest")" in
            updated)
                echo "  MAME catver updated: $mame_catver_dest ($(du -sh "$mame_catver_dest" | cut -f1))"
                ;;
            unchanged)
                echo "  MAME catver unchanged: $mame_catver_dest"
                ;;
        esac
    else
        echo "  Warning: failed to download catver.ini from $mame_catver_url"
    fi
fi

# 4c. MAME Redump CHD DATs — Logiqx XML with <disk sha1="..."> entries for .chd matching.
#     Source: https://github.com/MetalSlug/MAMERedump (MAME Redump/ folder)
echo ""
echo "Downloading MAME Redump CHD DATs..."
mkdir -p "$MAME_REDUMP_CHD_DIR"
mame_redump_api_url="https://api.github.com/repos/MetalSlug/MAMERedump/contents/MAME%20Redump?ref=main"
mame_redump_cache_json="$DOWNLOAD_CACHE_DIR/mame-redump-chd/listing.json"
mame_redump_downloaded=0
mame_redump_skipped=0

if download_with_cache "$mame_redump_api_url" "$mame_redump_cache_json" 120 "MAME Redump CHD listing"; then
    while IFS= read -r dat_name; do
        [[ -n "$dat_name" ]] || continue
        dest_path="$MAME_REDUMP_CHD_DIR/$dat_name"
        download_url="https://raw.githubusercontent.com/MetalSlug/MAMERedump/main/MAME%20Redump/${dat_name// /%20}"
        cache_path="$DOWNLOAD_CACHE_DIR/mame-redump-chd/$dat_name"

        if download_with_cache "$download_url" "$cache_path" 120 "MAME Redump $dat_name"; then
            case "$(install_file_if_changed "$cache_path" "$dest_path")" in
                updated)
                    mame_redump_downloaded=$((mame_redump_downloaded + 1))
                    ;;
                unchanged)
                    mame_redump_skipped=$((mame_redump_skipped + 1))
                    ;;
            esac
        else
            echo "    Warning: failed to download MAME Redump DAT: $dat_name"
        fi
    done < <(python3 - "$mame_redump_cache_json" <<'PYEOF'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as fh:
    items = json.load(fh)

for item in sorted(items, key=lambda entry: entry.get("name", "")):
    name = item.get("name", "")
    if name.endswith(".dat"):
        print(name)
PYEOF
)
    echo "  MAME Redump CHD DATs: $mame_redump_downloaded updated, $mame_redump_skipped unchanged"
else
    echo "  Warning: failed to fetch MAME Redump CHD DAT listing from GitHub"
fi

# 5. metadat/hacks/ — ROM hack / translation patch DATs (libretro curated, romhacking.net URLs)
PATCHES_DIR="$PROJECT_ROOT/data/patches/hacks"
hacks_updated=0
hacks_unchanged=0
if [[ -d "$CLONE_DIR/metadat/hacks" ]]; then
    mkdir -p "$PATCHES_DIR"
    for dat in "$CLONE_DIR/metadat/hacks/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            c_before=$copied
            s_before=$skipped
            copy_dat "$dat" "$PATCHES_DIR"
            if [[ $copied -gt $c_before ]]; then
                hacks_updated=$((hacks_updated + 1))
            elif [[ $skipped -gt $s_before ]]; then
                hacks_unchanged=$((hacks_unchanged + 1))
            fi
        fi
    done
    echo "  Patch/hack DATs:    $hacks_updated updated, $hacks_unchanged unchanged in $PATCHES_DIR"
else
    echo "  Skipping hacks (metadat/hacks not found in libretro-database clone)"
fi

# 5b. Supplemental DAT sets — homebrew, libretro-dats (libretro curated); TOSEC is manual drop-in
SUPPLEMENTAL_DIR="$PROJECT_ROOT/data/databases/supplemental"
supplemental_updated=0
supplemental_unchanged=0
for supplemental_type in homebrew libretro-dats; do
    src_supplemental="$CLONE_DIR/metadat/$supplemental_type"
    dest_supplemental="$SUPPLEMENTAL_DIR/$supplemental_type"
    if [[ ! -d "$src_supplemental" ]]; then
        echo "  Skipping supplemental/$supplemental_type (not found in repo)"
        continue
    fi
    mkdir -p "$dest_supplemental"
    type_count=0
    for dat in "$src_supplemental/"*.dat; do
        [[ -f "$dat" ]] || continue
        if should_include "$dat"; then
            c_before=$copied
            s_before=$skipped
            copy_dat "$dat" "$dest_supplemental"
            if [[ $copied -gt $c_before ]]; then
                supplemental_updated=$((supplemental_updated + 1))
                type_count=$((type_count + 1))
            elif [[ $skipped -gt $s_before ]]; then
                supplemental_unchanged=$((supplemental_unchanged + 1))
                type_count=$((type_count + 1))
            fi
        fi
    done
    echo "  Supplemental/$supplemental_type: $type_count files"
done
if [[ -d "$SUPPLEMENTAL_DIR/tosec" ]]; then
    tosec_count=$(find "$SUPPLEMENTAL_DIR/tosec" -maxdepth 1 -type f -name '*.dat' 2>/dev/null | wc -l)
    echo "  Supplemental/tosec:     $tosec_count files (manual drop-in)"
fi

# 6. Metadata DATs (genre, developer, publisher, maxusers, releaseyear)
METADATA_DIR="$PROJECT_ROOT/data/metadata"
METADAT_TYPES=("genre" "developer" "publisher" "maxusers" "releaseyear" "description")
meta_updated=0
meta_unchanged=0

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
            c_before=$copied
            s_before=$skipped
            copy_dat "$dat" "$dest_dir"
            if [[ $copied -gt $c_before ]]; then
                meta_updated=$((meta_updated + 1))
            elif [[ $skipped -gt $s_before ]]; then
                meta_unchanged=$((meta_unchanged + 1))
            fi
        fi
    done
done

# 7. GameTDB XML databases (Wii/GameCube, DS, 3DS, WiiU, Switch, PS3)
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
    zippath="$DOWNLOAD_CACHE_DIR/gametdb/$zipname"
    destpath="$GAMETDB_DIR/$xmlname"

    echo "  Fetching $zipname..."
    if download_with_cache "$url" "$zippath" 300 "$zipname"; then
        if gametdb_result="$(install_gametdb_xml "$zippath" "$xmlname" "$destpath")"; then
            if [[ "$gametdb_result" == "updated" ]]; then
                gametdb_copied=$((gametdb_copied + 1))
            else
                echo "    Unchanged payload: $xmlname"
            fi
        else
            echo "    Warning: failed to extract $xmlname from $zipname"
        fi
    else
        echo "    Warning: failed to download $zipname"
    fi
done

echo ""
echo "Done: $copied DATs copied, $skipped skipped, $meta_updated metadata DATs updated ($meta_unchanged unchanged), $gametdb_copied GameTDB databases downloaded"

if [[ -x "$PROJECT_ROOT/scripts/update_hasheous_dumps.sh" ]]; then
    echo ""
    echo "==> Hasheous offline dumps (public API ZIP download)"
    bash "$PROJECT_ROOT/scripts/update_hasheous_dumps.sh" --all-core \
        || echo "  warning: Hasheous dump download failed (see above)" >&2
fi

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
openvgdb_cache_zip="$DOWNLOAD_CACHE_DIR/openvgdb/openvgdb.zip"
if download_with_cache "$OPENVGDB_URL" "$openvgdb_cache_zip" 300 "OpenVGDB ZIP"; then
    cp "$openvgdb_cache_zip" "$openvgdb_tmp/openvgdb.zip"
    if unzip -o -q "$openvgdb_tmp/openvgdb.zip" "openvgdb.sqlite" -d "$openvgdb_tmp" 2>/dev/null; then
        case "$(install_file_if_changed "$openvgdb_tmp/openvgdb.sqlite" "$OPENVGDB_DEST")" in
            updated)
                echo "  OpenVGDB updated: $OPENVGDB_DEST"
                ;;
            unchanged)
                echo "  OpenVGDB unchanged: $OPENVGDB_DEST"
                ;;
        esac
    else
        # Extraction failed — evict the cached ZIP so next run redownloads.
        rm -f "$openvgdb_cache_zip"
        echo "  Warning: failed to extract openvgdb.sqlite from zip (cache evicted)"
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
find "$MAME_REDUMP_CHD_DIR" -maxdepth 1 -name '*.dat' 2>/dev/null | wc -l | xargs -I{} echo "  mame-redump-chd/:   {} files in $MAME_REDUMP_CHD_DIR"
if [[ -d "$METADATA_DIR" ]]; then
    find "$METADATA_DIR" -name '*.dat' | wc -l | xargs -I{} echo "  metadata DATs:      {} files"
fi
if [[ -d "$GAMETDB_DIR" ]]; then
    find "$GAMETDB_DIR" -name '*.xml' | wc -l | xargs -I{} echo "  GameTDB XMLs:       {} files"
fi
if [[ -d "$PATCHES_DIR" ]]; then
    find "$PATCHES_DIR" -maxdepth 1 -name '*.dat' 2>/dev/null | wc -l | xargs -I{} echo "  patch/hack DATs:    {} files in $PATCHES_DIR"
fi
if [[ -f "$OPENVGDB_DEST" ]]; then
    echo "  OpenVGDB:           $OPENVGDB_DEST"
fi
MAME_LISTXML_PATH="$PROJECT_ROOT/data/mame/listxml.xml"
MAME_CATVER_PATH="$PROJECT_ROOT/data/mame/catver.ini"
if [[ ! -f "$MAME_LISTXML_PATH" ]]; then
    echo ""
    echo "==> MAME listxml missing — trying scripts/update_mame_listxml.sh fallback..."
    if bash "$PROJECT_ROOT/scripts/update_mame_listxml.sh"; then
        echo "  MAME listxml fallback succeeded: $MAME_LISTXML_PATH"
    else
        echo ""
        echo "WARNING: $MAME_LISTXML_PATH is missing."
        echo "  The MAME listxml enrichment pass will be skipped during compendium builds."
        echo "  Arcade developer/year/players metadata will not be populated from listxml."
        echo "  Install MAME (see section 4b above) and re-run this script, or place listxml.xml manually."
    fi
fi
if [[ ! -f "$MAME_CATVER_PATH" ]]; then
    echo ""
    echo "WARNING: $MAME_CATVER_PATH is missing."
    echo "  The MAME catver enrichment pass will be skipped during compendium builds."
    echo "  Arcade genre metadata will not be populated from catver.ini."
    echo "  Re-run this script when network access is available, or place catver.ini manually."
fi

if [[ -x "$PROJECT_ROOT/scripts/update_enrichment_fingerprint_sidecar.sh" ]]; then
    metadata_count=0
    metadata_mtime=0
    if [[ -d "$PROJECT_ROOT/data/metadata" ]]; then
        metadata_count="$(find "$PROJECT_ROOT/data/metadata" -type f 2>/dev/null | wc -l | tr -d ' ')"
        metadata_mtime="$(find "$PROJECT_ROOT/data/metadata" -type f -printf '%T@\n' 2>/dev/null | sort -n | tail -1 | cut -d. -f1)"
    fi
    openvgdb_sha=""
    if [[ -f "$OPENVGDB_DEST" ]]; then
        openvgdb_sha="$(compendium_sha256_of "$OPENVGDB_DEST")"
    fi
    mame_listxml_sha=""
    if [[ -f "$MAME_LISTXML_PATH" ]]; then
        mame_listxml_sha="$(compendium_sha256_of "$MAME_LISTXML_PATH")"
    fi
    bash "$PROJECT_ROOT/scripts/update_enrichment_fingerprint_sidecar.sh" metadata \
        "{\"file_count\":${metadata_count:-0},\"root_mtime\":${metadata_mtime:-0}}" || true
    bash "$PROJECT_ROOT/scripts/update_enrichment_fingerprint_sidecar.sh" openvgdb \
        "{\"sqlite_sha256\":\"${openvgdb_sha}\"}" || true
    bash "$PROJECT_ROOT/scripts/update_enrichment_fingerprint_sidecar.sh" mame_listxml \
        "{\"xml_sha256\":\"${mame_listxml_sha}\"}" || true
fi

echo ""
echo "Next step: run scripts/generate_compendium_manifest.sh to update the manifest."
