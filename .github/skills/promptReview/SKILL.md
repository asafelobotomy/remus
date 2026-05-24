---
name: promptReview
description: "Six-module quality review for .prompt.md, .agent.md, SKILL.md, and .instructions.md files — contradiction detection, ambiguity analysis, persona consistency, cognitive load, coverage gaps, and composition conflict analysis. Integrates xanadEval-backed metrics and LLM-as-judge scoring in Modules 1–5."
---

# Prompt Review

> Skill metadata: version "1.0"; tags [review, prompt, agent, skill, instructions, quality]; recommended tools [read_file, file_search, grep_search, semantic_search]; bundled tools [xanadEval].

Systematic quality review of Copilot surface files. Run all six modules in sequence; each produces zero or more labelled findings. Collect every finding before returning the summary.

## When to use

- Before merging any change to `.prompt.md`, `.agent.md`, `SKILL.md`, or `.instructions.md`
- When a prompt behaves unexpectedly and the cause is unclear
- When asked to review, audit, or improve a Copilot surface file

## When NOT to use

- When the request is to *create or edit* a surface file — this skill reviews; use the `agent-customization` skill for authoring
- When only YAML frontmatter syntax is in question — fix the syntax directly rather than running a full review
- During a lifecycle operation already in progress

---

## Module 1 — Contradiction Detection

Identify directives or rules within the file that produce conflicting instructions when applied together.

After completing the checklist, rate overall contradiction risk (LLM-as-judge): **Low / Medium / High / Critical**. Anti-patterns found at High or Critical severity warrant re-examining the file for subtle scope overlaps before finalising findings.

**What to look for:**

- A positive rule followed by a negative rule that applies to the same condition (e.g. "Always confirm before acting" and "Proceed immediately on user request")
- Risk-tier entries whose permitted actions contradict a scope rule elsewhere in the file
- `tools:` frontmatter that declares a tool the body explicitly prohibits
- "When to use" and "When NOT to use" ranges that overlap — the same trigger satisfies both
- Step sequences where step N undoes the postcondition required by step N+1

**Severity mapping:**

| Contradiction type | Severity |
| --- | --- |
| Behavioral (agent acts vs. must not act) | Critical |
| Scope or tool constraint | High |
| Risk-tier mismatch | High |
| "When to use" / "When NOT to use" overlap | Medium |
| Minor ordering or format inconsistency | Low |

**Output:** For each finding, emit `contradiction: [severity] <section> — <description>. Suggested fix: <fix>`.

---

## Module 2 — Semantic Ambiguity

Find directives that cannot be executed reliably because they lack a precise, observable definition.

After completing the checklist, score overall directive Clarity on a 0–1 scale (LLM-as-judge, 1 = all directives are precisely actionable). A score ≤ 0.5 indicates significant ambiguity needing revision — treat as High severity.

**What to look for:**

- Vague verbs: "handle appropriately", "be careful", "as needed", "if relevant"
- Undefined referents: pronouns or anaphora ("it", "they", "the result") with no clear antecedent in context
- Implicit prerequisites: behavior conditioned on state that is never established by the file (e.g. "if the user has already confirmed" without a confirmation step)
- Scope-free superlatives: "always", "never" applied to conditions that have obvious exceptions not addressed elsewhere
- Measurable quantities given as ranges without a default (e.g. "3–7 items" — what does the agent do by default?)

**For each finding**, provide a rewrite suggestion in the form:
> Original: `<original text>`  
> Suggested: `<precise replacement>`

**Output:** `ambiguity: [severity] <section> — <description>. Suggested rewrite: <rewrite>`.

Severity: High if the ambiguity causes the agent to silently skip a required step; Medium otherwise.

---

## Module 3 — Persona Consistency

Verify that the file presents a coherent, stable identity and tone throughout.

After completing the checklist, rate persona stability (LLM-as-judge): **Stable / Drifting / Conflicted**. A low scope-precision result — over-broad "When to use" definition, or the file assigns multiple distinct personas to the same agent — maps to Drifting (Medium) or Conflicted (High).

**What to look for:**

