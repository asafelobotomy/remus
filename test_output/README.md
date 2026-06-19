# Test Output Policy

Use this folder for repository-local processed-ROM test runs and lightweight local notes.

Put durable summaries in [../docs/reports/](../docs/reports/README.md), not in this directory.

## Tracked in git

Only these paths under `test_output/` are committed:

- `README.md` (this file)
- `attention.log` (follow-up queue — one line per open item)

Everything else under `test_output/` (databases, pipeline runs, processed ROMs, `*.log` files) is **local-only** and ignored by git.

## Formalized Pipeline Testing

Run all pipeline tests through the test runner script to ensure consistent output:

```bash
# Default: scan → match → report
./scripts/run_pipeline_test.sh

# Custom label and steps
./scripts/run_pipeline_test.sh --label "serial-fix" --steps scan,match,organize,report

# Dry-run organize step
./scripts/run_pipeline_test.sh --steps scan,match,organize,report --dry-run

# Custom input directory and confidence threshold
./scripts/run_pipeline_test.sh --input /path/to/roms --confidence 50
```

Each run creates a timestamped directory with consistent artifact names:

```text
test_output/full_test_3103_1530/
├── test.db               # SQLite database
├── match-report.txt      # Matching confidence report
├── pipeline.log          # Full stdout/stderr log
├── summary.txt           # Human-readable result summary
└── organized/            # Organized output (if organize step ran)
```

The script automatically prunes test runs older than the 5 most recent.

For ad-hoc cleanup after one-off validation runs, use:

```bash
./scripts/clean-workspace.sh          # remove audit build trees + local DBs
./scripts/clean-workspace.sh --dry-run
./scripts/prune_test_output.sh --dry-run
./scripts/prune_test_output.sh --apply
```

## Rules

- Keep no more than 5 active test runs in this folder at a time (auto-enforced by the script).
- Store temporary notes, command results, and review artifacts here.
- Processed ROM payloads may exist here during local testing, but they should stay temporary and small.
- Put any item that needs follow-up in `attention.log` instead of creating more output trees.
- Remove stale output as soon as the underlying issue is resolved.
- Move any retained validation summary into [../docs/reports/](../docs/reports/README.md).
- **Do not** create ad-hoc `.db` or report files in the root of `test_output/` — use the script.
