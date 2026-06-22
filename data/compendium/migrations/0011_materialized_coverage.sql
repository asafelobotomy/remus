PRAGMA foreign_keys = ON;
-- Materialized post-build coverage snapshot. Refreshed after each compendium build,
-- enrichment, or ingest that rebuilds FTS. Read by remus-cli --coverage-report.
CREATE TABLE IF NOT EXISTS compendium_coverage_stats (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    built_at TEXT NOT NULL,
    total_games INTEGER NOT NULL,
    total_signatures INTEGER NOT NULL,
    total_systems INTEGER NOT NULL,
    active_sources INTEGER NOT NULL,
    shadowed_sources INTEGER NOT NULL,
    disc_based_games INTEGER NOT NULL,
    games_with_disc_sets INTEGER NOT NULL,
    disc_set_coverage_pct REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS compendium_source_coverage (
    source_id TEXT PRIMARY KEY,
    enabled INTEGER NOT NULL,
    priority INTEGER NOT NULL,
    source_items INTEGER NOT NULL,
    sigs_owned INTEGER NOT NULL,
    games_covered INTEGER NOT NULL,
    coverage_pct REAL NOT NULL,
    sig_yield_pct REAL NOT NULL,
    shadowed INTEGER NOT NULL,
    FOREIGN KEY (source_id) REFERENCES sources(source_id)
);
CREATE INDEX IF NOT EXISTS idx_compendium_source_coverage_rank ON compendium_source_coverage(
    shadowed DESC,
    sig_yield_pct ASC,
    coverage_pct ASC
);
