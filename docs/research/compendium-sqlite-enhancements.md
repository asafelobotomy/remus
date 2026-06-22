# Compendium SQLite Enhancements

Remus keeps SQLite as the canonical store for `remus_compendium.db` (~180k games, ~493k signatures) and the user library (`remus.db`). These changes address the real bottlenecks at current scale: **write lock contention**, **operational job serialization**, and **repeated heavy coverage aggregation** — not query-engine throughput.

## What changed

### Shared pragma helpers (`src/core/compendium_sql_pragmas.h`)

| Path | Pragmas |
|------|---------|
| Write (`applyWritePragmas`) | `journal_mode=WAL`, `busy_timeout=30000`, `foreign_keys=ON` |
| Read-only (`applyReadOnlyPragmas`) | `busy_timeout=60000`, `query_only=ON` |
| Post-build finalize (`finalizeDatabasePragmas`) | `synchronous=NORMAL`, `wal_checkpoint(TRUNCATE)` |

CLI wrappers live in `src/cli/compendium_sql_utilities.h`.

Write batches use `BEGIN IMMEDIATE` (`beginImmediateTransaction`) so a writer acquires the reserved lock up front instead of failing late under contention.

### Materialized coverage snapshot (migration 0011)

Tables:

- `compendium_coverage_stats` — single summary row (`id = 1`)
- `compendium_source_coverage` — per-source metrics

Refreshed by `populateCompendiumCoverageSnapshot()` at the end of:

- `--build-compendium`
- `--enrich-compendium` (when FTS is rebuilt)
- `--ingest-compendium` (when FTS is rebuilt)

`remus-cli --coverage-report` reads these tables when present; otherwise it falls back to the original live CTE queries.

### Wired entry points

| Command / module | Write pragmas | BEGIN IMMEDIATE | Read-only pragmas | Finalize |
|------------------|---------------|-----------------|-------------------|----------|
| `--build-compendium` | build pragmas (bulk) | yes | — | yes |
| `--enrich-compendium` | yes | — | — | yes |
| `--ingest-compendium` | yes | yes | — | yes |
| `--dedup-compendium` | yes | yes | — | — |
| `--import-patch-catalog` | yes | — | — | — |
| `--coverage-report` | — | — | yes | — |
| `CompendiumDiscBridge` (read) | — | — | yes | — |
| Verification storage attach | — | — | yes | — |

`CompendiumProvider` intentionally stays read/write (may create FTS) and does **not** set `query_only`.

### Scripts and migration version

- `scripts/setup_compendium_db.sh` and `scripts/apply_compendium_migrations.sh` include `0011_materialized_coverage.sql`.
- `Constants::DatabaseSchema::Compendium::MIGRATION_VERSION` is **11**.

Existing databases: run `scripts/apply_compendium_migrations.sh <path-to-compendium.db>`, then rebuild or run a command that calls `finalizeCompendiumBuildArtifacts` to populate the snapshot.

## Operational notes

- **Job serialization** remains important: `scripts/run_compendium_job.sh` uses `flock` so only one compendium writer runs at a time.
- **busy_timeout** (30s write / 60s read) reduces transient `SQLITE_BUSY` during overlapping read/write; it does not replace flock for long-running builds.
- **WAL checkpoint at build end** keeps `-wal`/`-shm` sidecar files bounded after bulk loads that use `synchronous=OFF`.
- **Coverage snapshot staleness**: dedup, patch import, and manual SQL edits do not refresh the snapshot until the next build/enrich/ingest finalize. `--coverage-report` live fallback handles databases that predate migration 0011 or lack a snapshot row.

## Constants reference

```cpp
// src/core/constants/database_schema.h (Compendium namespace)
MIGRATION_VERSION = 11
BUSY_TIMEOUT_WRITE_MS = 30000
BUSY_TIMEOUT_READ_MS = 60000
```

## Future work (optional)

- Rewrite correlated subqueries in `0003_phase2_extended_checks.sql` validation for faster offline checks.
- Refresh coverage snapshot after dedup if dedup becomes a common standalone operation.

For analytics-sidecar options (DuckDB, Parquet history), see [compendium-duckdb-future-reference.md](compendium-duckdb-future-reference.md).
