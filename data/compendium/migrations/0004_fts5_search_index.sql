-- ── 0002 FTS5 full-text search index ──────────────────────────────────────────
--
-- Provides fast substring search over all game titles (canonical + aliases).
-- Populated as a post-build step by the build pipeline after enrichment.
--
-- Design notes:
--   Trigram tokenizer: indexes every 3-character substring, enabling both
--   MATCH 'term' queries (no wildcards needed) and LIKE '%term%' queries
--   via the same index automatically.
--   External payload columns (game_id, system_id, region_code) are declared
--   UNINDEXED: stored but not in the inverted index. Filterable with WHERE.
--   BM25 rank: ORDER BY rank returns best-matching titles first.
--   One row per title form per game: the canonical title and each alias are
--   separate rows. GROUP BY game_id deduplicates in the search query.
--   Rebuilt on each full compendium build. Never incrementally updated.
--
-- Requires SQLite >= 3.38.0 (trigram tokenizer). Qt 6 bundles SQLite >= 3.39.0.
-- FTS5 itself requires SQLite >= 3.9.0.
--
-- Minimum search term: 3 unicode characters (trigram requirement). Shorter
-- terms return zero rows from MATCH. Callers guard against this.

CREATE VIRTUAL TABLE IF NOT EXISTS games_search USING fts5(
    title,
    game_id UNINDEXED,
    system_id UNINDEXED,
    region_code UNINDEXED,
    tokenize = 'trigram'
);
