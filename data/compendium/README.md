# Run the Phase 1 Compendium SQL

This directory contains executable SQLite migration and seed scripts for the
Phase 1 canonical compendium database.

## Create a fresh compendium database

One command:

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
```

## Validate phase 1 constraints and collisions

```bash
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db
```

Or manually:

```bash
sqlite3 -header -column data/compendium/remus_compendium.db < data/compendium/validation/0001_phase1_checks.sql
```

`scripts/build_compendium_full.sh` runs this gate automatically after a successful build.

Phase 2 quality thresholds (informational, do not block phase 1):

```bash
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db \
  data/compendium/validation/0002_phase2_quality_checks.sql
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
- merge_policy_count: `21`

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

1. `scripts/update_dats.sh --all` — sync libretro, No-Intro, Redump, MAME, metadata, GameTDB, OpenVGDB
2. `scripts/generate_compendium_manifest.sh` — write `compendium-manifest-full.json`
3. `remus-cli --build-compendium` — ingest DATs, run enrichment passes, merge, FTS index
4. `scripts/import_patch_catalog.sh` — import libretro hack/translation DATs into the patch catalog tables
5. `.github/scripts/validate-compendium-db.sh` — schema and content validation gates
6. `--coverage-report` — per-source TSV coverage summary

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

## Files

- Migration: [data/compendium/migrations/0001_phase1_canonical_schema.sql](migrations/0001_phase1_canonical_schema.sql)
- Patch catalog migration: [data/compendium/migrations/0002_patch_catalog.sql](migrations/0002_patch_catalog.sql)
- Regions seed: [data/compendium/seeds/0001_regions.sql](seeds/0001_regions.sql)
- Systems seed: [data/compendium/seeds/0002_systems.sql](seeds/0002_systems.sql)
- Merge policy seed: [data/compendium/seeds/0003_merge_policy.sql](seeds/0003_merge_policy.sql)
- Libretro name migration: [data/compendium/migrations/0003_systems_libretro_name.sql](migrations/0003_systems_libretro_name.sql)
- FTS migration: [data/compendium/migrations/0004_fts5_search_index.sql](migrations/0004_fts5_search_index.sql)
- External ID columns: [data/compendium/migrations/0005_game_external_ids.sql](migrations/0005_game_external_ids.sql)
- Validator: [data/compendium/validation/0001_phase1_checks.sql](validation/0001_phase1_checks.sql)
- Runner script: [scripts/setup_compendium_db.sh](../../scripts/setup_compendium_db.sh)
