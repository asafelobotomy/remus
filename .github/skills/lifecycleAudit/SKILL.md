---
name: lifecycleAudit
description: Audit xanadAssistant workspace lifecycle state — inspect install status, surface repair reasons, and validate the lockfile before proposing install, update, repair, or restore operations
---

# Lifecycle Audit

> Skill metadata: version "1.0"; license MIT; tags [xanadAssistant, lifecycle, inspect, repair, lockfile]; recommended tools [codebase, runCommands].

Systematic lifecycle state review before any install, update, repair, or factory-restore operation.

## When to use

- Before proposing any `setup`, `update`, `repair`, or `factory-restore` operation
- When a workspace's install state is unknown, stale, or suspect
- When `repairReasons` or `needsMigration` are referenced in a conversation

## When NOT to use

- During an already-in-progress lifecycle operation — proceed without auditing mid-flight
- When the user explicitly requests a specific operation and state is already known

## Steps

1. **Inspect** — run `python3 xanadAssistant.py inspect --workspace . --package-root <xanad-root> --json` and verify `installState` and `manifestSummary`.

2. **Check** — run `python3 xanadAssistant.py check --workspace . --package-root <xanad-root> --json` and read `status`, `warnings`, and `result.summary`. If you need machine-readable `repairReasons`, read them from `plan` output rather than `check`.

3. **Classify the state**:

   | `installState` | `check.status` / `plan.repairReasons` | Action |
   |---|---|---|
   | `installed` | `clean` and no plan repair reasons | Proceed with intended operation |
   | `installed` | `drift` or non-empty plan repair reasons | Run `repair` first, then re-check |
   | `not-installed` | any | Run `setup` |
   | any + `needsMigration: true` | any | Run `repair` to migrate lockfile shape first |

4. **Surface findings** — report `installState`, `selectedPacks`, `profile`, `check.status`, and any plan `repairReasons` before planning. Prefer `plan` output over ad-hoc file edits.

5. **Ownership** — keep managed vs skipped surfaces explicit. Do not edit files with `ownership: local` without first running `plan` and reviewing the output.

## Verify

- [ ] `inspect` output has been read this session
- [ ] `check.status` is `clean` or the user has acknowledged the reported drift
- [ ] plan `repairReasons` is empty or the user has acknowledged them
- [ ] `needsMigration` is false or repair has been run
- [ ] Memory health warnings (`memory_mcp_missing`, `memory_mcp_unregistered`, `memory_db_schema_corrupt`) are noted if present — a missing DB is first-run-safe (info only); missing hook or corrupt schema warrants repair