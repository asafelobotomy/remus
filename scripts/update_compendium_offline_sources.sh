#!/usr/bin/env bash
set -euo pipefail
# Ensure all offline compendium inputs are present before a build.
# Delegates to per-source updaters; each skips files when content is unchanged.
#
# Usage:
#   scripts/update_compendium_offline_sources.sh [--strict-offline]
#
# Called automatically by scripts/build_compendium_full.sh unless --skip-update.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STRICT_OFFLINE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict-offline)
            STRICT_OFFLINE=true
            shift
            ;;
        -h | --help)
            echo "usage: $(basename "$0") [--strict-offline]"
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

check_offline_mirrors() {
    local manifest="$ROOT_DIR/data/compendium/offline-mirrors.json"
    if [[ ! -f "$manifest" ]]; then
        echo "error: missing offline mirror manifest: $manifest" >&2
        exit 1
    fi
    if ! command -v jq >/dev/null 2>&1; then
        echo "error: jq is required for --strict-offline mirror checks" >&2
        exit 1
    fi

    local missing=0

    check_required_file() {
        local rel_path="$1" min_bytes="$2" hint="$3"
        local abs_path="$ROOT_DIR/$rel_path"
        local size=0
        if [[ -f "$abs_path" ]]; then
            size="$(stat -c%s "$abs_path" 2>/dev/null || echo 0)"
        fi
        if [[ ! -f "$abs_path" || "$size" -lt "$min_bytes" ]]; then
            echo "error: required offline mirror missing or too small: $rel_path (need >= ${min_bytes} bytes)" >&2
            echo "hint: run $hint" >&2
            missing=$((missing + 1))
        fi
    }

    count_dir_glob() {
        local abs_dir="$1" glob_pattern="$2" exclude_name="${3:-}"
        if [[ ! -d "$abs_dir" ]]; then
            echo 0
            return 0
        fi
        if [[ "$glob_pattern" == *"**"* ]]; then
            local suffix="${glob_pattern#**/}"
            if [[ -n "$exclude_name" ]]; then
                find "$abs_dir" -type f -name "$suffix" ! -name "$exclude_name" 2>/dev/null | wc -l | tr -d ' '
            else
                find "$abs_dir" -type f -name "$suffix" 2>/dev/null | wc -l | tr -d ' '
            fi
        else
            if [[ -n "$exclude_name" ]]; then
                find "$abs_dir" -maxdepth 1 -type f -name "$glob_pattern" ! -name "$exclude_name" 2>/dev/null | wc -l | tr -d ' '
            else
                find "$abs_dir" -maxdepth 1 -type f -name "$glob_pattern" 2>/dev/null | wc -l | tr -d ' '
            fi
        fi
    }

    check_required_directory() {
        local rel_path="$1" glob_pattern="$2" min_files="$3" hint="$4" exclude_name="${5:-}"
        local abs_path="$ROOT_DIR/$rel_path"
        local count
        count="$(count_dir_glob "$abs_path" "$glob_pattern" "$exclude_name")"
        if [[ "${count:-0}" -lt "$min_files" ]]; then
            echo "error: required offline directory under-populated: $rel_path (need >= ${min_files} files matching $glob_pattern, found ${count:-0})" >&2
            echo "hint: run $hint" >&2
            missing=$((missing + 1))
        fi
    }

    # Legacy required[] (backward compatible)
    while IFS=$'\t' read -r rel_path min_bytes hint; do
        [[ -n "$rel_path" ]] || continue
        check_required_file "$rel_path" "$min_bytes" "$hint"
    done < <(jq -r '.required[]? | [.path, (.min_bytes|tostring), (.hint // "scripts/update_dats.sh --all")] | @tsv' "$manifest")

    # required_files[]
    while IFS=$'\t' read -r rel_path min_bytes hint; do
        [[ -n "$rel_path" ]] || continue
        check_required_file "$rel_path" "$min_bytes" "$hint"
    done < <(jq -r '.required_files[]? | [.path, (.min_bytes|tostring), (.hint // "scripts/update_dats.sh --all")] | @tsv' "$manifest")

    # required_directories[]
    while IFS=$'\t' read -r rel_path glob_pattern min_files hint exclude_name; do
        [[ -n "$rel_path" ]] || continue
        check_required_directory "$rel_path" "$glob_pattern" "$min_files" "$hint" "$exclude_name"
    done < <(jq -r '.required_directories[]? | [.path, .glob, (.min_files|tostring), (.hint // "scripts/update_dats.sh --all"), (.exclude_glob // "")] | @tsv' "$manifest")

    # required_one_of[] — at least one option must satisfy its gate
    local one_of_ids
    one_of_ids="$(jq -r '.required_one_of[]?.id // empty' "$manifest")"
    while IFS= read -r group_id; do
        [[ -n "$group_id" ]] || continue
        local hint satisfied=0
        hint="$(jq -r --arg id "$group_id" '.required_one_of[] | select(.id == $id) | .hint // "see offline-mirrors.json"' "$manifest")"
        local option_count
        option_count="$(jq -r --arg id "$group_id" '.required_one_of[] | select(.id == $id) | .options | length' "$manifest")"
        local i
        for ((i = 0; i < option_count; i++)); do
            local rel_path min_bytes min_files glob_pattern
            rel_path="$(jq -r --arg id "$group_id" --argjson idx "$i" '.required_one_of[] | select(.id == $id) | .options[$idx].path' "$manifest")"
            min_bytes="$(jq -r --arg id "$group_id" --argjson idx "$i" '.required_one_of[] | select(.id == $id) | .options[$idx].min_bytes // 0' "$manifest")"
            min_files="$(jq -r --arg id "$group_id" --argjson idx "$i" '.required_one_of[] | select(.id == $id) | .options[$idx].min_files // 0' "$manifest")"
            glob_pattern="$(jq -r --arg id "$group_id" --argjson idx "$i" '.required_one_of[] | select(.id == $id) | .options[$idx].glob // ""' "$manifest")"
            local abs_path="$ROOT_DIR/$rel_path"
            if [[ -n "$glob_pattern" && "$min_files" -gt 0 ]]; then
                local count
                count="$(count_dir_glob "$abs_path" "$glob_pattern")"
                if [[ "${count:-0}" -ge "$min_files" ]]; then
                    satisfied=1
                    break
                fi
            elif [[ "$min_bytes" -gt 0 ]]; then
                local size=0
                if [[ -f "$abs_path" ]]; then
                    size="$(stat -c%s "$abs_path" 2>/dev/null || echo 0)"
                fi
                if [[ -f "$abs_path" && "$size" -ge "$min_bytes" ]]; then
                    satisfied=1
                    break
                fi
            elif [[ -e "$abs_path" ]]; then
                satisfied=1
                break
            fi
        done
        if [[ "$satisfied" -ne 1 ]]; then
            echo "error: required offline one_of group not satisfied: $group_id" >&2
            echo "hint: $hint" >&2
            missing=$((missing + 1))
        fi
    done <<<"$one_of_ids"

    if [[ "$missing" -gt 0 ]]; then
        echo "error: $missing required offline mirror check(s) failed (--strict-offline)" >&2
        exit 1
    fi
    echo "==> Strict offline mirror preflight passed"
}

if $STRICT_OFFLINE; then
    check_offline_mirrors
fi

echo "==> Compendium offline sources (skip unchanged files)"
bash "$ROOT_DIR/scripts/update_dats.sh" --all

echo ""
echo "==> LaunchBox metadata (auto-download when missing)"
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