- Voice drift: sections that shift between first-person ("I will"), second-person ("you should"), and third-person ("the agent must") without an established convention
- Tone register inconsistency: formal technical register in one section, colloquial or chatty register in another
- Conflicting personality directives: "Be concise" vs. "Always explain your reasoning in detail"
- Multiple named roles or personas (e.g. both "You are a reviewer" and "You are a planner" in the same file)
- Response-style instructions that contradict an imported style token from the associated pack

**Output:** `persona: [severity] <section> — <description>. Suggested fix: <fix>`.

Severity: Medium if the inconsistency is detectable mid-conversation; Low if it is a one-off phrasing choice.

---

## Module 4 — Cognitive Load Assessment

Warn when the prompt's structural complexity makes it difficult to apply reliably.

**Gather metrics.** Run these commands from the workspace root:

```
python3 .github/tools/xanadEval/xanadEval.py tokens <path>
python3 .github/tools/xanadEval/xanadEval.py check <path>
```

`tokens` gives token estimate, section count, code block count, and workflow-step detection; use those figures in place of manual estimation. For SKILL.md files, `check` produces spec compliance and advisory flags (`module-count`, `over-specificity`, `complexity`, `eval-presence`, etc.). When xanadEval is unavailable, apply the thresholds below by inspection.

**Metrics — warn or block per section:**

| Metric | Warning threshold | Block threshold |
| --- | --- | --- |
| Conditional nesting depth | > 2 levels | > 3 levels |
| Rules per section | > 7 | > 10 |
| Steps in a single sequence | > 8 | > 12 |
| Compound conditionals (AND/OR chains per sentence) | > 2 | > 3 |
| Repeated constraints across sections | 3+ occurrences | 5+ occurrences |

Emit `cognitive-load: warning` for any metric at or above the warning threshold. Emit `cognitive-load: block` when the block threshold is reached — complexity at that level makes reliable execution unlikely and the file must be restructured before merging.

**xanadEval check advisory flags (SKILL.md only)** — each ✗ maps to the severity shown:

| Advisory flag | Level | Meaning |
| --- | --- | --- |
| `complexity` | block | Structural complexity exceeds heuristic threshold |
| `module-count` | warning | Fewer than 2 or more than 6 modules (acceptable range: 2–6) |
| `over-specificity` | warning | Excessive rigidity that reduces adaptability |
| `negative-delta-risk` | warning | Instructions that could cause the agent to refuse valid work, be less helpful in its target domain, or add friction without a stated safety benefit |
| `max-rules-per-section` | block | Rules per section exceeds the block threshold — cross-reference the metric table above |

If xanadEval reports no workflow steps detected on an agent or prompt file, emit `cognitive-load: warning` — the file may lack clear procedure structure. For skill files, `workflow_steps: not detected` is expected and does not generate a warning.

**Also flag:**
- Tables with more than 10 rows that do not have an obvious sort or lookup key
- Sentences over 40 words
- Numbered lists that mix independent rules with sequential steps in the same list

**Output:** `cognitive-load: [warning|block] <section> — <metric> = <value> (threshold: <threshold>). Suggested fix: <fix>`.

---

## Module 5 — Semantic Coverage

Identify gaps in the file's stated intent: scenarios that should be handled but are not.

After completing the checklist, rate overall coverage Completeness (LLM-as-judge): **Complete / Gaps-present / Incomplete**. Gaps-present or Incomplete confirms coverage-gap findings. Also rate Scope coverage: is the file's scope well-defined, or does it bleed into adjacent agent territory? Output: `scope-coverage: [well-defined | bleeding] — <description>`. Bleeding scope is a Medium-severity coverage-gap finding.

**Checklist — run for every file type:**

- [ ] At least one clear "happy path" is described from trigger to completion
- [ ] At least one explicit failure or error path is described (what to do when a step cannot complete)
- [ ] A "When NOT to use" section exists (required for skills; recommended for agents)
- [ ] The file does not assume a specific tool is always available without providing a fallback when it is not
- [ ] Every external dependency (another agent, MCP server, external API) has a documented failure path
- [ ] If the file declares a multi-step workflow, the last step has a defined termination condition

**File-type-specific checks:**

*Agent files (`.agent.md`):*
- [ ] Handoffs are defined for at least: scope-unclear, unexpected-failure, and out-of-domain cases
- [ ] Risk tiers cover all destructive operations mentioned in the body
- [ ] The frontmatter `tools:` list covers every tool referenced by name in the body

