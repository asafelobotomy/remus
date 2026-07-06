# Run the Phase 1 Compendium SQL

This directory contains executable SQLite migration and seed scripts for the
Phase 1 canonical compendium database.

## Create a fresh compendium database

**Recommended (first use):** build a complete offline catalog in one step:

```bash
bash scripts/init_compendium.sh
```

This creates the schema (if needed), runs the full DAT ingest and enrichment pipeline, and materializes artwork blobs for offline runtime.

**Schema only** (empty — cannot match ROMs until you build):

```bash
bash scripts/setup_compendium_db.sh
```

Manual steps:

```bash
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0001_phase1_canonical_schema.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0002_patch_catalog.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/seeds/0001_regions.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/seeds/0002_systems.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/seeds/0003_merge_policy.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0003_systems_libretro_name.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0004_fts5_search_index.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0005_game_external_ids.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0006_game_achievement_count.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/migrations/0007_disc_sets.sql
```

## Validate phase 1 constraints and collisions

```bash
# Serialized quick gate (~1 min) — preferred for agent/CI
./scripts/validate_compendium_quick.sh

# Or directly:
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db
```

Or manually:

```bash
sqlite3 -header -column data/compendium/remus_compendium.db < data/compendium/validation/0001_phase1_checks.sql
```

`scripts/build_compendium_full.sh` runs validation gates automatically after a successful build (via `run_compendium_job.sh`).

Phase 2 quality thresholds (informational, do not block phase 1):

```bash
./scripts/validate_compendium_quick.sh          # applies migrations 0008–0012, runs 0002 (~1 min)
./scripts/validate_compendium_extended.sh       # applies migrations 0008–0012, runs 0003 (~30s)
```

Full builds also run strict gate `0006_enabled_source_gate.sql` (enabled sources with zero items).

Disc set ingest checks (populated databases; WARN checks are informational unless `--strict`):

```bash
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db \
  data/compendium/validation/0005_disc_set_ingest_checks.sql --warn-only
```

Backfill disc topology on an existing database (applies migration 0007 when missing):

```bash
bash scripts/backfill_disc_sets.sh data/compendium/remus_compendium.db
# Force rebuild when topology already exists:
bash scripts/backfill_disc_sets.sh data/compendium/remus_compendium.db --force
```

External data source reference: [docs/reports/COMPENDIUM-DATA-SOURCES.md](../../docs/reports/COMPENDIUM-DATA-SOURCES.md)

## Verify core seed counts

```bash
sqlite3 data/compendium/remus_compendium.db "SELECT COUNT(*) AS systems_count FROM systems;"
sqlite3 data/compendium/remus_compendium.db "SELECT COUNT(*) AS regions_count FROM regions;"
sqlite3 data/compendium/remus_compendium.db "SELECT COUNT(*) AS merge_policy_count FROM merge_policy;"
```

Expected values:

- systems_count: `112`
- regions_count: `21`
- merge_policy_count: `25`

A fresh bootstrap database contains schema, seeds, and indexes only. The full
validator is intended for a populated build output, so `content.*` checks will
remain `FAIL` until you run a compendium build or ingest workflow.

Bootstrap-only CI validation (schema + seeds, no populated content):

```bash
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db \
  data/compendium/validation/0000_bootstrap_checks.sql
```

## Build and inspect compendium catalogs

This repository currently exposes compendium rebuilding as a manifest-driven
developer workflow. Bundled runtime builds consume the resulting
`remus_compendium.db` automatically when it is present.

### Full pipeline (recommended)

For a complete catalogue refresh (DAT sync, manifest generation, build, patch
import, validation, and coverage report):

```bash
bash scripts/build_compendium_full.sh
```

Steps performed by `build_compendium_full.sh`:

