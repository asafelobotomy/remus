#!/usr/bin/env bash
# Lint QML under src/gui/qml (informational; does not gate merge yet).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v qmllint >/dev/null 2>&1; then
    echo "qmllint not found; install qt6-declarative-dev-tools." >&2
    exit 1
fi

mapfile -t qml_files < <(find src/gui/qml -name '*.qml' -type f | sort)
if [[ "${#qml_files[@]}" -eq 0 ]]; then
    echo "No QML files found under src/gui/qml." >&2
    exit 1
fi

qmllint "${qml_files[@]}"
