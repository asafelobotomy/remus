#!/usr/bin/env bash
# Lint QML under src/gui/qml (required in CI).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v qmllint >/dev/null 2>&1; then
    for candidate in /usr/lib/qt6/bin/qmllint /usr/bin/qmllint; do
        if [[ -x "$candidate" ]]; then
            export PATH="$(dirname "$candidate"):${PATH}"
            break
        fi
    done
fi

if ! command -v qmllint >/dev/null 2>&1; then
    echo "qmllint not found; install qt6-declarative-dev-tools (or qt6-declarative on Arch)." >&2
    exit 1
fi

mapfile -t qml_files < <(find src/gui/qml -name '*.qml' -type f | sort)
if [[ "${#qml_files[@]}" -eq 0 ]]; then
    echo "No QML files found under src/gui/qml." >&2
    exit 1
fi

qmllint "${qml_files[@]}"