1. `scripts/update_compendium_offline_sources.sh` — sync DATs/metadata (via `update_dats.sh`), LaunchBox metadata, libretro-thumbnails (skip with `--skip-update`)
2. `scripts/generate_compendium_manifest.sh` — write `compendium-manifest-full.json`
3. `remus-cli --build-compendium` — ingest DATs, run enrichment passes, merge, FTS index
4. Standalone artwork consolidate (unless `--skip-consolidate`)
5. `scripts/import_patch_catalog.sh` — import libretro hack/translation DATs into the patch catalog tables
6. `scripts/apply_compendium_migrations.sh` — idempotent migrations 0001–0012 (ledger in `schema_migrations`; manifest in `data/compendium/migrations/manifest.json`)
7. `--coverage-report` / `--disc-set-coverage` — per-source and per-system TSV summaries
8. `scripts/validate_compendium_tier.sh ci` — schema and content validation gates (skip with `--skip-validation`)
9. `scripts/validate_compendium_tier.sh artwork` — warn-only `0013_artwork_coverage.sql` after consolidate

**Build flags:** `--skip-update`, `--skip-validation` (ci tier only), `--skip-migrations`, `--strict-offline` (Tier A `offline-mirrors.json` preflight + artwork manifest gate), `--force-enrichment`, `--recover`, `--allow-patch-skip`, `--skip-consolidate`, `--offline-only`, `--force-full-rebuild`.

`--force-enrichment` bypasses the **build-level** enrichment-input fingerprint skip only (so enrichment passes run even when offline mirror checksums are unchanged). Per-pass `no_gaps` predicates and per-game skip sets (`loadGamesWithMinSourceFieldFacts`, LaunchBox `no_match` facts) still apply unless a source has no remaining gap work. Use `--enrich-source <name>` with `--force-enrichment` for targeted re-runs. `--skip-fts` skips the post-enrichment FTS rebuild when no canonical title fields changed.

**Detached runner:** `scripts/run_compendium_full_build_detached.sh` — holds the per-DB flock until the build exits; log defaults to `$REMUS_COMPENDIUM_BUILD_LOG` or `${TMPDIR:-/tmp}/remus_compendium_full_build.log`.

**Progress JSON:** During builds, `remus_compendium.db.progress.json` tracks `build_phase`, `overall_pct`, and enrichment pass detail. Wrapper phases include `dat_update`, `manifest`, `credentials`, `artwork`, `validate`, and `complete`. The GUI Compendium Wizard polls this file.

**Packaged builds:** Linux AppImages bundle `usr/share/remus/scripts/` and `usr/share/remus/data/compendium/`; the launcher sets `REMUS_DATA_DIR`. Writable output DB defaults to the user data directory when the bundled path is read-only.

**Credentials:** Compendium build enrichment uses `enrichment-credentials.json` and `REMUS_*` env vars. SteamGridDB (`REMUS_SGDB_API_KEY`) is runtime artwork only and is not written to enrichment-credentials.json.

Patch catalog import runs **after** the compendium build because it writes to
`patch_catalog_sources` / `patch_catalog_entries` in the populated database.
Hack DATs are fetched by `update_dats.sh` into `data/patches/hacks/`.

### Manual build

```bash
# Rebuild a compendium database from a manifest
remus-cli --build-compendium \
  --compendium-manifest /path/to/manifest.json \
  --compendium-output data/compendium/remus_compendium.db

# Import patch catalog separately (also done automatically by build_compendium_full.sh)
bash scripts/import_patch_catalog.sh data/compendium/remus_compendium.db

# Inspect patch catalog coverage directly
sqlite3 data/compendium/remus_compendium.db "SELECT system_name, catalog_name, entry_count FROM patch_catalog_sources ORDER BY system_name;"
```

### Incremental build modes

`--build-compendium` compares each enabled DAT source's `checksum_sha256` against the
latest row in `source_snapshots` (daily manifest `build_id` churn is ignored). Depending
on what changed, it chooses one of:

