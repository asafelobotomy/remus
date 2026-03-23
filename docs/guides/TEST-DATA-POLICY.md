# Test Data Policy

Use repository-local paths when you need ROM inputs or tracked test-output notes.

## Use the canonical folders

- `roms/` holds local ROM inputs for repository-scoped work.
- `test_output/` holds tracked test-output notes and summaries.

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
- Syncable notes, hashes, summaries, and review logs stay under `test_output/`.

This split keeps the repository safe to publish while preserving useful test context.