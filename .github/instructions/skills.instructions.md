---
name: Skill Files
applyTo: "skills/**/SKILL.md,packs/**/skills/**/SKILL.md"
description: "Conventions for SKILL.md files in this workspace — frontmatter, section structure, and verification discipline"
---

# Skill File Instructions

## Frontmatter

- Every `SKILL.md` must include `name` and `description` in the YAML frontmatter.
- `name` must match the parent directory name exactly.
- `description` must be a single sentence beginning with `Use when:` or precisely stating what the skill does and when.

## Required sections

- Sections must appear in this order: `## When to use`, `## When NOT to use`, then the step or module body, then `## Verify`.
- `## When NOT to use` must include at least one entry covering the case where a more-specific skill should be preferred instead.
- `## Verify` must be a Markdown checklist (`- [ ]` items); each item must reference an observable outcome, not merely a completed step (e.g., "`inspect` output has been read this session", not "Step 3 was completed").
- The skill body must open with a metadata comment block: `> Skill metadata: version "X.Y"; tags [...]; recommended tools [...]`. Start new skills at `version "1.0"` and increment the minor version with each behavioural change. List only tools the skill's steps explicitly call in `recommended tools`.

## Authoring rules

- Steps must be numbered; each step must have a clear action and either a success criterion or an explicit fallback path.
- Multi-module skills must contain 2–6 modules or top-level sections (xanadEval `module-count` threshold).
- External dependencies (MCP servers, other agents, APIs) must each have a documented failure path or fallback.
- Do not embed agent routing decisions inside a skill — a skill describes a procedure, not a routing table.
