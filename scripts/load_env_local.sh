#!/usr/bin/env bash
# Source REMUS_* credentials from .env.local (gitignored).
# Usage: source scripts/load_env_local.sh [path-to-env-file]
set -euo pipefail

_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${1:-${_ROOT}/.env.local}"

# Fail fast if credential files were accidentally added to git (plaintext secrets).
if command -v git >/dev/null 2>&1 && git -C "${_ROOT}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    _tracked_credential_files=()
    for _cred_path in .env.local data/compendium/enrichment-credentials.json; do
        if git -C "${_ROOT}" ls-files --error-unmatch "${_cred_path}" >/dev/null 2>&1; then
            _tracked_credential_files+=("${_cred_path}")
        fi
    done
    if [[ ${#_tracked_credential_files[@]} -gt 0 ]]; then
        echo "error: credential file(s) are tracked by git (remove from index, keep local only):" >&2
        printf '  %s\n' "${_tracked_credential_files[@]}" >&2
        echo "hint: git rm --cached <file>  # then confirm .gitignore covers it" >&2
        exit 1
    fi
fi
unset _cred_path _tracked_credential_files

if [[ -f "${ENV_FILE}" ]]; then
    case "${ENV_FILE}" in
        *.json)
            echo "error: refusing to source JSON as shell env: ${ENV_FILE}" >&2
            ;;
        *)
            set -a
            # shellcheck disable=SC1090
            source "${ENV_FILE}"
            set +a
            ;;
    esac
fi

# GitHub API token from `gh` when logged in (update_dats, MAME listxml).
# shellcheck source=gh_git_env.sh
source "$(dirname "${BASH_SOURCE[0]}")/gh_git_env.sh"
