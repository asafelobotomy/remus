# Define the Phase 1 Compendium Compiler Contract

## State the Purpose

This contract defines the exact input and output behavior for the Phase 1
compiler that builds the canonical compendium SQLite database.

It complements the schema design in
[docs/plans/phase1-canonical-compendium-schema.md](phase1-canonical-compendium-schema.md).

## Define Input Contract

### Source Manifest

The compiler accepts one source manifest JSON file.

Required manifest fields:

- `build_id`: Unique build identifier.
- `schema_version`: Target schema version integer.
- `sources`: Array of source descriptors.

Each source descriptor includes:

- `source_id`: Stable key. Example: `nointro`, `redump`, `libretro_metadata`.
- `source_type`: `dat`, `xml`, `json`, or `api-export`.
- `snapshot_id`: Stable snapshot key.
- `snapshot_label`: Human-readable snapshot label.
- `snapshot_ref`: Commit SHA, release tag, or URL.
- `path`: Local input path.
- `checksum_sha256`: Optional snapshot checksum.
- `enabled`: Boolean.
- `priority`: Integer merge priority.

### Input Record Expectations

The compiler may receive records from different formats. Every parsed record
must normalize to this internal envelope before linking:

- `source_id`
- `snapshot_id`
- `external_key`
- `system_hint`
- `title_raw`
- `region_raw`
- `hashes`: `{ crc32?, md5?, sha1? }`
- `serials`: `[]`
- `fields`: map of candidate metadata fields
- `payload_json`: raw source payload for provenance

### Required Pre-Normalization Rules

- Hashes must be uppercase without spaces.
- Serials must be uppercase and trimmed.
- Region tokens must be uppercase and normalized to known codes.
- Empty strings become NULL-equivalent in compiler logic.

## Define Output Contract

### Primary Artifact

The compiler emits one SQLite file per build:

- `remus_compendium_<build_id>.db`

### Required Tables Populated

The compiler must populate, at minimum:

- `compendium_builds`
- `sources`
- `source_snapshots`
- `systems`
- `regions`
- `system_regions`
- `games`
- `game_names`
- `game_signatures`
- `game_serials`
- `source_items`
- `game_facts`
- `canonical_resolution`
- `merge_policy`
- `merge_conflicts`

### Build Manifest Row

One row must be inserted into `compendium_builds` with:

- exact schema version
- exact source manifest JSON used for the run
- build timestamp

## Define Deterministic Merge Semantics

For each `games.game_id + field_name`:

1. Collect candidate facts from `game_facts`.
2. Apply rules in `merge_policy` sorted by `rule_order`.
3. Materialize winning `fact_id` into `canonical_resolution`.
4. If no single winner emerges, write a row to `merge_conflicts`.

The compiler must never silently drop ambiguous candidates.

## Define CLI Contract

Phase 1 compiler command shape:

```bash
remus-cli --build-compendium \
  --manifest path/to/manifest.json \
  --output path/to/remus_compendium_<build_id>.db
```

Required exit behavior:

- `0`: Build completed with no unresolved blocking conflicts.
- `1`: Build failed due to schema/IO/parse errors.
- `2`: Build completed with unresolved conflicts above configured threshold.

## Define Validation Contract

### Must-Pass Checks

- SQLite schema exists and integrity check returns `ok`.
- Unique constraints hold for signatures and keys.
- `systems` count matches expected seeded systems for Phase 1.
- `merge_policy` contains active baseline rules.
- `canonical_resolution` has no duplicate `(game_id, field_name)`.

### Recommended Post-Build Assertions

- No orphan references from facts/resolution tables.
- Hash collision report generated for review.
- Serial collision report generated for review.
- Region normalization report generated for review.

## Define Artifact Metadata Contract

The compiler must emit a sidecar JSON report:

- `remus_compendium_<build_id>.report.json`

Required report fields:

- `build_id`
- `schema_version`
- `input_sources`
- `records_ingested`
- `games_created`
- `signatures_created`
- `serials_created`
- `facts_created`
- `resolved_fields`
- `unresolved_conflicts`
- `duration_ms`

## Define Backward-Compatibility Expectations

- Phase 1 output must be queryable by future runtime providers without schema edits.
- New fields in Phase 2+ must be additive.
- Existing Phase 1 columns must not be repurposed.

## Define Non-Goals for Phase 1

- No artwork binary ingestion.
- No live API fetch during build.
- No user library file mutation.

## Link to Implementation Inputs

The contract aligns with current model sources:

- [src/core/database_schema.cpp](../../src/core/database_schema.cpp)
- [src/core/database_types.h](../../src/core/database_types.h)
- [src/metadata/metadata_provider.h](../../src/metadata/metadata_provider.h)
- [src/metadata/local_database_provider.cpp](../../src/metadata/local_database_provider.cpp)
- [src/core/systems.cpp](../../src/core/systems.cpp)