| Mode | When | Work performed |
|------|------|----------------|
| **Skip** | All checksums match, enrichment fingerprint matches | No-op (missing sidecar report does not trigger rebuild) |
| **Enrichment-only** | Checksums match, enrichment inputs changed | Re-run enrichment passes + FTS on existing DB |
| **Incremental ingest** | One or more DAT checksums changed | Purge/re-ingest changed sources only, then dedup/merge/enrichment |
| **Full** | No DB, schema mismatch, or `--force-full-rebuild` | Drop schema, ingest all DATs |

Use `--force-full-rebuild` to bypass incremental planning and always drop schema + re-ingest
every enabled DAT.

For offline Hasheous enrichment during builds, download platform dumps once:

```bash
scripts/update_hasheous_dumps.sh --all-core   # curated platforms (~25 ZIPs from hasheous.org API)
# scripts/update_hasheous_dumps.sh --all      # all 111 platform dumps (large)
```

Dumps live under `data/hasheous/dumps/` (gitignored; see `data/hasheous/dumps/README.md`).
Unit tests use `tests/fixtures/hasheous_offline/` only.

LaunchBox bulk enrichment (local XML, not redistributed):

```bash
scripts/update_launchbox_metadata.sh --source /path/to/Metadata.xml
build/remus-cli --enrich-compendium --enrich-source launchbox
```

Supplemental DAT sets (homebrew, libretro-dats, optional TOSEC drop-in) sync to
`data/databases/supplemental/` via `update_dats.sh` and appear in the full manifest when
regenerated with `scripts/generate_compendium_manifest.sh`.

## Files

- Migration: [data/compendium/migrations/0001_phase1_canonical_schema.sql](migrations/0001_phase1_canonical_schema.sql)
- Patch catalog migration: [data/compendium/migrations/0002_patch_catalog.sql](migrations/0002_patch_catalog.sql)
- Regions seed: [data/compendium/seeds/0001_regions.sql](seeds/0001_regions.sql)
- Systems seed: [data/compendium/seeds/0002_systems.sql](seeds/0002_systems.sql)
- Merge policy seed: [data/compendium/seeds/0003_merge_policy.sql](seeds/0003_merge_policy.sql)
- Libretro name migration: [data/compendium/migrations/0003_systems_libretro_name.sql](migrations/0003_systems_libretro_name.sql)
- FTS migration: [data/compendium/migrations/0004_fts5_search_index.sql](migrations/0004_fts5_search_index.sql)
- External ID columns: [data/compendium/migrations/0005_game_external_ids.sql](migrations/0005_game_external_ids.sql)
- Achievement count column: [data/compendium/migrations/0006_game_achievement_count.sql](migrations/0006_game_achievement_count.sql)
- Disc sets + tracks: [data/compendium/migrations/0007_disc_sets.sql](migrations/0007_disc_sets.sql)
- Game facts lookup index: [data/compendium/migrations/0008_game_facts_lookup_index.sql](migrations/0008_game_facts_lookup_index.sql)
- Signature source-entry bridge index: [data/compendium/migrations/0009_game_signatures_source_entry_key.sql](migrations/0009_game_signatures_source_entry_key.sql)
- Extended metadata columns: [data/compendium/migrations/0010_game_extended_metadata.sql](migrations/0010_game_extended_metadata.sql)
- Materialized coverage snapshot: [data/compendium/migrations/0011_materialized_coverage.sql](migrations/0011_materialized_coverage.sql)
- Validator: [data/compendium/validation/0001_phase1_checks.sql](validation/0001_phase1_checks.sql)
- Disc set validator: [data/compendium/validation/0004_disc_set_checks.sql](validation/0004_disc_set_checks.sql)
- Disc set ingest validator: [data/compendium/validation/0005_disc_set_ingest_checks.sql](validation/0005_disc_set_ingest_checks.sql)
- Backfill script: [scripts/backfill_disc_sets.sh](../../scripts/backfill_disc_sets.sh)
- Runner script: [scripts/setup_compendium_db.sh](../../scripts/setup_compendium_db.sh)
