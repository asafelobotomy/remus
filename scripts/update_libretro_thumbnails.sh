#!/usr/bin/env bash
# Acquire libretro-thumbnails system packs into data/acquisition/libretro-thumbnails/.
# Uses shallow git submodules from the active libretro-thumbnails meta repository.
#
# Usage:
#   scripts/update_libretro_thumbnails.sh [--system "Sega - Mega Drive - Genesis"] [--all]
#   scripts/update_libretro_thumbnails.sh --help
#
# Skips re-fetch when per-system SHA-256 marker matches the submodule HEAD tree.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/compendium_offline_helpers.sh
source "$ROOT_DIR/scripts/compendium_offline_helpers.sh"

META_REPO_URL="https://github.com/libretro-thumbnails/libretro-thumbnails.git"
ACQUISITION_ROOT="$ROOT_DIR/data/acquisition/libretro-thumbnails"
MARKER_SUFFIX=".remus-sync-sha256"

# Aligned with scripts/update_dats.sh CORE_SYSTEMS
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

sync_all=false
declare -a REQUESTED_SYSTEMS=()

usage() {
    cat <<'USAGE'
Usage:
  scripts/update_libretro_thumbnails.sh [options]

Options:
  --system <name>   Sync one libretro-thumbnails system (repeatable)
  --all             Sync all CORE_SYSTEMS (default when no --system given)
  --help            Show this help

Examples:
  scripts/update_libretro_thumbnails.sh --system "Sega - Mega Drive - Genesis"
  scripts/update_libretro_thumbnails.sh --all

Acquisition root:
  data/acquisition/libretro-thumbnails/

Recovery:
  If a submodule is corrupt, remove its directory under the acquisition root and re-run.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --system)
            REQUESTED_SYSTEMS+=("$2")
            shift 2
            ;;
        --all)
            sync_all=true
            shift
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

if ! command -v git >/dev/null 2>&1; then
    echo "error: git is required" >&2
    exit 1
fi

if [[ ${#REQUESTED_SYSTEMS[@]} -eq 0 ]]; then
    sync_all=true
fi

systems_to_sync=()
if $sync_all; then
    systems_to_sync=("${CORE_SYSTEMS[@]}")
else
    systems_to_sync=("${REQUESTED_SYSTEMS[@]}")
fi

is_core_system() {
    local needle="$1"
    local sys
    for sys in "${CORE_SYSTEMS[@]}"; do
        if [[ "$sys" == "$needle" ]]; then
            return 0
        fi
    done
    return 1
}

ensure_meta_repo() {
    if [[ -d "$ACQUISITION_ROOT/.git" ]]; then
        echo "==> Updating libretro-thumbnails meta repo"
        git -C "$ACQUISITION_ROOT" fetch --depth 1 origin main 2>/dev/null \
            || git -C "$ACQUISITION_ROOT" fetch --depth 1 origin master 2>/dev/null \
            || true
        git -C "$ACQUISITION_ROOT" pull --ff-only 2>/dev/null || true
        return 0
    fi

    echo "==> Cloning libretro-thumbnails meta repo (shallow)"
    mkdir -p "$(dirname "$ACQUISITION_ROOT")"
    git clone --depth 1 "$META_REPO_URL" "$ACQUISITION_ROOT"
}

system_tree_hash() {
    local system="$1"
    local system_dir="$ACQUISITION_ROOT/$system"
    if [[ -d "$system_dir/.git" ]] || [[ -f "$system_dir/.git" ]]; then
        git -C "$system_dir" rev-parse HEAD 2>/dev/null || true
        return 0
    fi
    if [[ -d "$system_dir" ]]; then
        find "$system_dir" -type f -name '*.png' 2>/dev/null | sort | head -c 4096 | compendium_sha256_of /dev/stdin 2>/dev/null || echo "missing"
    fi
}

sync_system() {
    local system="$1"
    if ! is_core_system "$system"; then
        echo "  warning: '$system' is not in CORE_SYSTEMS — syncing anyway" >&2
    fi

    local system_dir="$ACQUISITION_ROOT/$system"
    local marker_file="$system_dir$MARKER_SUFFIX"

    echo "==> libretro-thumbnails: $system"

    if ! git -C "$ACQUISITION_ROOT" submodule status "$system" >/dev/null 2>&1; then
        echo "  warning: no submodule entry for '$system' in meta repo — skipping" >&2
        return 0
    fi

    git -C "$ACQUISITION_ROOT" submodule update --init --depth 1 -- "$system"

    if [[ ! -d "$system_dir" ]]; then
        echo "  warning: submodule checkout missing: $system_dir" >&2
        return 0
    fi

    local new_hash=""
    new_hash="$(system_tree_hash "$system")"
    if [[ -z "$new_hash" ]]; then
        echo "  warning: no PNG files found under $system_dir" >&2
        return 0
    fi

    if [[ -f "$marker_file" ]] && [[ "$(cat "$marker_file")" == "$new_hash" ]]; then
        echo "  unchanged (marker match)"
        return 0
    fi

    printf '%s\n' "$new_hash" >"$marker_file"
    local png_count
    png_count=$(find "$system_dir" -type f -name '*.png' 2>/dev/null | wc -l | tr -d ' ')
    echo "  updated ($png_count PNG files)"
}

echo "=== Remus libretro-thumbnails acquisition ==="
echo "Target: $ACQUISITION_ROOT"

ensure_meta_repo

updated=0
unchanged=0
skipped=0

for system in "${systems_to_sync[@]}"; do
    before_hash=""
    marker_file="$ACQUISITION_ROOT/$system$MARKER_SUFFIX"
    if [[ -f "$marker_file" ]]; then
        before_hash="$(cat "$marker_file")"
    fi

    if sync_system "$system"; then
        after_hash=""
        if [[ -f "$marker_file" ]]; then
            after_hash="$(cat "$marker_file")"
        fi
        if [[ -n "$before_hash" && "$before_hash" == "$after_hash" ]]; then
            unchanged=$((unchanged + 1))
        else
            updated=$((updated + 1))
        fi
    else
        skipped=$((skipped + 1))
    fi
done

echo ""
echo "libretro-thumbnails sync complete: updated=$updated unchanged=$unchanged skipped=$skipped"
