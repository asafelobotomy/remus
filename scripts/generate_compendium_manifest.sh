#!/usr/bin/env bash
# purpose: generate a compendium build manifest from DAT files in a directory.
# when: use before --build-compendium for bulk/full catalogue imports; not needed for hand-written manifests.
# inputs: --dat-dir <path>, --output <path>, --build-id <id>, --snapshot-label <label>, --fetched-at <iso8601>
# outputs: JSON manifest file with one dat source entry per DAT file.
# risk: safe
# source: original

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

mapfile -d '' DAT_FILES < <(find "$DAT_DIR" -maxdepth 1 -type f -name '*.dat' -print0 | sort -z)
if [[ ${#DAT_FILES[@]} -eq 0 ]]; then
    echo "error: no .dat files found in $DAT_DIR" >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_PATH")"

{
    printf '{\n'
    printf '  "build_id": "%s",\n' "$(json_escape "$BUILD_ID")"
    printf '  "schema_version": 1,\n'
    printf '  "sources": [\n'

    for i in "${!DAT_FILES[@]}"; do
        dat_file="${DAT_FILES[$i]}"
        dat_name="$(basename "$dat_file")"
        dat_stem="${dat_name%.dat}"
        slug="$(slugify "$dat_stem")"

        source_id="libretro-dat-${slug}"
        snapshot_id="${source_id}-${DATE_STAMP}"
        checksum_sha256="$(sha256sum "$dat_file" | awk '{print $1}')"

        if [[ "$dat_file" == "$ROOT_DIR/"* ]]; then
            rel_path="${dat_file#"$ROOT_DIR/"}"
        else
            rel_path="$dat_file"
        fi

        printf '    {\n'
        printf '      "source_id": "%s",\n' "$(json_escape "$source_id")"
        printf '      "display_name": "%s",\n' "$(json_escape "Libretro DAT: $dat_stem")"
        printf '      "source_type": "dat",\n'
        printf '      "snapshot_id": "%s",\n' "$(json_escape "$snapshot_id")"
        printf '      "snapshot_label": "%s",\n' "$(json_escape "$SNAPSHOT_LABEL")"
        printf '      "snapshot_ref": "",\n'
        printf '      "path": "%s",\n' "$(json_escape "$rel_path")"
        printf '      "checksum_sha256": "%s",\n' "$(json_escape "$checksum_sha256")"
        printf '      "license_id": "CC-BY-SA-4.0",\n'
        printf '      "license_url": "https://creativecommons.org/licenses/by-sa/4.0/",\n'
        printf '      "fetched_at": "%s",\n' "$(json_escape "$FETCHED_AT")"
        printf '      "enabled": true,\n'
        printf '      "priority": 10,\n'
        printf '      "attribution_required": true\n'

        if [[ "$i" -lt $((${#DAT_FILES[@]} - 1)) ]]; then
            printf '    },\n'
        else
            printf '    }\n'
        fi
    done

    printf '  ]\n'
    printf '}\n'
} > "$OUTPUT_PATH"

echo "Manifest written: $OUTPUT_PATH"
echo "Sources: ${#DAT_FILES[@]}"
