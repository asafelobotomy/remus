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

compendium_latest_post_ingest_backup() {
    local db="$1"
    local backup_dir base
    backup_dir="$(dirname "$db")/backups"
    base="$(basename "$db" .db)"
    ls -1t "$backup_dir/${base}.post-ingest."*.db 2>/dev/null | head -1 || true
}

compendium_restore_post_ingest_backup() {
    local db="$1"
    local backup="${2:-$(compendium_latest_post_ingest_backup "$db")}"

    if [[ -z "$backup" || ! -f "$backup" ]]; then
        echo "error: no post-ingest backup found for $db" >&2
        echo "hint: backups live in $(dirname "$db")/backups/*.post-ingest.*.db" >&2
        return 1
    fi

    echo "==> Restoring post-ingest backup"
    echo "    from=$backup"
    echo "    to=$db"
    compendium_adopt_database_artifact "$backup" "$db"
}

compendium_db_effective_game_count() {
    local db="$1"
    if [[ ! -f "$db" ]]; then
        echo 0
        return 0
    fi
    sqlite3 -batch -cmd "PRAGMA busy_timeout=5000;" "$db" \
        "PRAGMA wal_checkpoint(PASSIVE); SELECT COUNT(*) FROM games;" 2>/dev/null | tail -1 || echo 0
}

compendium_adopt_database_artifact() {
    local src="$1"
    local dest="$2"

    if [[ ! -f "$src" ]]; then
        echo "error: adopt source not found: $src" >&2
        return 1
    fi

    mkdir -p "$(dirname "$dest")"
    cp -a -- "$src" "$dest"
    if [[ -f "${src}-wal" ]]; then
        cp -a -- "${src}-wal" "${dest}-wal"
    else
        rm -f -- "${dest}-wal"
    fi
    if [[ -f "${src}-shm" ]]; then
        cp -a -- "${src}-shm" "${dest}-shm"
    else
        rm -f -- "${dest}-shm"
    fi
}

compendium_find_best_staged_db() {
    local output_db="$1"
    local dir base prefix path count best_path="" best_count=0

    dir="$(dirname "$output_db")"
    base="$(basename "$output_db")"
    prefix="${base}.staged-"

    shopt -s nullglob
    for path in "$dir/${prefix}"*; do
        case "$path" in
            *-wal | *-shm)
                continue
                ;;
        esac
        [[ -f "$path" ]] || continue
        count="$(compendium_db_effective_game_count "$path")"
        if [[ "${count:-0}" -gt "$best_count" ]]; then
            best_count=$count
            best_path=$path
        fi
    done
    shopt -u nullglob

    if [[ "$best_count" -gt 0 ]]; then
        echo "$best_path"
    fi
}

compendium_find_best_pre_rebuild_backup() {
    local output_db="$1"
    local path count best_path="" best_count=0

    shopt -s nullglob
    for path in "${output_db}.pre-rebuild-"*.bak; do
        [[ -f "$path" ]] || continue
        count="$(compendium_db_effective_game_count "$path")"
        if [[ "${count:-0}" -gt "$best_count" ]]; then
            best_count=$count
            best_path=$path
        fi
    done
    shopt -u nullglob

    if [[ "$best_count" -gt 0 ]]; then
        echo "$best_path"
    fi
}

compendium_progress_status() {
    local progress_file="$1"
    if [[ ! -f "$progress_file" ]]; then
        return 0
    fi
    if command -v jq >/dev/null 2>&1; then
        jq -r '.status // ""' "$progress_file" 2>/dev/null || true
    fi
}

compendium_full_build_lock_holder() {
    local lock_path="$1"
    if [[ ! -f "$lock_path" ]]; then
        return 0
    fi
    head -1 "$lock_path" 2>/dev/null || true
}

compendium_full_build_is_running() {
    local lock_path="$1"
    local pid

    pid="$(compendium_full_build_lock_holder "$lock_path")"
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        return 0
    fi

    if pgrep -f "build_compendium_full\\.sh" >/dev/null 2>&1; then
        return 0
    fi

    return 1
}

compendium_try_resume_in_progress_build() {
    local output_db="$1"
    local progress_file="${output_db}.progress.json"
    local output_count staged_path backup_path pre_rebuild_path
    local staged_count backup_count pre_rebuild_count
    local best_path="" best_kind="" best_count=0 count path status

    output_count="$(compendium_db_effective_game_count "$output_db")"
    if [[ "${output_count:-0}" -gt 0 ]]; then
        echo "==> Found populated compendium ($output_count games); will plan incremental refresh"
        return 0
    fi

    staged_path="$(compendium_find_best_staged_db "$output_db")"
    if [[ -n "$staged_path" ]]; then
        staged_count="$(compendium_db_effective_game_count "$staged_path")"
        if [[ "${staged_count:-0}" -gt "$best_count" ]]; then
            best_path=$staged_path
            best_kind="staged"
            best_count=$staged_count
        fi
    fi

    backup_path="$(compendium_latest_post_ingest_backup "$output_db")"
    if [[ -n "$backup_path" && -f "$backup_path" ]]; then
        backup_count="$(compendium_db_effective_game_count "$backup_path")"
        if [[ "${backup_count:-0}" -gt "$best_count" ]]; then
            best_path=$backup_path
            best_kind="post-ingest-backup"
            best_count=$backup_count
        fi
    fi

    pre_rebuild_path="$(compendium_find_best_pre_rebuild_backup "$output_db")"
    if [[ -n "$pre_rebuild_path" ]]; then
        pre_rebuild_count="$(compendium_db_effective_game_count "$pre_rebuild_path")"
        if [[ "${pre_rebuild_count:-0}" -gt "$best_count" ]]; then
            best_path=$pre_rebuild_path
            best_kind="pre-rebuild-backup"
            best_count=$pre_rebuild_count
        fi
    fi

    if [[ -z "$best_path" || "$best_count" -le 0 ]]; then
        echo "==> No recoverable in-progress compendium build found; starting fresh ingest"
        return 0
    fi

    status="$(compendium_progress_status "$progress_file")"
    echo "==> Recovering in-progress compendium build instead of fresh ingest"
    echo "    source=$best_path"
    echo "    kind=$best_kind"
    echo "    games=$best_count"
    if [[ -n "$status" ]]; then
        echo "    progress_status=$status"
    fi

    compendium_adopt_database_artifact "$best_path" "$output_db"
    output_count="$(compendium_db_effective_game_count "$output_db")"
    echo "==> Adopted recoverable build into $output_db ($output_count games)"
}
