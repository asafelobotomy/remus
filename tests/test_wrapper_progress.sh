#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/compendium_db_guard.sh"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
progress_file="$tmpdir/remus_compendium.db.progress.json"

write_wrapper_progress "$progress_file" "dat_update" "in_progress" 0 "Updating offline sources"
phase="$(compendium_progress_build_phase "$progress_file")"
[[ "$phase" == "dat_update" ]]

write_wrapper_progress "$progress_file" "manifest" "in_progress" 3 "Generating manifest"
pct="$(jq -r '.overall_pct' "$progress_file")"
[[ "$pct" == "3" ]]

write_wrapper_progress "$progress_file" "complete" "complete" 100 "Done"
status="$(jq -r '.status' "$progress_file")"
[[ "$status" == "complete" ]]

echo "write_wrapper_progress ok"
