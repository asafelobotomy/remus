#!/usr/bin/env bash
# Lint QML under src/gui/qml (required in CI).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR" || exit 1

if ! command -v qmllint >/dev/null 2>&1; then
    for candidate in /usr/lib/qt6/bin/qmllint /usr/bin/qmllint; do
        if [[ -x "$candidate" ]]; then
            qmllint_dir="$(dirname "$candidate")"
            export PATH="${qmllint_dir}:${PATH}"
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

# qmllint runs without a cmake build, so Remus.Gui context properties and generated
# qmltypes are unavailable. Downgrade categories that depend on those to "info" so
# the job surfaces issues but only fails on hard syntax errors (non-zero exit other
# than warning-only 255). See docs/CONTRIBUTING.md (qml-lint is informational).
#
# Qt 6.7+ renamed several flags; detect version and use the appropriate set.
QT_MINOR="$(qmllint --version 2>&1 | grep -oP '6\.\K[0-9]+' | head -1)"
if [[ "${QT_MINOR:-0}" -ge 7 ]]; then
    # Qt 6.7+ flag names
    qmllint \
        --context-properties info \
        --import info \
        --missing-type info \
        --incompatible-type info \
        --missing-property info \
        --signal-handler-parameters info \
        --required info \
        --alias-cycle info \
        --deprecated info \
        "${qml_files[@]}"
else
    # Qt ≤ 6.6 flag names (Ubuntu 24.04 CI)
    qmllint \
        --unqualified info \
        --import info \
        --type info \
        --property info \
        --signal info \
        --required info \
        --alias info \
        --deprecated info \
        --with info \
        "${qml_files[@]}"
fi
