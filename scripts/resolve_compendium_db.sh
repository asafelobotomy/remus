#!/usr/bin/env bash
# Resolve the absolute path to the Remus compendium SQLite database.
#
# SQLTools (and some other editors) require a hardcoded absolute path for SQLite
# connections — ${workspaceFolder} is not expanded reliably.
#
# Usage:
#   scripts/resolve_compendium_db.sh                 # print absolute db path
#   scripts/resolve_compendium_db.sh --ensure        # bootstrap if missing
#   scripts/resolve_compendium_db.sh --json          # SQLTools connection JSON
#   scripts/resolve_compendium_db.sh --ensure --json
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_DB="$ROOT_DIR/data/compendium/remus_compendium.db"

ENSURE=0
JSON=0

usage() {
    cat <<'EOF'
resolve_compendium_db.sh — locate (or bootstrap) remus_compendium.db

Options:
  --ensure   Run scripts/setup_compendium_db.sh when no valid database is found
  --json     Emit a sqltools.connections JSON array on stdout
  -h, --help Show this help

Search order:
  1. REMUS_COMPENDIUM_DB (if set and valid)
  2. data/compendium/remus_compendium.db (canonical)
  3. Other *.db files under data/compendium/ (skips backups/)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ensure) ENSURE=1 ;;
        --json) JSON=1 ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            exit 1
            ;;
    esac
    shift
done

abs_path() {
    local path="$1"
    if command -v realpath >/dev/null 2>&1; then
        realpath -m "$path"
        return
    fi
    if [[ -e "$path" ]]; then
        echo "$(cd "$(dirname "$path")" && pwd)/$(basename "$path")"
        return
    fi
    local parent
    parent="$(dirname "$path")"
    if [[ -d "$parent" ]]; then
        echo "$(cd "$parent" && pwd)/$(basename "$path")"
    else
        echo "$path"
    fi
}

is_valid_compendium_db() {
    local db="$1"
    [[ -f "$db" ]] || return 1
    command -v sqlite3 >/dev/null 2>&1 || return 1
    sqlite3 -batch "$db" "SELECT 1 FROM sqlite_master WHERE type='table' AND name='systems' LIMIT 1;" 2>/dev/null \
        | grep -qx '1'
}

candidate_paths() {
    if [[ -n "${REMUS_COMPENDIUM_DB:-}" ]]; then
        abs_path "$REMUS_COMPENDIUM_DB"
    fi

    abs_path "$DEFAULT_DB"

    local db
    shopt -s nullglob
    for db in "$ROOT_DIR"/data/compendium/*.db; do
        [[ "$db" == "$DEFAULT_DB" ]] && continue
        abs_path "$db"
    done
    shopt -u nullglob
}

resolve_compendium_db() {
    local candidate
    while IFS= read -r candidate; do
        [[ -n "$candidate" ]] || continue
        if is_valid_compendium_db "$candidate"; then
            echo "$candidate"
            return 0
        fi
    done < <(candidate_paths | awk '!seen[$0]++')

    if [[ "$ENSURE" -eq 1 ]]; then
        if ! command -v sqlite3 >/dev/null 2>&1; then
            echo "error: sqlite3 is required to bootstrap compendium database" >&2
            exit 1
        fi
        bash "$ROOT_DIR/scripts/setup_compendium_db.sh" "$DEFAULT_DB" >/dev/null
        if is_valid_compendium_db "$DEFAULT_DB"; then
            abs_path "$DEFAULT_DB"
            return 0
        fi
        echo "error: bootstrap completed but database is not valid: $DEFAULT_DB" >&2
        exit 1
    fi

    return 1
}

db_path="$(resolve_compendium_db)" || {
    echo "error: no valid compendium database found under ${ROOT_DIR}/data/compendium" >&2
    echo "hint: bash scripts/setup_compendium_db.sh  # or re-run with --ensure" >&2
    exit 1
}

if [[ "$JSON" -eq 1 ]]; then
    if command -v jq >/dev/null 2>&1; then
        jq -n --arg db "$db_path" '[{
            "name": "Compendium",
            "driver": "SQLite",
            "database": $db
        }]'
    else
        cat <<EOF
[
  {
    "name": "Compendium",
    "driver": "SQLite",
    "database": "${db_path}"
  }
]
EOF
    fi
else
    printf '%s\n' "$db_path"
fi
