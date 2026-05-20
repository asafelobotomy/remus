# remus — Copilot Instructions

> This project uses **xanadAssistant** to manage its Copilot surface files (agents, skills, hooks, prompts).
> Lifecycle authority: `xanadAssistant.py` — use the `xanadLifecycle` agent for all xanadAssistant operations.

## My Role

I work **in** remus — implementing features, reviewing code, running tests, and maintaining the project's Copilot surface via xanadAssistant. Changes to agents, skills, hooks, and prompts go through `xanadAssistant.py update` rather than direct file edits to `.github/`.

## Key Commands

| Task | Command |
| ------ | --------- |
| Run tests | `(not detected)` |
| Drift preflight | `python3 scripts/copilot_audit.py` |
| LOC gate | `python3 scripts/check_loc.py` |
| Inspect Copilot install state | `python3 <xanad-root>/xanadAssistant.py inspect --workspace . --package-root <xanad-root> --json` |
| Check for repair needs | `python3 <xanad-root>/xanadAssistant.py health-check --workspace . --package-root <xanad-root> --json` |

## Lifecycle Operations

Use the **xanadLifecycle** agent for all xanadAssistant operations. Trigger phrases:

| Trigger phrase | Operation |
| --- | --- |
| `"set up xanadAssistant"` | First-time install |
| `"update xanadAssistant"` | Pull latest agents, skills, hooks, prompts |
| `"run lifecycle check"` | Inspect + check; surface repair reasons |
| `"repair install"` | Fix stale or broken managed files |
| `"factory restore"` | Reset to clean managed state |
| `"health check"` | Run xanadAssistant install-state health check |

Available prompts: `/setup` (install or refresh), `/bootstrap` (cold-start from bare workspace), `/update` (pull latest package files).

Do not edit files under `.github/agents/`, `.github/skills/`, `.github/mcp/`, or `.github/prompts/` directly — these are managed by xanadAssistant. Use the `lifecycleAudit` skill to review state before proposing any lifecycle operation.

**Conditional behaviors:**
- **If `xanadTools` MCP is available** and can resolve a local xanadAssistant package root or a supported remote source, setup-oriented lifecycle operations may use its `lifecycle.*` tools instead of shelling out directly. If `xanadTools` MCP is unavailable, fall back to `xanadAssistant.py` directly.
- **If `inspect` or `health-check` reports `package_name_mismatch` or `successor_cleanup_required`**, the workspace is being migrated from `copilot-instructions-template`; use `repair` or `update` so xanadAssistant can archive predecessor-owned files and install the current bundle atomically.

## Agent Routing

Route specialist work to the matching agent before acting directly. If a task involves work that maps to a specialist agent's domain, delegate that work to the specialist agent first and continue from the returned result. If a delegated agent cannot complete its task, handle the failure inline or surface it to the user before continuing.

| Work type | Required agent |
| --- | --- |
| Pruning stale artefacts, caches, archives, dead files, or tightening repository hygiene | `Cleaner` |
| Git status, staging, commit messages, commits, preflight before push, push, pull, rebase, branch, stash, tag, release notes, PR title/body, or PR creation | `Commit` |
| Scanning workspace dependencies, auditing packages, checking for CVEs or outdated versions, or installing/updating/removing packages | `Deps` |
| Broad read-only codebase exploration, architecture lookup, file discovery, symbol discovery, or “find where this lives” | `Explore` |
| Root-cause diagnosis, failing tests, regression triage, broken commands, or unclear behavior reproduction | `Debugger` |
| Complex multi-step planning, phased rollout, migration planning, or a scoped execution plan before coding | `Planner` |
| External documentation, upstream behavior, GitHub-source research, or source-backed comparisons before coding or review | `Researcher` |
| Documentation updates, migration notes, contract explanations, walkthroughs, or README/user-facing technical guides | `Docs` |
| Code review, architecture review, security review, maintainability review, regression-risk review, review of a PR/diff, or a codebase **audit** | `Review` |
| xanadAssistant inspect, check, plan, apply, update, repair, factory-restore, or **health check** | `xanadLifecycle` |

## Coding Conventions

- Language: **(not detected)** · Package manager: **(not detected)**
- **Testing**: Always — write tests alongside every code change.
- Read before modifying — never edit a file whose current content you have not read in this task
- No silent error swallowing

## PDCA + Test Scope

Plan → Do → Check → Act on every non-trivial change.

- Before commit, merge, or push in this repository, run `python3 scripts/copilot_audit.py`.
- Default: run the narrowest test suite covering changed paths
- Broaden to the full suite at task completion and before merging

## Operating Modes

**Implement** (default): plan → implement → test.
**Review**: read-only; state findings before proposing fixes.
**Refactor**: no behaviour changes; tests pass before and after.
**Response style**: Balanced — code with brief explanation. · **Ambiguity**: Act then tell — make a reasonable choice and explain it. · **Tone**: Pair programmer — collaborative, iterative, direct.

## Memory

Memory provides workspace-specific rules and cached facts; use it when available, not as lifecycle authority.

### Memory MCP server

When hooks are enabled, a `memory` MCP server is available. Each specialist agent's instruction file defines when and how to use it. The pattern used by every agent is:

1. Call `memory_dump(agent="<agent-name>")` at the start of each task to load rules and cached facts.
2. Follow all returned **rules** unconditionally for the rest of the task.
3. For any **fact** you intend to act on, call `elapsed(start=fact.updated_at)` to verify its age.
4. When you discover something specific to this workspace's commands, tool versions, paths, or established conventions, call `memory_set(agent="<agent-name>", key=..., value=...)` before finishing.
5. If the `memory` server is unavailable, emit one visible note ("⚠️ Memory MCP unavailable: [reason]") then continue without it.

## Skills and Agents

See `## Agent Routing` for the authoritative routing table; this section is a quick-reference index.

### Skills

- `lifecycleAudit` — loaded on demand; run before any lifecycle operation
- `promptReview` — loaded on demand; evaluate or improve Copilot surface files
- `ciPreflight` — loaded on demand; run CI-equivalent checks before push

### Agents

- `Cleaner` — prune stale artefacts, caches, archives, and dead files
- `Commit` — git operations, commit messages, staging, push, pull, PR work
- `Debugger` — diagnose failures and isolate root causes before implementation
- `Deps` — scan dependencies, audit packages, check for vulnerabilities, install/update/remove
- `Docs` — write and update documentation, migration guides, and technical walkthroughs
- `Explore` — broad read-only codebase exploration and architecture lookup
- `Planner` — produce scoped execution plans for multi-step work before implementation
- `Researcher` — gather source-backed external constraints before implementation or review
- `Review` — code, architecture, security, and regression-risk review; handles codebase audits
- `xanadLifecycle` — handles all `inspect`, `health-check`, `health-report`, `plan`, `apply`, `update`, `repair`, `factory-restore`, and **health check** requests
