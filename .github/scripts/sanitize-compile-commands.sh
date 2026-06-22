#!/usr/bin/env bash
# Remove compiler flags from compile_commands.json that clang/clangd reject.
# GCC hardening flags (e.g. CachyOS -mno-direct-extern-access) break clangd indexing.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build-debug}"
CC_FILE="$ROOT_DIR/$BUILD_DIR/compile_commands.json"

if [[ ! -f "$CC_FILE" ]]; then
    exit 0
fi

python3 - "$CC_FILE" <<'PY'
import json
import shlex
import sys
from pathlib import Path

cc_path = Path(sys.argv[1])
STRIP = {
    "-mno-direct-extern-access",
}


def strip_flags(parts: list[str]) -> list[str]:
    return [part for part in parts if part not in STRIP]


def strip_command_if_needed(command: str) -> tuple[str, bool]:
    parts = shlex.split(command)
    stripped = strip_flags(parts)
    if stripped == parts:
        return command, False
    return shlex.join(stripped), True


data = json.loads(cc_path.read_text(encoding="utf-8"))
changed = 0

for entry in data:
    if "command" in entry and isinstance(entry["command"], str):
        cleaned, updated = strip_command_if_needed(entry["command"])
        if updated:
            entry["command"] = cleaned
            changed += 1
    if "arguments" in entry and isinstance(entry["arguments"], list):
        stripped = strip_flags(entry["arguments"])
        if stripped != entry["arguments"]:
            entry["arguments"] = stripped
            changed += 1

cc_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
rel = cc_path.name if cc_path.parent.name == "." else f"{cc_path.parent.name}/{cc_path.name}"
print(f"sanitize-compile-commands: {rel} ({changed} entr{'y' if changed == 1 else 'ies'} updated)")
PY
