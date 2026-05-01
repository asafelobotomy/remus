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
| **Code** | `coding.agent.md` | GPT-5.3-Codex / Claude Sonnet 4.6 | Implement features, refactor, multi-step coding tasks | Review, Audit, Researcher, Explore, Commit, Organise, Planner, Docs, Debugger, Cleaner |
| **Review** | `review.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Deep code review and architectural analysis with Lean/Kaizen critique | Code, Audit, Organise, Docs, Debugger, Cleaner |
| **Audit** | `audit.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Read-only health check and security audit — structural validation, upstream comparison, OWASP, secrets | Code, Setup, Researcher, Extensions, Organise, Planner, Cleaner |
| **Setup** | `setup.agent.md` | Claude Sonnet 4.6 | Template lifecycle — first-time setup, upstream updates, backup restore, and factory restore | Audit, Extensions, Organise, Researcher |
| **Researcher** | `researcher.agent.md` | Claude Sonnet 4.6 | Online and offline research — fetch docs, track URLs, structured output | Code, Audit, Explore, Docs, Planner |
| **Fast** | `fast.agent.md` | Claude Haiku 4.5 | Quick questions, syntax lookups, and lightweight single-file edits | Code, Explore, Commit |
| **Explore** | `explore.agent.md` | Claude Haiku 4.5 | Fast read-only codebase exploration and Q&A subagent | Researcher |
| **Extensions** | `extensions.agent.md` | Claude Sonnet 4.6 | VS Code extension management, profiles, and workspace configuration | Code, Audit, Organise, Researcher |
| **Commit** | `commit.agent.md` | GPT-5.2 / Claude Sonnet 4.6 | Full git lifecycle — stage, commit, push, pull, rebase, merge, branch, stash, tag, release, PR creation | Code, Review, Audit, Debugger, Organise, Cleaner |
| **Debugger** | `debugger.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Diagnose failures, isolate root causes, triage regressions, and propose minimal fix paths | Code, Researcher, Audit, Planner |
| **Docs** | `docs.agent.md` | Claude Sonnet 4.6 | Draft and update project documentation, walkthroughs, migration notes, README sections | Code, Researcher, Review, Explore |
| **Organise** | `organise.agent.md` | GPT-5.3-Codex / Claude Sonnet 4.6 | Subagent-only structural worker — organise directories, move files, fix broken pathing | Code, Explore, Docs |
| **Planner** | `planner.agent.md` | GPT-5.4 / Claude Sonnet 4.6 | Break down complex work into scoped execution plans, file lists, risks, verification steps | Code, Explore, Researcher, Debugger, Docs |
| **Cleaner** | `cleaner.agent.md` | GPT-5.3-Codex / Claude Sonnet 4.6 | Repository hygiene — prune stale artefacts, caches, archives, and dead files | Code, Audit, Organise, Docs, Commit |
| ~~Doctor~~ | `doctor.agent.md` | — | *Deprecated: replaced by the Audit agent. Kept for reference.* | — |

---

## Canonical Triggers

### Setup agent (update mode)

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

### Audit agent

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Health check` | Run D1–D14 checks across all instruction infrastructure |
| `Security audit` | Run S1–S10 security checks (OWASP, secrets, injection, supply-chain) |
| `Full audit` | Run both D1–D14 health checks and S1–S10 security checks |
| `Check your heartbeat` | Fire heartbeat protocol (see `.copilot/workspace/HEARTBEAT.md`) |
| `Check attention budget` | D1 only — count lines in copilot-instructions.md |
| `Check agent files` | D4 only — validate all `.github/agents/` frontmatter |
| `Check MCP config` | D5 only — validate `.vscode/mcp.json` |

### Setup agent (setup mode)

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Set up this project` | Run full onboarding from copilot-instructions-template |
| `Re-run setup` | Refresh scaffold files |

---

## Canonical Protocol Sources

- First-time setup: [SETUP.md](SETUP.md)
- Update, backup restore, and factory restore: [UPDATE.md](UPDATE.md)
- Canonical inventory and counts: `.copilot/workspace/workspace-index.json`
- Canonical managed-file manifest: `.github/copilot-version.md`

## Skill Inventory

Project skills live under `.github/skills/`.
Use `.github/copilot-version.md` as the canonical inventory during audits.

Installed skills:
- `accessibility-review`, `agentic-workflows`, `api-design`, `audit-procedures`, `changelog-entry`, `commit-preflight`, `compress-prose`, `conventional-commit`, `create-adr`, `dependency-update`, `docker-scaffold`, `env-config`, `extension-review`, `fix-ci-failure`, `git-workflows`, `issue-triage`, `lean-pr-review`, `mcp-builder`, `mcp-management`, `onboarding-docs`, `performance-profiling`, `plugin-management`, `refactor-extract`, `security-audit`, `skill-creator`, `skill-management`, `tech-debt-audit`, `test-coverage-review`, `tool-protocol`, `webapp-testing`

## Compatibility Notes

`doctor.agent.md` is retained as a compatibility-only reference during the Audit-agent transition.
`security.agent.md` is intentionally absent because upstream merged that role into `audit.agent.md` plus the `security-audit` skill.

---

## Subagent Governance

- Maximum subagent depth: **3** (defined in §9 of copilot-instructions.md)
- Each agent's `agents:` allow-list restricts which subagents it may invoke
- All subagents inherit §11 Tool Protocol, §12 Skill Protocol, §13 MCP Protocol, §14 Workspace Knowledge
- Subagent output must include: files changed, LOC delta, test result, baseline breaches

---

## Handoff Chains

```
User → Code → Review → Code (iterate)
             ↘ Audit (health check or security scan before done)
             ↘ Commit (stage and publish changes)
User → Audit → Code (apply fixes)
             ↘ Setup (version behind)
User → Setup → Audit (post-update health check)
User → Researcher → Code (implement findings)
                  ↘ Audit (verify written files)
User → Fast → Code (escalate large tasks)
            ↘ Commit (stage and publish)
User → Commit → Review (review before commit)
              ↘ Audit (post-commit security scan)
User → Extensions → Audit (post-config health check)
User → Debugger → Code (implement the fix)
User → Planner → Code (implement the plan)
User → Docs → Review (review written docs)
```

---

*See also: `.github/copilot-instructions.md` · `.github/agents/` · `.copilot/workspace/` · `.github/skills/` · `JOURNAL.md`*
