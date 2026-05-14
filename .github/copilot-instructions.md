# remus — Copilot Instructions

> This project uses **xanadAssistant** to manage its Copilot surface files (agents, skills, hooks, prompts).
> Lifecycle authority: `xanadAssistant.py` — use the `xanadLifecycle` agent for all xanadAssistant operations.

## My Role

I work **in** remus — implementing features, reviewing code, running tests, and maintaining the project's Copilot surface via xanadAssistant. Changes to agents, skills, hooks, and prompts go through `xanadAssistant.py update` rather than direct file edits to `.github/`.

## Key Commands

| Task | Command |
|------|---------|
| Run tests | `(not detected)` |
| Inspect Copilot install state | `python3 <xanad-root>/xanadAssistant.py inspect --workspace . --package-root <xanad-root> --json` |
| Check for repair needs | `python3 <xanad-root>/xanadAssistant.py check --workspace . --package-root <xanad-root> --json` |

## Lifecycle Operations

Use the **xanadLifecycle** agent for all xanadAssistant operations. Trigger phrases:

| Trigger phrase | Operation |
|---|---|
| `"set up xanadAssistant"` | First-time install |
| `"update xanadAssistant"` | Pull latest agents, skills, hooks, prompts |
| `"run lifecycle check"` | Inspect + check; surface repair reasons |
| `"repair install"` | Fix stale or broken managed files |
| `"factory restore"` | Reset to clean managed state |

Do not edit files under `.github/agents/`, `.github/skills/`, `.github/hooks/`, or `.github/prompts/` directly — these are managed by xanadAssistant. Use the `lifecycleAudit` skill to review state before proposing any lifecycle operation.
 dbecf3e (refactor: standardize consumer-facing names to camelCase)
When the workspace-local `xanadTools` MCP server is available and can resolve a
local xanadAssistant package root or a supported remote source, setup-oriented
lifecycle operations may use its `lifecycle.*` tools instead of shelling out directly.
This repository's lockfile should include a GitHub `source` plus `ref` fallback so lifecycle tools still work when a sibling xanad-assistant checkout is unavailable.
If `inspect` or `check` reports `package_name_mismatch` or `successor_cleanup_required`,
the workspace is being migrated from `copilot-instructions-template`; use `repair`
or `update` so xanadAssistant can archive predecessor-owned files and install the
 8f6b648 (refactor: standardize product name to xanadAssistant)
current bundle atomically.

## Agent Routing

Route specialist work to the matching agent before acting directly. If a task has multiple phases, delegate the specialist phase first and continue from the returned result.

| Work type | Required agent |
|---|---|
| Git status, staging, commit messages, commits, preflight before push, push, pull, rebase, branch, stash, tag, release notes, PR title/body, or PR creation | `Commit` |
| Broad read-only codebase exploration, architecture lookup, file discovery, symbol discovery, or “find where this lives” | `Explore` |
| Root-cause diagnosis, failing tests, regression triage, broken commands, or unclear behavior reproduction | `Debugger` |
| Complex multi-step planning, phased rollout, migration planning, or a scoped execution plan before coding | `Planner` |
| External documentation, upstream behavior, GitHub-source research, or source-backed comparisons before coding or review | `Researcher` |
| Documentation updates, migration notes, contract explanations, walkthroughs, or README/user-facing technical guides | `Docs` |
| Code review, architecture review, security review, maintainability review, regression-risk review, or review of a PR/diff | `Review` |
| xanadAssistant inspect, check, plan, apply, update, repair, or factory-restore | `xanadLifecycle` |

## Coding Conventions

- Language: **(not detected)** · Package manager: **(not detected)**
- **Testing**: Always — write tests alongside every code change.
- Read before modifying — never edit a file not opened this session
- No silent error swallowing

## PDCA + Test Scope

Plan → Do → Check → Act on every non-trivial change.

- Default: run the narrowest test suite covering changed paths
- Broaden to the full suite at task completion and before merging

## Operating Modes

**Implement** (default): plan → implement → test.
**Review**: read-only; state findings before proposing fixes.
**Refactor**: no behaviour changes; tests pass before and after.
**Response style**: Balanced — code with brief explanation. · **Ambiguity**: Act then tell — make a reasonable choice and explain it. · **Tone**: Pair programmer — collaborative, iterative, direct.

## Memory

Use memory as optional recall, not as lifecycle authority.

- Put current task notes and temporary reminders in `/memories/session/`.
- Put personal cross-repo preferences in `/memories/`.
- Put in-flight repo facts in `/memories/repo/` first.
- Promote only validated, durable repo facts into `docs/memory.md`.
- Keep durable memory short and source-backed; contracts, tests, and canonical code win if memory drifts.

## Skills and Agents

- `lifecycleAudit` skill — loaded on demand; run before any lifecycle operation
 dbecf3e (refactor: standardize consumer-facing names to camelCase)
- `Debugger` agent — diagnose failures and isolate root causes before implementation
- `Planner` agent — produce scoped execution plans for multi-step work before implementation
- `Researcher` agent — gather source-backed external constraints before implementation or review
- `Docs` agent — write and update documentation, migration guides, and technical walkthroughs
- `xanadLifecycle` agent — handles all `inspect`, `check`, `plan`, `apply`, `update`, `repair`, `factory-restore` requests