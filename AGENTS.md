# AGENTS.md — Remus

> AI agent entry point. All VS Code Copilot agents for this project are defined in
> `.github/agents/`. This file is the canonical index and trigger reference.
>
> Primary guidance document: `.github/copilot-instructions.md`
> Version stamp: `.github/copilot-version.md`

---

## Agent Inventory

| Agent | File | Model (primary) | Role | Allow-list |
|-------|------|-----------------|------|------------|
| **Cleaner** | `cleaner.agent.md` | Claude Sonnet 4.6 | Repository hygiene — prune stale artefacts, caches, archives, and dead files | Review, Organise, Docs, Commit |
| **Commit** | `commit.agent.md` | Claude Sonnet 4.6 | Full git lifecycle — stage, commit, push, pull, rebase, merge, branch, stash, tag, release, PR creation | Explore, Review, Debugger |
| **Debugger** | `debugger.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Diagnose failures, isolate root causes, triage regressions, and propose minimal fix paths | Explore, Review, Planner, Researcher |
| **Deps** | `deps.agent.md` | Claude Sonnet 4.6 | Scan dependencies, audit packages, check for vulnerabilities, install/update/remove | Explore, Researcher, Review |
| **Docs** | `docs.agent.md` | Claude Sonnet 4.6 | Draft and update project documentation, walkthroughs, migration notes, README sections | Researcher, Review, Explore, Planner |
| **Explore** | `explore.agent.md` | GPT-5.4 mini / Claude Haiku 4.5 | Fast read-only codebase exploration and Q&A subagent | *(none)* |
| **Organise** | `organise.agent.md` | Claude Sonnet 4.6 | Subagent-only structural worker — organise directories, move files, fix broken pathing | Explore, Docs |
| **Planner** | `planner.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Break down complex work into scoped execution plans, file lists, risks, and verification steps | Explore, Debugger, Review, Researcher, Docs |
| **Researcher** | `researcher.agent.md` | Claude Sonnet 4.6 | Online and offline research — fetch docs, track URLs, structured output | Explore, Planner, Review, Docs |
| **Review** | `review.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Deep code review and architectural analysis with Lean/Kaizen critique | Explore, Debugger, Planner, Researcher |
| **Triage** | `triage.agent.md` | Claude Haiku 4.5 | First-pass complexity assessment before choosing execution path | Planner |
| **xanadLifecycle** | `xanadLifecycle.agent.md` | Claude Sonnet 4.6 | Handles all inspect, check, plan, apply, update, repair, factory-restore requests | Explore, Debugger, Planner |

---

## Canonical Triggers

### xanadLifecycle agent

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Update your instructions` | Fetch upstream template, compare versions, apply changes |
| `Check for instruction updates` | Same as above |
| `Update from copilot-instructions-template` | Same as above |
| `Sync instructions with the template` | Same as above |
| `Check the template for updates` | Same as above |
| `Force check instruction updates` | Bypasses version equality check |
| `Restore instructions from backup` | Restore from `.github/archive/` backup |
| `Factory restore instructions` / `Reinstall instructions from scratch` | Archive current managed files, remove them, and reinstall from the latest upstream payload |
| `Roll back the instructions update` | Same as above |
| `List instruction backups` | List available backups in `.github/archive/` |

### Health checks and security audit (via Review agent)

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Health check` | Run D1–D14 checks across all instruction infrastructure |
| `Security audit` | Run S1–S10 security checks (OWASP, secrets, injection, supply-chain) |
| `Full audit` | Run both D1–D14 health checks and S1–S10 security checks |
| `Check attention budget` | D1 only — count lines in copilot-instructions.md |
| `Check agent files` | D4 only — validate all `.github/agents/` frontmatter |
| `Check MCP config` | D5 only — validate `.vscode/mcp.json` |

### xanadLifecycle agent (setup mode)

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Set up this project` | Run full onboarding from copilot-instructions-template |
| `Re-run setup` | Refresh scaffold files |

---

## Canonical Protocol Sources

- Canonical inventory and counts: `.github/xanadAssistant-lock.json` (per-file hash inventory)
- Canonical managed-file manifest: `.github/copilot-version.md` (version and profile)

## Skill Inventory

Project skills live under `.github/skills/`.
For audits, use `.github/xanadAssistant-lock.json` as the canonical managed-file manifest.

Installed skills:

- `ciPreflight`, `lifecycleAudit`

## Compatibility Notes

The Audit, Code, Setup, Fast, and Extensions agent roles from an earlier template version have been consolidated:

- Health-check and security-audit work is handled by the **Review** agent.
- Template lifecycle (setup, update, repair) is handled by the **xanadLifecycle** agent.
- Extension and workspace config work is handled directly through VS Code or the **xanadLifecycle** agent.

---

## Subagent Governance

- Maximum subagent depth: **3** (defined in §9 of copilot-instructions.md)
- Each agent's `agents:` allow-list restricts which subagents it may invoke
- All subagents inherit §11 Tool Protocol, §12 Skill Protocol, §13 MCP Protocol, §14 Workspace Knowledge
- Subagent output must include: files changed, LOC delta, test result, baseline breaches

---

## Handoff Chains

```text
User → Review → Commit (apply fixes, then stage and publish)
User → Debugger → (implement fix)
              ↘ Review (verify the fix)
User → Planner → (implement the plan)
User → Researcher → (implement findings)
                  ↘ Review (verify written files)
User → Commit → Review (review before committing)
User → Docs → Review (review written docs)
User → Deps → Review (verify dependency changes)
User → xanadLifecycle → Review (post-lifecycle health check)
User → Cleaner → Commit (stage cleaned files)
```

---

*See also: `.github/copilot-instructions.md` · `.github/agents/` · `.copilot/workspace/` · `.github/skills/` · `JOURNAL.md`*
