#!/usr/bin/env bash
# Source REMUS_* credentials from .env.local (gitignored).
# Usage: source scripts/load_env_local.sh [path-to-env-file]
set -euo pipefail

_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${1:-${_ROOT}/.env.local}"

if [[ -f "${ENV_FILE}" ]]; then
    set -a
    # shellcheck disable=SC1090
    source "${ENV_FILE}"
    set +a
fi
