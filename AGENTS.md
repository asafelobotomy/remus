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
| **Code** | `coding.agent.md` | GPT-5.1 / Claude Sonnet 4.6 | Implement features, refactor, multi-step coding tasks | Review, Doctor, Fast, Researcher, Explore |
| **Review** | `review.agent.md` | GPT-5.4 / Claude Opus 4.6 | Deep code review and architectural analysis with Lean/Kaizen critique | Code, Fast, Researcher |
| **Doctor** | `doctor.agent.md` | Claude Sonnet 4.6 | Read-only health check — instructions, agents, MCP config, workspace files | Code, Update, Researcher, Explore |
| **Update** | `update.agent.md` | Claude Sonnet 4.6 | Fetch and apply upstream instruction updates, or restore from backup | Doctor |
| **Setup** | `setup.agent.md` | Claude Sonnet 4.6 | First-time project setup and onboarding from copilot-instructions-template | Doctor |
| **Researcher** | `researcher.agent.md` | Claude Sonnet 4.6 | Online and offline research — fetch docs, track URLs, structured output | Code, Doctor |
| **Fast** | `fast.agent.md` | Claude Haiku 4.5 | Quick questions, syntax lookups, and lightweight single-file edits | Code |
| **Explore** | `explore.agent.md` | Claude Haiku 4.5 | Fast read-only codebase exploration and Q&A subagent | *(none — leaf node)* |

---

## Canonical Triggers

### Update agent

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Update your instructions` | Fetch upstream template, compare versions, apply changes |
| `Check for instruction updates` | Same as above |
| `Update from copilot-instructions-template` | Same as above |
| `Sync instructions with the template` | Same as above |
| `Check the template for updates` | Same as above |
| `Force check instruction updates` | Bypasses version equality check |
| `Restore instructions from backup` | Restore from `.github/archive/` backup |
| `Roll back the instructions update` | Same as above |
| `List instruction backups` | List available backups in `.github/archive/` |

### Doctor agent

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Health check` | Run D1–D9 checks across all instruction infrastructure |
| `Check your heartbeat` | Fire heartbeat protocol (see `.copilot/workspace/HEARTBEAT.md`) |
| `Check attention budget` | D1 only — count lines in copilot-instructions.md |
| `Check agent files` | D4 only — validate all `.github/agents/` frontmatter |
| `Check MCP config` | D5 only — validate `.vscode/mcp.json` |

### Setup agent

| Trigger phrase | Behaviour |
|----------------|-----------|
| `Set up this project` | Run full onboarding from copilot-instructions-template |
| `Re-run setup` | Refresh scaffold files |

---

## Subagent Governance

- Maximum subagent depth: **3** (defined in §9 of copilot-instructions.md)
- Each agent's `agents:` allow-list restricts which subagents it may invoke
- All subagents inherit §11 Tool Protocol, §12 Skill Protocol, §13 MCP Protocol
- Subagent output must include: files changed, LOC delta, test result, baseline breaches

---

## Handoff Chains

```
User → Code → Review → Code (iterate)
             ↘ Doctor (health check)
User → Doctor → Code (apply fixes)
              ↘ Update (version behind)
User → Update → Doctor (post-update health check)
User → Researcher → Code (implement findings)
                  ↘ Doctor (verify written files)
User → Fast → Code (escalate large tasks)
```

---

*See also: `.github/copilot-instructions.md` · `.github/agents/` · `.copilot/workspace/` · `.github/skills/` · `JOURNAL.md`*
