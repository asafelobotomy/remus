# Test Data Policy

Use repository-local paths when you need ROM inputs or lightweight tracked test notes.

## Use the canonical folders

- `roms/` holds local ROM inputs for repository-scoped work (ROM payloads are never committed).
- `test_output/` holds disposable pipeline runs plus a small tracked follow-up log.

## Tracked vs local under `test_output/`

| Path | Git |
|------|-----|
| `test_output/README.md` | Tracked |
| `test_output/attention.log` | Tracked (follow-up queue) |
| Pipeline run directories, `*.db`, `*.log`, processed ROMs | Local only (gitignored) |

## Keep ROMs local

- Put ROMs, disc images, archives, and other game payloads under `roms/`.
- Do not commit ROM payloads.
- Do not add ROM inventories to tracked documentation.

## Keep test output small

- Keep at most 5 active test cases in `test_output/`.
- Use `test_output/` for processed-ROM test runs plus the lightweight artifacts that help review them.
- Treat processed ROM payloads in `test_output/` as temporary local artifacts.
- Use `test_output/attention.log` for anything that needs follow-up, cleanup, or repair.
- Delete stale test-output artifacts once they are no longer needed.

## Separate payloads from reports

- ROM payloads stay under `roms/`.
- Processed test cases live under `test_output/` only while they are active and within the 5-case limit.
- Durable validation write-ups belong in `docs/reports/`, not under `test_output/`.

This split keeps the repository safe to publish while preserving useful test context.
