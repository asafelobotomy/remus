---
name: Eval Files
applyTo: "evals/**/*.yaml,packs/**/evals/**/*.yaml"
description: "Conventions for eval.yaml suite files and task files in this workspace — naming, schema, graders, and coverage"
---

# Eval File Instructions

## eval.yaml

- `name` must be `"<SuiteName>-eval"` where `SuiteName` matches the parent directory name exactly.
- `description` must state what agent or skill behavior the suite evaluates.
- `graders` must include at least one entry; use `text` (with a `pattern` config key) for keyword presence and `behavior` (with a `max_tokens` config key) for token or timing bounds. An `llm` grader type is available for semantic correctness; consult the eval schema before using it as its config fields are not standardised here.
- `tasks` must contain exactly one glob entry (`- "tasks/*.yaml"`); do not list individual task files by name.

## Task files

- File `id` must match the filename without the `.yaml` extension.
- Every task must include `description`, `prompt`, and at least one of `expected` or `expected_absent`.
- Every task must have a `tags` list that includes `smoke`.
- Name task files by role: `basic-invocation.yaml` for the primary happy-path invocation of a skill; `positive-trigger-N.yaml` to verify an agent or skill triggers on a matching prompt; `negative-trigger-N.yaml` to verify it does not trigger on unrelated input.

## Coverage requirements

- Every agent eval suite must contain at least `positive-trigger-1.yaml` and `negative-trigger-1.yaml`.
- Every skill eval suite must contain at least `basic-invocation.yaml`.
- `expected` values are plain strings matched against the response; `expected_absent` values are regex patterns that must not appear in the response.
- Pack eval suites under `packs/**/evals/` follow the same coverage requirements as top-level suites.
