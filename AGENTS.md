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
| **Code** | `coding.agent.md` | GPT-5.1 / Claude Sonnet 4.6 | Implement features, refactor, multi-step coding tasks | Review, Doctor, Fast, Researcher, Explore, Security |
| **Review** | `review.agent.md` | GPT-5.4 / Claude Opus 4.6 | Deep code review and architectural analysis with Lean/Kaizen critique | Code, Fast, Researcher, Doctor, Explore, Security |
| **Doctor** | `doctor.agent.md` | Claude Sonnet 4.6 | Read-only health check — instructions, agents, MCP config, workspace files | Code, Setup, Researcher, Explore, Security, Extensions |
| **Setup** | `setup.agent.md` | Claude Sonnet 4.6 | Template lifecycle — first-time setup, upstream updates, and backup restore | Doctor, Code, Extensions, Researcher, Explore |
| **Researcher** | `researcher.agent.md` | Claude Sonnet 4.6 | Online and offline research — fetch docs, track URLs, structured output | Code, Doctor, Explore, Security |
| **Fast** | `fast.agent.md` | Claude Haiku 4.5 | Quick questions, syntax lookups, and lightweight single-file edits | Code, Explore |
| **Explore** | `explore.agent.md` | Claude Haiku 4.5 | Fast read-only codebase exploration and Q&A subagent | Researcher |
| **Extensions** | `extensions.agent.md` | Claude Sonnet 4.6 | VS Code extension management, profiles, and workspace configuration | Security, Researcher, Doctor, Fast, Explore |
| **Security** | `security.agent.md` | GPT-5.4 / Claude Opus 4.6 | Read-only security audit — OWASP, secrets, injection, supply-chain | Code, Doctor, Researcher, Explore, Review |

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

### Setup agent (setup mode)

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
             ↘ Security (security check before done)
User → Doctor → Code (apply fixes)
              ↘ Setup (version behind)
              ↘ Security (security diagnostics)
User → Setup → Doctor (post-update health check)
User → Researcher → Code (implement findings)
                  ↘ Doctor (verify written files)
User → Fast → Code (escalate large tasks)
User → Security → Code (apply fixes)
               ↘ Doctor (post-fix health check)
User → Extensions → Doctor (post-config health check)
```

---

*See also: `.github/copilot-instructions.md` · `.github/agents/` · `.copilot/workspace/` · `.github/skills/` · `JOURNAL.md`*