*Skill files (`SKILL.md`):*
- [ ] A `## Verify` checklist is present
- [ ] Every step either has a success criterion or delegates to a fallback
- [ ] `xanadEval check` spec compliance passes — failing checks (`spec-frontmatter`, `spec-name`, `spec-dir-match`, `spec-description`) are High-severity findings here. If xanadEval is unavailable, skip and emit: `coverage-gap: [medium] spec-compliance — xanadEval unavailable; spec checks could not be run`.
    - [ ] If an eval suite is expected: `xanadEval check` `eval-presence` advisory passes; absence is a Medium-severity finding. If xanadEval is unavailable, note the gap manually.
  - [ ] If coverage gaps were found: run `python3 .github/tools/xanadEval/xanadEval.py suggest --dry-run <path>` from the workspace root; each expected eval task not yet present is a Low-severity finding. If xanadEval is unavailable, note the gap manually.

*Instructions files (`.instructions.md`):*
- [ ] `applyTo:` pattern is present and non-empty
- [ ] Rules are expressed as imperatives, not preferences ("Do X", not "You might want to X")

**Output:** `coverage-gap: [severity] <section> — <description>. Suggested addition: <addition>`.

Severity: High if the gap leaves an error path unhandled; Medium if it is a missing best-practice section; Low otherwise.

---

## Module 6 — Composition Conflict Analysis

Detect conflicts between the file under review and any files it imports or references.

**Step 1 — Identify imports.** Scan the file for:
- Markdown links: `[label](path)` where `path` resolves to another surface file in the workspace
- Template variable references: `{{token:name}}` or `{{agent:name:key}}` that expand from a pack tokens file
- Explicit `applyTo:` references that match another file's scope

Use workspace file-reading tools to read each referenced file. If a referenced file cannot be found, emit `composition: [high] import unresolved — <path>`.

**Step 2 — Compare each import pair.**

For each (parent, import) pair, check:
- **Direct contradiction**: a rule in the parent negates a rule in the import for the same condition
- **Shadowing**: a local rule restates and subtly changes a rule from the import, creating a silent override
- **Duplicate rules that have drifted**: the same rule appears in both files but with different wording that may produce different behavior
- **Circular reference**: the import references back to the parent or creates a cycle
- **Pack token conflicts**: a `{{token}}` expanded from the active pack contradicts a hardcoded directive in the body

**Step 3 — Report findings.** For each conflict, show both the parent excerpt and the import excerpt side by side.

**Output:** `composition: [severity] <parent-section> ↔ <import-file>#<import-section> — <description>. Suggested resolution: <resolution>`.

Severity: Critical for direct behavioral contradiction; High for shadowing; Medium for duplicate drift; Low for style inconsistency between files.

---

## Finding output format

Produce a consolidated findings table after running all six modules:

| Severity | Module | Section | Finding | Suggested fix |
| --- | --- | --- | --- | --- |
| Critical | Contradiction | `## Commit workflow` | Step 4 negates the lock established by step 2 | Remove step 4 or add a guard condition |
| … | … | … | … | … |

Use these severity labels consistently: **Critical**, **High**, **Medium**, **Low**.

If a module produces zero findings, include one row: `— | <module> | — | No findings. | —`.

Close with a summary line:
> **Result:** `N` critical, `N` high, `N` medium, `N` low — [ready to merge | needs revision before merge | block: restructure required]

Decision rules:
- Any Critical → **block: restructure required**
- Any High → **needs revision before merge**
- Medium or lower only → **ready to merge** (with suggestions noted)

---

## Verify

- [ ] All six modules run and each produced at least a "no findings" row
- [ ] Module 4: `xanadEval tokens` figures used, or xanadEval unavailability noted
- [ ] For SKILL.md files: `xanadEval check` spec violations mapped to Module 5 findings
- [ ] For SKILL.md files: LLM-as-judge quality ratings (contradiction risk, Clarity score, persona stability, coverage Completeness) applied in Modules 1/2/3/5
- [ ] Composition imports resolved — linked files read before reporting conflict findings
- [ ] Every ambiguity finding includes a rewrite suggestion
- [ ] Every cognitive-load finding includes the measured metric value and the threshold
- [ ] Every coverage-gap finding includes a suggested addition
- [ ] The consolidated findings table is present
- [ ] The summary line states a clear merge decision
