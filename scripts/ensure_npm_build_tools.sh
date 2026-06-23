#!/usr/bin/env bash
# purpose: install npm build tools (cwebp-bin) and prepend node_modules/.bin to PATH.
# when: sourced by compendium build/validation scripts before consolidate or artwork ingest.
# inputs: ROOT_DIR (repo root; defaults to parent of scripts/)
# outputs: PATH with node_modules/.bin when cwebp-bin is installed
# risk: safe (network only when npm install runs)

ensure_npm_build_tools() {
    local root="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

    if [[ ! -f "$root/package.json" ]]; then
        return 0
    fi

    if [[ ! -x "$root/node_modules/.bin/cwebp" ]]; then
        echo "==> Installing npm build tools (cwebp-bin)"
        (cd "$root" && npm install --no-fund --no-audit)
    fi

    if [[ -d "$root/node_modules/.bin" ]]; then
        export PATH="$root/node_modules/.bin:${PATH}"
    fi
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    ensure_npm_build_tools
    if command -v cwebp >/dev/null 2>&1; then
        cwebp -version 2>&1 | head -1
    else
        echo "cwebp not available after npm install" >&2
        exit 1
    fi
fi
