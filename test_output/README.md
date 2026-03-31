# Test Output Policy

Use this folder for repository-local processed-ROM test runs and the tracked notes that explain them.

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

```
test_output/full_test_3103_1530/
├── test.db               # SQLite database
├── match-report.txt      # Matching confidence report
├── pipeline.log          # Full stdout/stderr log
├── summary.txt           # Human-readable result summary
└── organized/            # Organized output (if organize step ran)
```

The script automatically prunes test runs older than the 5 most recent.

## Rules

- Keep no more than 5 active test runs in this folder at a time (auto-enforced by the script).
- Store notes, command results, hashes, summaries, and review artifacts here.
- Processed ROM payloads may exist here during local testing, but they should stay temporary and small.
- Put any item that needs follow-up in `attention.log` instead of creating more output trees.
- Remove stale output as soon as the underlying issue is resolved.
- **Do not** create ad-hoc `.db` or report files in the root of `test_output/` — use the script.

Only lightweight notes and logs are intended to remain tracked in GitHub.