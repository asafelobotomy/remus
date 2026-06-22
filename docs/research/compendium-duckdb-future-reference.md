# DuckDB Sidecar — Future Reference

**Status:** deferred. Remus continues to use SQLite as the single canonical compendium store. See [compendium-sqlite-enhancements.md](compendium-sqlite-enhancements.md) for what was implemented instead.

This document preserves research conclusions so a future sidecar can be evaluated without re-litigating the decision.

## When DuckDB might be worth revisiting

| Scenario | DuckDB fit |
|----------|------------|
| Federated analytics across compendium + user library + verification logs | Strong — `ATTACH` multiple SQLite files, columnar execution |
| Historical coverage/build metrics in Parquet or CSV archives | Strong — native Parquet, window functions, cheap aggregations |
| Ad-hoc research queries on attached `remus_compendium.db` at current size (~500k rows) | Marginal — SQLite + materialized coverage tables is sufficient |
| Replacing SQLite as the write path for compendium builds | Poor — duplicate consistency model, no WAL sharing, extra operational surface |

## Why SQLite enhancement was chosen (2026)

1. **Pain was contention and ops**, not OLAP latency on half-million-row tables.
2. **Materialized coverage** removes the heaviest repeated read pattern without a second engine.
3. **WAL + busy_timeout + BEGIN IMMEDIATE + post-build checkpoint** address the observed build/verify overlap failures.
4. A DuckDB sidecar adds build pipeline steps, artifact versioning, and drift risk between canonical SQLite and derived analytics DB.

## Sketch: optional sidecar layout

If needed later, treat DuckDB as a **derived, rebuildable** artifact — never the source of truth.

```
data/compendium/
  remus_compendium.db          # canonical (SQLite)
  analytics/
    coverage_history.parquet   # append-only build snapshots
    compendium_analytics.duckdb  # optional; rebuilt from SQLite + Parquet
```

Example bootstrap (illustrative, not shipped):

```sql
INSTALL sqlite;
LOAD sqlite;
ATTACH 'remus_compendium.db' AS compendium (TYPE SQLITE, READ_ONLY);
SELECT * FROM compendium.compendium_source_coverage;
```

For time-series coverage, export `compendium_coverage_stats` + `compendium_source_coverage` to Parquet after each build and query with DuckDB or Polars.

## Alternatives to evaluate alongside DuckDB

- **SQLite-only:** more materialized tables, covering indexes, incremental snapshot refresh triggers.
- **Polars / pandas:** one-off scripts over exported Parquet/CSV for research notebooks.
- **ClickHouse / Postgres:** only if compendium moves to a shared multi-user server (out of scope for offline-first Remus).

## References

- DuckDB SQLite extension: <https://duckdb.org/docs/extensions/sqlite>
- Prior Remus research: compendium build deep research, multi-disc SHA256 research (under `docs/reports/`)
