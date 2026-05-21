# ADR-0003 — Region is a match-row attribute, not a scan-row attribute

**Status**: Accepted  
**Date**: 2026-05

---

## Context

A file's region tag (e.g. `USA`, `Europe`, `Japan`) is encoded in its filename using No-Intro / Redump conventions. The question is where in the pipeline this derived value should be persisted so that later steps (export, bundle naming, match reports) have a stable, queryable source of truth rather than recomputing it on demand.

Three candidate points existed:

| Location | Upside | Downside |
| --- | --- | --- |
| **File row** (at scan time) | Available immediately after `--scan` | Requires a new `files.region` column; duplicates game metadata |
| **Match row / game record** (at match time) | Consistent with the game-metadata model; no extra column needed | Not available until `--match` has run |
| **Export time** (on the fly) | Zero DB changes | Not persisted; every consumer must repeat the fallback logic |

## Decision

Region is stored as part of the **game record** (i.e. the `games.region` column) and is populated at **match time** by `persistMetadata()`. If the metadata provider does not supply a region, `FilenameNormalizer::extractRegion()` is used as an in-pipeline fallback — the result is then written to `games.region` and available to all subsequent reads via `getAllMatches()` / `getMatchForFile()`.

The export-time fallback that re-ran `extractRegion()` on unmatched rows has been removed; only matched files are exported and their region is now reliably present in the database.

## Consequences

- `MatchResult.region` is populated from the DB for all exported rows — no runtime filename parsing at export time.
- Unmatched files (no game record) continue to have no persistent region, which is acceptable since they are excluded from export.
- If a future requirement needs region on unmatched files (e.g. a pre-match list view), a `files.region` column would be added at that point as a separate decision (ADR-000x).
