#!/usr/bin/env bash
# purpose: shared helpers to detect populated compendium DBs and block accidental destructive ops.
# when: sourced by setup/clean/build scripts before removing or recreating remus_compendium.db.
# inputs: COMPENDIUM_ALLOW_DESTRUCTIVE=1 to override populated-DB guards
# outputs: helper functions only (no side effects until called)
# risk: safe

compendium_db_game_count() {
    local db="$1"
    if [[ ! -f "$db" ]]; then
        echo 0
        return 0
    fi
    if ! command -v sqlite3 >/dev/null 2>&1; then
        echo 0
        return 0
    fi
    sqlite3 -batch -cmd "PRAGMA busy_timeout=5000;" "$db" "SELECT COUNT(*) FROM games;" 2>/dev/null | tail -1 || echo 0
}

compendium_db_is_populated() {
    local count
    count="$(compendium_db_game_count "$1")"
    [[ "${count:-0}" -gt 0 ]]
}

compendium_abort_if_populated_without_force() {
    local db="$1"
    local operation="$2"
    local force_flag="${3:-0}"

    if [[ ! -f "$db" ]]; then
        return 0
    fi
    if ! compendium_db_is_populated "$db"; then
        return 0
    fi
    if [[ "${COMPENDIUM_ALLOW_DESTRUCTIVE:-0}" == "1" || "$force_flag" == "1" ]]; then
        return 0
    fi

    local count
    count="$(compendium_db_game_count "$db")"
    echo "error: refusing to $operation populated compendium database" >&2
    echo "       $db ($count games)" >&2
    echo "hint: pass --force, set COMPENDIUM_ALLOW_DESTRUCTIVE=1, or use incremental refresh instead" >&2
    exit 1
}

compendium_backup_if_populated() {
    local db="$1"
    local label="${2:-pre-refresh}"

    if [[ ! -f "$db" ]]; then
        return 0
    fi
    if ! compendium_db_is_populated "$db"; then
        return 0
    fi

    local backup_dir
    backup_dir="$(dirname "$db")/backups"
    mkdir -p "$backup_dir"

    local stamp base dest
    stamp="$(date -u +%Y%m%dT%H%M%SZ)"
    base="$(basename "$db" .db)"
    dest="$backup_dir/${base}.${label}.${stamp}.db"

    echo "==> Backing up populated compendium ($dest)"
    cp -a -- "$db" "$dest"
    if [[ -f "${db}-wal" ]]; then
        cp -a -- "${db}-wal" "${dest}-wal"
    fi
    if [[ -f "${db}-shm" ]]; then
        cp -a -- "${db}-shm" "${dest}-shm"
    fi

    # Keep the three most recent backups per label prefix.
    local -a old_backups=()
    local entry
    while IFS= read -r entry; do
        old_backups+=("$entry")
    done < <(ls -1t "$backup_dir/${base}.${label}."*.db 2>/dev/null || true)
    local i
    for ((i = 3; i < ${#old_backups[@]}; i++)); do
        rm -f -- "${old_backups[$i]}" "${old_backups[$i]}-wal" "${old_backups[$i]}-shm"
    done
}
