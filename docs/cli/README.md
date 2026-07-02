# CLI Documentation

Command-line interface documentation and implementation details for Remus.

## Contents

- **[EXIT-CODES.md](EXIT-CODES.md)** - Exit code contract and primary-action rules
- **[CLI-ENHANCEMENTS.md](CLI-ENHANCEMENTS.md)** - CLI feature enhancements and improvements
- **[CLI-IMPLEMENTATION-SUMMARY.md](CLI-IMPLEMENTATION-SUMMARY.md)** - Implementation details and technical summary
- **[CLI-TEST-REPORT.md](CLI-TEST-REPORT.md)** - CLI testing report and test coverage

## JSON output

`--json` / `--mod-json` emit machine-readable output for: `--stats`, `--list`, `--info`, and mod catalog commands.

## Quick Start

See the main [README](../README.md) for general project information.

For usage examples, see [examples.md](../examples.md).

## Compendium enrichment flags

| Flag | Effect |
|------|--------|
| `--force-enrichment` | Bypass build-level enrichment fingerprint skip; per-pass gap checks and per-game skip sets still apply |
| `--enrich-source <name>` | Run a single enrichment pass (repeatable via build orchestration) |
| `--skip-fts` | Skip FTS rebuild after enrichment when no title/canonical fields changed |
| `--skip-consolidate-thumbnails` | Defer artwork consolidate; full build re-runs `remus-thumbnails` enrich after consolidate |

See [data/compendium/README.md](../../data/compendium/README.md) for the full build pipeline.
