#!/usr/bin/env bash
# purpose: generate a compendium build manifest from DAT files in a directory.
# when: use before --build-compendium for bulk/full catalogue imports; not needed for hand-written manifests.
# inputs: --dat-dir <path>, --output <path>, --build-id <id>, --snapshot-label <label>, --fetched-at <iso8601>
# outputs: JSON manifest file with one dat source entry per DAT file.
# risk: safe
# source: original
# platform: Linux/macOS; requires bash 4+, sha256sum or shasum, and python3 (for portable relative paths)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DAT_DIR="$ROOT_DIR/data/databases"
OUTPUT_PATH="$ROOT_DIR/data/compendium/compendium-manifest-full.json"
DATE_STAMP="$(date -u +%Y-%m-%d)"
BUILD_ID="full-catalogue-${DATE_STAMP}"
SNAPSHOT_LABEL="libretro-database ${DATE_STAMP}"
FETCHED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

usage() {
    cat <<'USAGE'
Usage:
  scripts/generate_compendium_manifest.sh [options]

Options:
  --dat-dir <path>         Directory containing .dat files (default: data/databases)
  --output <path>          Output manifest path (default: data/compendium/compendium-manifest-full.json)
  --build-id <id>          Manifest build_id value
  --snapshot-label <text>  Snapshot label for all sources
  --fetched-at <iso8601>   fetched_at timestamp for all sources
  -h, --help               Show this help
USAGE
}

json_escape() {
    local value="$1"
    value="${value//\\/\\\\}"
    value="${value//\"/\\\"}"
    value="${value//$'\n'/\\n}"
    printf '%s' "$value"
}

slugify() {
    local value="$1"
    printf '%s' "$value" \
        | tr '[:upper:]' '[:lower:]' \
        | sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//'
}

# Compute the SHA-256 hex digest of a file.
# Uses sha256sum (GNU/Linux) or falls back to shasum (macOS/BSD).
sha256_of() {
    local file="$1"
    if command -v sha256sum &>/dev/null; then
        sha256sum "$file" | awk '{print $1}'
    elif command -v shasum &>/dev/null; then
        shasum -a 256 "$file" | awk '{print $1}'
    else
        echo "error: no sha256 utility (sha256sum or shasum) found" >&2
        exit 1
    fi
}

# Compute the path of $1 relative to $2.
# Uses python3 for portability (Linux and macOS); falls back to GNU realpath.
compute_relative_path() {
    local target="$1" base="$2"
    if command -v python3 &>/dev/null; then
        python3 -c "import os,sys; print(os.path.relpath(sys.argv[1],sys.argv[2]))" "$target" "$base"
    else
        realpath --relative-to="$base" "$target"
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dat-dir)
            DAT_DIR="$2"
            shift 2
            ;;
        --output)
            OUTPUT_PATH="$2"
            shift 2
            ;;
        --build-id)
            BUILD_ID="$2"
            shift 2
            ;;
        --snapshot-label)
            SNAPSHOT_LABEL="$2"
            shift 2
            ;;
        --fetched-at)
            FETCHED_AT="$2"
            shift 2
            ;;
        -h|--help)
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

if [[ ! -d "$DAT_DIR" ]]; then
    echo "error: DAT directory does not exist: $DAT_DIR" >&2
    exit 1
fi

DAT_DIR="$(cd "$DAT_DIR" && pwd)"

