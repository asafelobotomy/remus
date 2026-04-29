---
name: Terminal Usage
applyTo: "scripts/**,**/*.sh"
description: "Terminal command safety and shell scripting conventions"
---

- Use `set -euo pipefail` in bash scripts.
- Quote variables and paths.
- Prefer explicit command arguments over `eval`.
- Validate external inputs before command execution.
