#!/usr/bin/env bash
# Resolve machine-specific paths for editor extensions (clangd, clang-format, qmlls, etc.).
# Emits a JSON object suitable for merging into Cursor/VS Code profile settings.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

require_cmd() {
    local name="$1"
    command -v "$name" >/dev/null 2>&1 || {
        echo "error: required tool not found on PATH: ${name}" >&2
        exit 1
    }
}

resolve_clang_format() {
    local pinned candidate major
    pinned="$(tr -d '[:space:]' < "$ROOT_DIR/.clang-format-version")"
    for candidate in \
        "/usr/bin/clang-format-${pinned}" \
        "clang-format-${pinned}" \
        "clang-format"; do
        if command -v "$candidate" >/dev/null 2>&1; then
            candidate="$(command -v "$candidate")"
            major="$("$candidate" --version 2>/dev/null | sed -n 's/.*version \([0-9][0-9]*\).*/\1/p' | head -1)"
            if [[ "$major" == "$pinned" ]]; then
                echo "$candidate"
                return 0
            fi
        fi
    done
    return 1
}

resolve_qt_bin_dir() {
    local dir
    if command -v qmake6 >/dev/null 2>&1; then
        dir="$(qmake6 -query QT_INSTALL_BINS 2>/dev/null || true)"
        if [[ -n "$dir" && -d "$dir" ]]; then
            echo "$dir"
            return 0
        fi
    fi
    for dir in /usr/lib/qt6/bin /usr/lib64/qt6/bin; do
        if [[ -x "${dir}/qmlls" || -x "${dir}/qmllint" ]]; then
            echo "$dir"
            return 0
        fi
    done
    return 1
}

resolve_qmlls() {
    local qt_bin="$1"
    if [[ -x "${qt_bin}/qmlls" ]]; then
        echo "${qt_bin}/qmlls"
        return 0
    fi
    if command -v qmlls >/dev/null 2>&1; then
        command -v qmlls
        return 0
    fi
    if command -v qmlls6 >/dev/null 2>&1; then
        command -v qmlls6
        return 0
    fi
    return 1
}

require_cmd clangd
require_cmd shellcheck

CLANG_FORMAT="$(resolve_clang_format)" || {
    echo "error: clang-format $(tr -d '[:space:]' < "$ROOT_DIR/.clang-format-version") not found" >&2
    exit 1
}
CLANGD="$(command -v clangd)"
SHELLCHECK="$(command -v shellcheck)"
LLDB="$(command -v lldb || true)"
QT_BIN_DIR="$(resolve_qt_bin_dir)" || {
    echo "error: Qt 6 bin directory not found (install qt6-base / qt6-declarative)" >&2
    exit 1
}
QMLLS="$(resolve_qmlls "$QT_BIN_DIR")" || {
    echo "error: qmlls not found under ${QT_BIN_DIR}" >&2
    exit 1
}

if command -v jq >/dev/null 2>&1; then
    jq -n \
        --arg clang_format "$CLANG_FORMAT" \
        --arg clangd "$CLANGD" \
        --arg shellcheck "$SHELLCHECK" \
        --arg lldb "$LLDB" \
        --arg qt_bin "$QT_BIN_DIR" \
        --arg qmlls "$QMLLS" \
        '{
            "clang-format.executable": $clang_format,
            "clang-format.executable.linux": $clang_format,
            "clangd.path": $clangd,
            "shellcheck.executablePath": $shellcheck,
            "qt-qml.qmlls.customExePath": $qmlls,
            "terminal.integrated.env.linux": {
                "PATH": "\($qt_bin):${env:PATH}"
            },
            "cmake.debugConfig": {
                "type": "lldb",
                "request": "launch",
                "cwd": "${workspaceFolder}",
                "env": {
                    "PATH": "\($qt_bin):${env:PATH}"
                }
            }
        }
        + (if ($lldb | length) > 0 then {"lldb.executable": $lldb} else {} end)'
else
    cat <<EOF
{
  "clang-format.executable": "${CLANG_FORMAT}",
  "clang-format.executable.linux": "${CLANG_FORMAT}",
  "clangd.path": "${CLANGD}",
  "shellcheck.executablePath": "${SHELLCHECK}",
  "qt-qml.qmlls.customExePath": "${QMLLS}",
  "terminal.integrated.env.linux": {
    "PATH": "${QT_BIN_DIR}:\${env:PATH}"
  },
  "cmake.debugConfig": {
    "type": "lldb",
    "request": "launch",
    "cwd": "\${workspaceFolder}",
    "env": {
      "PATH": "${QT_BIN_DIR}:\${env:PATH}"
    }
  }$( [[ -n "$LLDB" ]] && printf ',\n  "lldb.executable": "%s"' "$LLDB" )
}
EOF
fi
