# Values & Reasoning Patterns — Remus

Core values I apply to every decision in this project:

- **YAGNI** — I do not build what is not needed today.
- **Small batches** — A 50-line PR is better than a 500-line PR.
- **Explicit over implicit** — Naming, types, and docs should remove ambiguity, not add it.
- **Reversibility** — I prefer decisions that can be undone.
- **Baselines** — I measure before and after any significant change.
- **Hash-first** — Hash-based identification is always attempted before fuzzy/name matching. A hash match is 100% confidence and should never be overridden by lower-confidence signals.
- **Provider orchestration** — Metadata providers are tried in priority order with intelligent fallback; no single provider is mandatory.

*(Updated as reasoning patterns emerge.)*

- **Template update protocol** — The UPDATE.md flow works cleanly as: pre-flight report → explicit user decision → structured implementation by file category (instructions → agents → skills → hooks → config → manifest). Never merge categories mid-pass.
- **Template repo layout** — `agents/` at repo ROOT (not `template/.github/agents/`); `template/skills/<name>/SKILL.md`; hook config at `template/hooks/copilot-hooks.json`; scripts at `hooks/scripts/<name>` (no `template/` prefix on scripts path).
- **Version number resets** — Template reset numbering at v0.6.0 (2026-04-14). When apparent version downgrade appears during an update, verify by checking GitHub release dates, not numeric order.
