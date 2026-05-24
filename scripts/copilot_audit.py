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

    # D4: Validate agent frontmatter has required fields.
    agent_issues = _check_agent_frontmatter()
    if agent_issues:
        for issue in agent_issues:
            print(issue)
        return 1

    print("copilot_audit.py: OK")
    return 0


_AGENT_REQUIRED_FIELDS = {"name", "model", "tools", "agents"}


def _check_agent_frontmatter() -> list[str]:
    """D4: Every .github/agents/*.agent.md must declare name, model, tools, agents."""
    agents_dir = ROOT / ".github" / "agents"
    if not agents_dir.exists():
        return [f"D4_MISSING_DIR: .github/agents/"]

    issues: list[str] = []
    for agent_file in sorted(agents_dir.glob("*.agent.md")):
        lines = agent_file.read_text(encoding="utf-8").splitlines()
        if not lines or lines[0].strip() != "---":
            issues.append(f"D4_NO_FRONTMATTER: {agent_file.relative_to(ROOT)}")
            continue
        fm_keys: set[str] = set()
        for line in lines[1:]:
            if line.strip() == "---":
                break
            if ":" in line:
                fm_keys.add(line.split(":", 1)[0].strip())
        missing = _AGENT_REQUIRED_FIELDS - fm_keys
        if missing:
            issues.append(
                f"D4_MISSING_FIELDS({','.join(sorted(missing))}): {agent_file.relative_to(ROOT)}"
            )
    return issues


if __name__ == "__main__":
    sys.exit(main())