# Source IDs that produce zero signatures because the system has no compendium
# mapping (engine-based platforms, scripting environments, ROM hacks, etc.).
# Disabled rather than removed so they still appear in the manifest for auditing.
EXCLUDED_SOURCE_IDS=(
    "libretro-dat-mobile-j2me"          # 214 K items, no system mapping
    "libretro-dat-scummvm"              # engine-only
    "libretro-dat-hbmame"               # MAME hack set, no mapping
    "libretro-dat-pico-8"               # scripting env
    "libretro-dat-tic-80"               # scripting env
    "libretro-dat-puzzlescript"         # scripting env
    "libretro-dat-doom"                 # engine WADs
    "libretro-dat-lowres-nx"            # fantasy console
    "libretro-dat-wasm-4"               # fantasy console
    "libretro-dat-uzebox"               # homebrew micro
    "libretro-dat-handheld-electronic-game" # LCD games, no mapping
    "libretro-dat-vircon32"             # fantasy console
    "libretro-dat-chip-8"               # interpreter
    "libretro-dat-cave-story"           # single game
    "libretro-dat-infocom-z-machine"    # interpreter
    "libretro-dat-rick-dangerous"       # single game
    "libretro-dat-flashback"            # single game
    "libretro-dat-dinothawr"            # single game
    "libretro-dat-cannonball"           # single game
    "libretro-dat-mrboom"               # single game
    "libretro-dat-quake"                # engine WADs
    "libretro-dat-quake-ii"             # engine WADs
    "libretro-dat-quake-iii"            # engine WADs
    "libretro-dat-wolfenstein-3d"       # engine WADs
    "libretro-dat-chailove"             # scripting env
    "libretro-dat-tomb-raider"          # engine assets
    "libretro-dat-jump-n-bump"          # single game
    "libretro-dat-rpg-maker"            # engine assets
    "libretro-dat-microw8"              # fantasy console
    "libretro-dat-arduboy-inc-arduboy"  # duplicate of nointro variant
    "libretro-dat-system"               # meta entry
    "libretro-dat-elektor-tv-games-computer" # no system mapping
    "libretro-dat-dice"                 # no system mapping
    "libretro-nointro-mobile-j2me"      # no system mapping
    # Superseded by libretro-redump/nointro counterpart: curated dat/ snapshot
    # is either near-empty or has a <15% match rate vs 87-100% for the full set.
    "libretro-dat-sega-saturn"                          # 4 items / 0 games; redump: 2102 items / 87%
    "libretro-dat-nintendo-super-nintendo-entertainment-system" # 25 items; nointro: 4255 items
    "libretro-dat-nec-pc-98"                            # 6 items; redump: 109 items
    "libretro-dat-nintendo-wii"                         # 12% match; redump: 97%
    # Hashless GameTDB catalogue (serial/metadata only, no crc/md5/sha1). Inflates
    # game counts without verification capability; use Redump + Digital No-Intro instead.
    "libretro-dat-nintendo-wii-u"
    "libretro-dat-sony-playstation-3"                   # 12% match; redump: 99%
    "libretro-dat-nintendo-gamecube"                    # 49% match; redump: 97%
    "libretro-dat-microsoft-xbox-360"                   # 52% match; nointro: 100%
    # Superseded: nointro/dat counterpart has much higher match rate
    "libretro-nointro-sega-32x"                         # 43% match; dat: 98%
    "libretro-redump-microsoft-xbox-360"                # 51% match; nointro: 100%
    # Zero-item sources (no content in libretro-database)
    "libretro-dat-lutro"                                # 0 items; Lutro is a scripting framework
    "libretro-dat-mobile-zeebo"                         # 0 items
    "libretro-nointro-mobile-zeebo"                     # 0 items
    "libretro-dat-microsoft-xbox-360-games-on-demand"   # 0 items
    "libretro-nointro-microsoft-xbox-360-games-on-demand" # 0 items
)

is_excluded() {
    local id="$1"
    for excluded in "${EXCLUDED_SOURCE_IDS[@]}"; do
        [[ "$id" == "$excluded" ]] && return 0
    done
    return 1
}

mapfile -d '' DAT_FILES < <(find "$DAT_DIR" -maxdepth 1 -type f -name '*.dat' -print0 | sort -z)
mapfile -d '' NO_INTRO_FILES < <(find "$DAT_DIR/no-intro" -maxdepth 1 -type f -name '*.dat' -print0 2>/dev/null | sort -z)
mapfile -d '' REDUMP_FILES < <(find "$DAT_DIR/redump" -maxdepth 1 -type f -name '*.dat' -print0 2>/dev/null | sort -z)
mapfile -d '' MAME_FILES < <(find "$DAT_DIR/mame" -maxdepth 1 -type f -name '*.dat' -print0 2>/dev/null | sort -z)
mapfile -d '' MAME_REDUMP_CHD_FILES < <(find "$DAT_DIR/mame-redump-chd" -maxdepth 1 -type f -name '*.dat' -print0 2>/dev/null | sort -z)

ALL_FILES=()
ALL_PREFIXES=()
ALL_PRIORITIES=()

for f in "${DAT_FILES[@]}"; do
    ALL_FILES+=("$f")
    ALL_PREFIXES+=("libretro-dat")
    ALL_PRIORITIES+=("10")
done
for f in "${NO_INTRO_FILES[@]}"; do
    ALL_FILES+=("$f")
    ALL_PREFIXES+=("libretro-nointro")
    ALL_PRIORITIES+=("20")
done
for f in "${MAME_FILES[@]}"; do
    ALL_FILES+=("$f")
    ALL_PREFIXES+=("mame-official")
    ALL_PRIORITIES+=("25")
done
for f in "${MAME_REDUMP_CHD_FILES[@]}"; do
    ALL_FILES+=("$f")
    ALL_PREFIXES+=("mame-redump-chd")
    ALL_PRIORITIES+=("35")
done
for f in "${REDUMP_FILES[@]}"; do
    ALL_FILES+=("$f")
    ALL_PREFIXES+=("libretro-redump")
    ALL_PRIORITIES+=("30")
done

