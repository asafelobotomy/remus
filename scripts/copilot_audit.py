#!/usr/bin/env python3
"""Lightweight local audit helper for Copilot instruction surfaces.

This script performs quick structural checks and exits non-zero on failures.
"""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent

REQUIRED_FILES = [
    ROOT / ".github" / "copilot-instructions.md",
    ROOT / ".github" / "copilot-version.md",
    ROOT / "AGENTS.md",
    ROOT / ".copilot" / "workspace" / "workspace-index.json",
]


def main() -> int:
    missing = [str(path.relative_to(ROOT)) for path in REQUIRED_FILES if not path.exists()]
    if missing:
        for item in missing:
            print(f"MISSING: {item}")
        return 1

    # Guard against unresolved template placeholders in active instructions.
    instructions = (ROOT / ".github" / "copilot-instructions.md").read_text(encoding="utf-8")
    if "{{" in instructions or "}}" in instructions:
        print("PLACEHOLDER_TOKENS_FOUND: .github/copilot-instructions.md")
        return 1

    print("copilot_audit.py: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
