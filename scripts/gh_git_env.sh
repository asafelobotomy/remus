#!/usr/bin/env bash
# Export GitHub CLI auth for git commits/push and GitHub API scripts (never prints secrets).
#
# Usage (source before git commit/push when git user.* is unset):
#   source scripts/gh_git_env.sh
#   git commit ...
#   git -c credential.helper= -c 'credential.helper=!gh auth git-credential' push
#
# Also sets GITHUB_TOKEN when unset (used by update_dats.sh / update_mame_listxml.sh).

_gh_git_env_finish() {
    return 0 2>/dev/null || exit 0
}

if ! command -v gh >/dev/null 2>&1; then
    _gh_git_env_finish
fi

if ! gh auth status >/dev/null 2>&1; then
    _gh_git_env_finish
fi

if [[ -z "${GITHUB_TOKEN:-}" ]]; then
    _gh_tok="$(gh auth token 2>/dev/null || true)"
    [[ -n "${_gh_tok}" ]] && export GITHUB_TOKEN="${_gh_tok}"
fi

if [[ -z "${GIT_AUTHOR_NAME:-}" || -z "${GIT_AUTHOR_EMAIL:-}" ]]; then
    _gh_login="$(gh api user -q .login 2>/dev/null || true)"
    _gh_id="$(gh api user -q .id 2>/dev/null || true)"
    if [[ -n "${_gh_login}" && -n "${_gh_id}" ]]; then
        export GIT_AUTHOR_NAME="${GIT_AUTHOR_NAME:-${_gh_login}}"
        export GIT_AUTHOR_EMAIL="${GIT_AUTHOR_EMAIL:-${_gh_id}+${_gh_login}@users.noreply.github.com}"
        export GIT_COMMITTER_NAME="${GIT_COMMITTER_NAME:-${GIT_AUTHOR_NAME}}"
        export GIT_COMMITTER_EMAIL="${GIT_COMMITTER_EMAIL:-${GIT_AUTHOR_EMAIL}}"
    fi
fi

_gh_git_env_finish
