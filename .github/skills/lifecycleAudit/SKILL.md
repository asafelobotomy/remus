---
name: lifecycleAudit
description: Check xanadAssistant workspace health — inspect install status, surface repair reasons, and validate the lockfile before proposing install, update, repair, or restore operations
---

# Lifecycle Health Check

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

> `<xanad-root>` is the directory containing `xanadAssistant.py`. Use `.` when running from the package root (self-hosted install) or the absolute path to the installed package otherwise. If `xanadAssistant.py` is not found, halt and report the error to the user before proceeding.

1. **Inspect** — run `python3 xanadAssistant.py inspect --workspace . --package-root <xanad-root> --json` and verify `installState` and `manifestSummary`. If the command exits non-zero, halt and report the error before proceeding.

2. **Check** — run `python3 xanadAssistant.py health-check --workspace . --package-root <xanad-root> --json` and read `status`, `warnings`, and `result.summary`. Treat exit `7` as a normal drift signal to classify, not as an execution failure. Halt only on other non-zero exits. If you need machine-readable `repairReasons`, run `python3 xanadAssistant.py plan repair --workspace . --package-root <xanad-root> --json` and read them from `plan` output rather than `health-check`.

3. **Classify the state** — the `needsMigration: true` row takes priority over all other rows:

   | `installState` | `health-check.status` / `plan.repairReasons` | Action |
   | --- | --- | --- |
   | `installed` | `clean` and no plan repair reasons | Proceed with intended operation |
   | `installed` | `drift` or non-empty plan repair reasons | Run `repair` first, then re-check |
   | `not-installed` | any | Run `setup` |
   | any + `needsMigration: true` | any | Run `repair` to migrate lockfile shape first |

4. **Surface findings** — report `installState`, `selectedPacks`, `profile`, `health-check.status`, and any plan `repairReasons` before proposing the intended operation. Prefer `plan` output over ad-hoc file edits.

5. **Ownership** — keep managed vs skipped surfaces explicit. Do not edit files with `ownership: local` without first running `plan` and reviewing the output.

## Verify

- [ ] `inspect` output has been read this session
- [ ] `check.status` is `clean` or the user has acknowledged the reported drift
- [ ] plan `repairReasons` is empty or the user has acknowledged them
- [ ] `needsMigration` is false or repair has been run
- [ ] Memory health warnings (`memory_mcp_missing`, `memory_mcp_unregistered`, `memory_db_schema_corrupt`) are noted if present — a missing DB is first-run-safe (info only); missing hook or corrupt schema warrants proposing a repair operation to the user
