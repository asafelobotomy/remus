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
sqlite3 data/compendium/remus_compendium.db < data/compendium/seeds/0001_regions.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/seeds/0002_systems.sql
sqlite3 data/compendium/remus_compendium.db < data/compendium/seeds/0003_merge_policy.sql
```

## Validate phase 1 constraints and collisions

```bash
sqlite3 -header -column data/compendium/remus_compendium.db < data/compendium/validation/0001_phase1_checks.sql
```

## Verify core seed counts

```bash
sqlite3 data/compendium/remus_compendium.db "SELECT COUNT(*) AS systems_count FROM systems;"
sqlite3 data/compendium/remus_compendium.db "SELECT COUNT(*) AS regions_count FROM regions;"
sqlite3 data/compendium/remus_compendium.db "SELECT COUNT(*) AS merge_policy_count FROM merge_policy;"
```

Expected values:

- systems_count: `42`
- regions_count: `6`
- merge_policy_count: `21`

## Build and inspect compendium catalogs

This repository currently exposes compendium rebuilding as a manifest-driven
developer workflow. Bundled runtime builds consume the resulting
`remus_compendium.db` automatically when it is present.

```bash
# Rebuild a compendium database from a manifest
remus-cli --build-compendium \
  --compendium-manifest /path/to/manifest.json \
  --compendium-output data/compendium/remus_compendium.db

# Inspect patch catalog coverage directly
sqlite3 data/compendium/remus_compendium.db "SELECT system_name, catalog_name, entry_count FROM patch_catalog_sources ORDER BY system_name;"
```

## Files

- Migration: [data/compendium/migrations/0001_phase1_canonical_schema.sql](migrations/0001_phase1_canonical_schema.sql)
- Regions seed: [data/compendium/seeds/0001_regions.sql](seeds/0001_regions.sql)
- Systems seed: [data/compendium/seeds/0002_systems.sql](seeds/0002_systems.sql)
- Merge policy seed: [data/compendium/seeds/0003_merge_policy.sql](seeds/0003_merge_policy.sql)
- Validator: [data/compendium/validation/0001_phase1_checks.sql](validation/0001_phase1_checks.sql)
- Runner script: [scripts/setup_compendium_db.sh](../../scripts/setup_compendium_db.sh)
