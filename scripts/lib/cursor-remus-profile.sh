#!/usr/bin/env bash
# Shared helpers for Cursor/VS Code Remus profile + workspace association.
# Sourced by configure-workspace-extensions.sh and bootstrap-dev-environment.sh.
#
# shellcheck shell=bash

cursor_profile__workspace_uri() {
    local root="$1"
    local abs
    if command -v realpath >/dev/null 2>&1; then
        abs="$(realpath "$root")"
    else
        abs="$(cd "$root" && pwd)"
    fi
    printf 'file://%s' "$abs"
}

cursor_profile__resolve_location() {
    local user_dir="$1"
    local profile_name="$2"
    local storage="${user_dir}/globalStorage/storage.json"

    [[ -f "$storage" ]] || return 1
    command -v jq >/dev/null 2>&1 || return 1

    jq -r --arg name "$profile_name" '
        .userDataProfiles[]? | select(.name == $name) | .location
    ' "$storage" | head -1
}

cursor_profile__generate_location_id() {
    local profile_name="$1"
    printf -- '-%s' "$(printf '%s' "$profile_name" | sha256sum | cut -c1-7)"
}

cursor_profile__register_profile() {
    local user_dir="$1"
    local profile_name="$2"
    local profile_id="$3"
    local storage="${user_dir}/globalStorage/storage.json"
    local profile_dir="${user_dir}/profiles/${profile_id}"

    mkdir -p "$profile_dir"
    [[ -f "${profile_dir}/settings.json" ]] || echo '{}' >"${profile_dir}/settings.json"
    [[ -f "${profile_dir}/extensions.json" ]] || echo '[]' >"${profile_dir}/extensions.json"

    if [[ ! -f "$storage" ]]; then
        mkdir -p "$(dirname "$storage")"
        echo '{"userDataProfiles":[],"profileAssociations":{"workspaces":{},"emptyWindows":{}}}' >"$storage"
    fi

    local tmp existing
    existing="$(jq -r --arg name "$profile_name" '
        [.userDataProfiles[]? | select(.name == $name)] | length
    ' "$storage")"
    if [[ "$existing" != "0" ]]; then
        return 0
    fi

    tmp="$(mktemp)"
    jq --arg id "$profile_id" --arg name "$profile_name" \
        '.userDataProfiles += [{"location": $id, "name": $name, "icon": "game"}]' \
        "$storage" >"$tmp"
    mv "$tmp" "$storage"
}

cursor_profile__ensure_profile() {
    local editor_cmd="$1"
    local user_dir="$2"
    local profile_name="${3:-Remus}"

    local loc
    loc="$(cursor_profile__resolve_location "$user_dir" "$profile_name" || true)"
    if [[ -n "$loc" ]]; then
        printf '%s\n' "$loc"
        return 0
    fi

    # Installing/listing extensions materializes the profile in many Cursor builds.
    "$editor_cmd" --profile "$profile_name" --list-extensions >/dev/null 2>&1 || true

    loc="$(cursor_profile__resolve_location "$user_dir" "$profile_name" || true)"
    if [[ -n "$loc" ]]; then
        printf '%s\n' "$loc"
        return 0
    fi

    loc="$(cursor_profile__generate_location_id "$profile_name")"
    cursor_profile__register_profile "$user_dir" "$profile_name" "$loc"
    printf '%s\n' "$loc"
}

cursor_profile__associate_workspace() {
    local user_dir="$1"
    local root_dir="$2"
    local profile_location="$3"
    local storage="${user_dir}/globalStorage/storage.json"
    local workspace_uri
    workspace_uri="$(cursor_profile__workspace_uri "$root_dir")"

    [[ -f "$storage" ]] || return 1
    command -v jq >/dev/null 2>&1 || return 1

    local current
    current="$(jq -r --arg ws "$workspace_uri" '.profileAssociations.workspaces[$ws] // empty' "$storage")"
    if [[ "$current" == "$profile_location" ]]; then
        echo "already:${workspace_uri}"
        return 0
    fi

    local tmp
    tmp="$(mktemp)"
    jq --arg ws "$workspace_uri" --arg loc "$profile_location" \
        '.profileAssociations.workspaces[$ws] = $loc' "$storage" >"$tmp"
    mv "$tmp" "$storage"
    echo "associated:${workspace_uri}"
}

cursor_profile__ensure_workspace_binding() {
    local editor_cmd="$1"
    local user_dir="$2"
    local root_dir="$3"
    local profile_name="${4:-Remus}"

    local profile_location result
    profile_location="$(cursor_profile__ensure_profile "$editor_cmd" "$user_dir" "$profile_name")"
    result="$(cursor_profile__associate_workspace "$user_dir" "$root_dir" "$profile_location")"
    printf '%s|%s\n' "$profile_location" "$result"
}
