---
name: ciPreflight
description: "Use when: running pre-commit or pre-push CI-equivalent checks in any workspace — discovers the project's CI workflow commands using workspace tools, filters for local executability, scopes to staged changes, and runs checks cheapest-first before delegating failures."
---

# CI Preflight

> Skill metadata: version "1.1"; tags [commit, preflight, ci, workflow, generic]; recommended tools [file_search, read_file, grep_search, run_in_terminal, vscode_askQuestions].

Discover and execute a workspace's CI checks locally before commit or push,
using workspace tools to read actual workflow definitions rather than assuming
knowledge of the project's specific tooling or conventions.

## When to use

- Before any commit or push when CI check coverage is needed
- When the project's CI checks are unknown, vary by workspace, or may have changed
- When the workspace may use any language, framework, or build system

## When not to use

- When the workspace has a project-specific preflight skill — prefer that skill
  over this generic one (it will know the project's exact commands and repair
  steps)
- When the user has explicitly asked to skip verification
- When there is nothing staged and no proposed file list to verify against

## Steps

### 0. Discover CI workflow files

Use workspace file search tools to list files under `.github/workflows/`.

For each file found, read its full contents with file-reading tools.

**Keep** a file if its `on:` key includes `push` or `pull_request`.
**Skip** a file that only triggers on `schedule`, `workflow_dispatch`,
`release`, or deployment events — those require infrastructure or secrets that
cannot be reproduced locally.

### 1. Extract locally-executable run steps

For each kept workflow file, collect every `jobs.<id>.steps[]` entry that has
a `run:` key.

**Exclude** a step if its `run:` block:
- References `${{ secrets.* }}` — requires external credentials
- References environment variables that are only available on GitHub Actions
  runners (e.g. `GITHUB_RUN_ID`, `RUNNER_TOOL_CACHE`)
- Invokes `actions/deploy`, upload-artifact, download-artifact, or any step
  that publishes or transfers artefacts
- Calls a deployment or release tool (`gh release`, `npm publish`,
  `docker push`, etc.)

**Include** a step if its `run:` is a self-contained interpreter or tool
invocation that operates on local files (e.g. `python3 ...`, `npm test`,
`make check`, `go test ./...`, `cargo test`, `./gradlew test`).

### 2. Scope to staged changes

Run `git diff --cached --name-only` with a terminal tool to get the staged file
list.

Narrow the extracted check list by that scope:
- Skip Python-specific checks if no `.py` files or test-config files are staged
- Skip Node checks if no `.js`, `.ts`, or `package.json` are staged
- Keep any check whose scope cannot be narrowed (e.g. a global lint or a
  manifest freshness gate that affects the whole repo)

### 3. Fall back to project-structure detection

If Step 1 found no usable run steps (no workflow files, or all steps were
excluded), detect the verification command from workspace files using workspace
search and file-reading tools:

| Signal file or folder | Inferred command |
|---|---|
| `tests/` with `test_*.py` files | `python3 -m unittest discover -s tests -p 'test_*.py'` |
| `pytest.ini`, `pyproject.toml` with `[tool.pytest]` | `python3 -m pytest` |
| `package.json` with a `test` script | `npm test` |
| `Makefile` with a `test` target | `make test` |
| `go.mod` | `go test ./...` |
| `Cargo.toml` | `cargo test` |
| `Gemfile` with rspec | `bundle exec rspec` |

If fallback detection still finds nothing credible, stop and report that no
locally-executable CI-equivalent command could be derived from the workspace.

### 4. Order checks cheapest-first

Group extracted commands into tiers and run in this order, stopping at the
first blocker before running later tiers:

1. **Static / format / lint** — fastest, no side effects
   (e.g. `ruff check`, `eslint`, `golangci-lint`, budget/LOC scripts)
2. **Type checks** — moderate cost (e.g. `mypy`, `tsc --noEmit`)
3. **Unit tests** — moderate to expensive
4. **Integration / e2e tests** — most expensive; skip if not in scope of
   staged changes

When tier assignment is ambiguous, prefer the order commands appear in the
workflow file — the project author likely ordered them by cost already.

### 5. Execute and handle each result

Run each command with a terminal tool. For each result:

| Outcome | Action |
|---|---|
| Exit 0 | Continue to the next check |
| Stale generated artifact (exit nonzero + recognisable regen command in output) | Re-run the generator, explicitly `git add` regenerated outputs, then re-run the check |
| Unit or lint failure | Delegate to `Debugger` with the exact failure output and staged file list; apply the minimal fix returned; re-run the failing check |
| Budget / LOC / format violation | Surface the exact violation output to the user; ask whether to fix now or accept residual risk before proceeding |
| Template-safety violation (unresolved `{{}}` tokens in a template file) | Block — do not commit until resolved |
| Step requires secrets or infra (detected mid-run) | Skip; note in summary — not a blocker |
| Any other nonzero exit | Surface exact stdout + stderr; use `vscode_askQuestions` if user input is required |

### 6. Return a summary to the caller

Return:
- The workflow files read and the commands extracted from them (or "detected
  from project structure" if Step 4 was used)
- Which checks were skipped, and why
- Whether any artifacts were regenerated and staged
- A clear **pass**, **block**, or **residual-risk** outcome for the caller to
  act on

## Verify

- [ ] Workflow files listed and read with workspace tools — not assumed from memory
- [ ] Only push/PR-triggered workflows included; others explicitly skipped
- [ ] Steps requiring secrets or external infra excluded and noted
- [ ] `git diff --cached --name-only` used to scope checks to staged files
- [ ] Commands executed in cheapest-first order; stopped at first blocker
- [ ] Stale artifacts repaired and restaged before re-running the failing check
- [ ] `Debugger` delegated to for test/lint failures, not ad-hoc guesses
- [ ] Summary returned with pass / block / residual-risk outcome
