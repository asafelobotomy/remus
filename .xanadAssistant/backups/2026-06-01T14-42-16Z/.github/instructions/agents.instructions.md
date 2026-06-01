---
name: Agent Files
applyTo: "agents/*.agent.md"
description: "Conventions for .agent.md files in this workspace — frontmatter schema, body structure, and routing discipline"
---

# Agent File Instructions

## Frontmatter

- Every `.agent.md` file must declare all of: `name`, `description`, `argument-hint`, `model`, `tools`, `agents`, `user-invocable`.
- `name` must match the filename case-insensitively — `Review` → `review.agent.md`.
- `description` must reflect the trigger phrases used in the routing tables in `copilot-instructions.md` and `AGENTS.md`; keep it in sync when either table changes.
- `argument-hint` must describe what a caller should pass — use imperative phrasing ("Describe the task…").
- `model` is a preference-ordered list; the first entry is the default, subsequent entries are fallbacks ordered by closer task-fit before lighter-but-less-capable models.
- `agents` lists only delegatee agents the body explicitly delegates to; do not list agents the body never references.

## Body

- Open with `You are the <Name> agent.` followed immediately by `Your role:` as a single sentence.
- Include a `Do not use this agent for:` section listing at least three out-of-scope scenarios.
- Include an `On every invocation` section with numbered steps defining the agent's procedure.
- For agents that perform destructive operations (e.g., file deletions, git reset --hard, force pushes), include a `Risk tiers` table covering every destructive operation mentioned in the body.
- Call `memory_dump(agent="<name>")` as the first `On every invocation` step when the body references memory or workspace-specific cached facts (e.g., command paths, tool versions, or established project conventions).
- Document each handoff to a delegatee agent with a one-line rationale either inline or in a dedicated handoffs section.
