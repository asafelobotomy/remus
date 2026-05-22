#!/usr/bin/env python3
"""LOC gate — enforces a line-count budget on the Copilot attention surface.

Checks the line count of files that are auto-loaded into AI context windows
and ensures they stay within a maintainable budget.

Exit codes:
  0 — all budgets satisfied (or advisory warnings only)
  1 — hard limit breached (blocks commit until trimmed)
"""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent

# Each entry: label, path, warn threshold, hard limit.
# warn  — advisory; prints a notice but does not block.
# hard  — blocks commit; must be resolved before merging.
TARGETS = [
    {
        "label": "copilot-instructions.md",
        "path": ROOT / ".github" / "copilot-instructions.md",
        "warn": 200,
        "hard": 400,
    },
]


def main() -> int:
    exit_code = 0
    for target in TARGETS:
        path: Path = target["path"]
        if not path.exists():
            print(f"MISSING: {path.relative_to(ROOT)}")
            exit_code = 1
            continue

        lines = len(path.read_text(encoding="utf-8").splitlines())
        label: str = target["label"]
        warn: int = target["warn"]
        hard: int = target["hard"]

        if lines >= hard:
            print(
                f"LOC_HARD_LIMIT: {label} — {lines} lines"
                f" (limit {hard}; trim to unblock)"
            )
            exit_code = 1
        elif lines >= warn:
            print(
                f"LOC_WARN: {label} — {lines} lines"
                f" (warn at {warn}, limit {hard}; consider trimming)"
            )
        else:
            print(f"check_loc.py: OK  {label} — {lines} / {hard} lines")

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