if [[ ${#ALL_FILES[@]} -eq 0 ]]; then
    echo "error: no .dat files found in $DAT_DIR (including no-intro/ and redump/ subdirs)" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_PATH")"
OUTPUT_DIR="$(cd "$(dirname "$OUTPUT_PATH")" && pwd)"
OUTPUT_PATH="$OUTPUT_DIR/$(basename "$OUTPUT_PATH")"

# Slugs covered by No-Intro or Redump — libretro-dat entries with the same slug are
# disabled to avoid shadowed ingest (signatures owned by lower-priority curated DATs).
SUPERSEDING_SLUGS=()
for f in "${NO_INTRO_FILES[@]}" "${REDUMP_FILES[@]}"; do
    [[ -f "$f" ]] || continue
    dat_name="$(basename "$f")"
    dat_stem="${dat_name%.dat}"
    SUPERSEDING_SLUGS+=("$(slugify "$dat_stem")")
done

slug_is_superseded() {
    local slug="$1"
    for s in "${SUPERSEDING_SLUGS[@]}"; do
        [[ "$slug" == "$s" ]] && return 0
    done
    return 1
}

declare -A CHECKSUM_BY_FILE=()
if command -v python3 >/dev/null 2>&1; then
    while IFS=$'\t' read -r dat_path digest; do
        [[ -n "$dat_path" && -n "$digest" ]] || continue
        CHECKSUM_BY_FILE["$dat_path"]="$digest"
    done < <(python3 - "${ALL_FILES[@]}" <<'PY'
import hashlib
import sys
from concurrent.futures import ThreadPoolExecutor

def digest(path: str) -> tuple[str, str]:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return path, h.hexdigest()

files = sys.argv[1:]
with ThreadPoolExecutor() as pool:
    for path, value in pool.map(digest, files):
        print(f"{path}\t{value}")
PY
)
fi

{
    printf '{\n'
    printf '  "build_id": "%s",\n' "$(json_escape "$BUILD_ID")"
    printf '  "schema_version": 1,\n'
    printf '  "sources": [\n'

    for i in "${!ALL_FILES[@]}"; do
        dat_file="${ALL_FILES[$i]}"
        dat_prefix="${ALL_PREFIXES[$i]}"
        dat_priority="${ALL_PRIORITIES[$i]}"
        dat_name="$(basename "$dat_file")"
        dat_stem="${dat_name%.dat}"
        slug="$(slugify "$dat_stem")"

        source_id="${dat_prefix}-${slug}"
        snapshot_id="${source_id}-${DATE_STAMP}"
        checksum_sha256="${CHECKSUM_BY_FILE[$dat_file]:-}"
        if [[ -z "$checksum_sha256" ]]; then
            checksum_sha256="$(sha256_of "$dat_file")"
        fi

        rel_path="$(compute_relative_path "$dat_file" "$OUTPUT_DIR")"

        printf '    {\n'
        printf '      "source_id": "%s",\n' "$(json_escape "$source_id")"
        case "$dat_prefix" in
            mame-official)    display_prefix="MAME DAT" ;;
            mame-redump-chd)  display_prefix="MAME Redump CHD DAT" ;;
            libretro-redump)  display_prefix="Redump DAT" ;;
            libretro-nointro) display_prefix="No-Intro DAT" ;;
            *)                display_prefix="Libretro DAT" ;;
        esac
        printf '      "display_name": "%s",\n' "$(json_escape "$display_prefix: $dat_stem")"
        printf '      "source_type": "dat",\n'
        printf '      "snapshot_id": "%s",\n' "$(json_escape "$snapshot_id")"
        printf '      "snapshot_label": "%s",\n' "$(json_escape "$SNAPSHOT_LABEL")"
        printf '      "snapshot_ref": "",\n'
        printf '      "path": "%s",\n' "$(json_escape "$rel_path")"
        printf '      "checksum_sha256": "%s",\n' "$(json_escape "$checksum_sha256")"
        printf '      "license_id": "CC-BY-SA-4.0",\n'
        printf '      "license_url": "https://creativecommons.org/licenses/by-sa/4.0/",\n'
        printf '      "fetched_at": "%s",\n' "$(json_escape "$FETCHED_AT")"
        if is_excluded "$source_id"; then
            printf '      "enabled": false,\n'
        elif [[ "$dat_prefix" == "libretro-dat" ]] && slug_is_superseded "$slug"; then
            printf '      "enabled": false,\n'
        else
            printf '      "enabled": true,\n'
        fi
        printf '      "priority": %s,\n' "$dat_priority"
        printf '      "attribution_required": true\n'

        if [[ "$i" -lt $((${#ALL_FILES[@]} - 1)) ]]; then
            printf '    },\n'
        else
            printf '    }\n'
        fi
    done

    printf '  ]\n'
    printf '}\n'
} > "$OUTPUT_PATH"

echo "Manifest written: $OUTPUT_PATH"
echo "Sources: ${#ALL_FILES[@]} total (${#DAT_FILES[@]} curated, ${#NO_INTRO_FILES[@]} no-intro, ${#MAME_FILES[@]} mame, ${#MAME_REDUMP_CHD_FILES[@]} mame-redump-chd, ${#REDUMP_FILES[@]} redump)"
