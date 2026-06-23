#!/usr/bin/env bash
# Shared helpers for idempotent compendium offline asset sync.
# Source from update_dats.sh, update_hasheous_dumps.sh, etc. — do not execute directly.

compendium_sha256_of() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk '{print $1}'
    else
        echo "error: no sha256 utility (sha256sum or shasum) found" >&2
        return 1
    fi
}

# Copy SRC → DEST when content differs. Prints "updated" or "unchanged" on stdout.
install_file_if_changed() {
    local src="$1"
    local dest="$2"

    if [[ ! -f "$src" ]]; then
        return 1
    fi

    if [[ -f "$dest" ]]; then
        local src_hash dest_hash
        src_hash="$(compendium_sha256_of "$src")"
        dest_hash="$(compendium_sha256_of "$dest")"
        if [[ "$src_hash" == "$dest_hash" ]]; then
            printf 'unchanged\n'
            return 0
        fi
    fi

    mkdir -p "$(dirname "$dest")"
    cp -- "$src" "$dest"
    printf 'updated\n'
    return 0
}
