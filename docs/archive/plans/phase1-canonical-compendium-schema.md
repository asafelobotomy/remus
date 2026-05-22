# Define the Phase 1 Canonical Compendium Schema

## State the Outcome

Phase 1 produces a deterministic SQLite compendium that supports:

- Offline identity matching by hash and serial.
- Canonical game records by system and region.
- Provenance-aware metadata merge with reproducible builds.
- Migration-safe alignment with current Remus models in:
  - [src/core/database_schema.cpp](../../src/core/database_schema.cpp)
  - [src/core/database_types.h](../../src/core/database_types.h)
  - [src/metadata/metadata_provider.h](../../src/metadata/metadata_provider.h)
  - [src/metadata/local_database_provider.h](../../src/metadata/local_database_provider.h)
  - [src/core/constants/systems.h](../../src/core/constants/systems.h)

## Set Phase 1 Boundaries

Phase 1 includes:

- Systems, regions, games, aliases, signatures, serials.
- Field-level facts with source provenance and confidence.
- Canonical value materialization from merge rules.
- Build manifest metadata for reproducibility.

Phase 1 excludes:

- Artwork binary storage.
- Remote API live caching.
- User library tables (files, matches, undo queue).

## Confirm Canonical Field Coverage

### Systems Coverage

Required system fields:

- Stable ID and slug.
- Display name and manufacturer.
- Generation and release year.
- Preferred hash algorithm.
- System traits needed for matching behavior.

### Regions Coverage

Required region fields:

- Region code and display name.
- Group code for normalized rollups.
- Optional parent code for hierarchy.

### Games Coverage

Required game fields:

- Stable game ID.
- Canonical title.
- System reference.
- Primary region reference.
- Release date and release year.
- Developer and publisher.
- Genre and players.
- Description and rating.

### Identity Coverage

Required identity fields:

- Hash signatures: type, value, source, confidence.
- Serials: normalized value, source, confidence.
- Name aliases with alias type.
- Source-native keys for traceability.

### Provenance Coverage

Required provenance fields:

- Source ID and source priority.
- Source license and source snapshot/version.
- Per-fact source and extraction time.
- Build ID and build timestamps.

## Define Normalization Rules

- Store all hash values uppercase without spaces.
- Store serial values uppercase and trimmed.
- Store region codes uppercase.
- Store release_date as ISO 8601 `YYYY-MM-DD` when known.
- Store release_year as integer when day/month are unknown.
- Keep source raw keys in `source_items.external_key`.

## Specify the Exact SQLite Schema

```sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS compendium_builds (
    build_id TEXT PRIMARY KEY,                     -- UUID or content hash ID
    schema_version INTEGER NOT NULL,
    built_at TEXT NOT NULL,                        -- ISO 8601 timestamp
    source_manifest_json TEXT NOT NULL,            -- exact source snapshots used
    notes TEXT
);

CREATE TABLE IF NOT EXISTS sources (
    source_id TEXT PRIMARY KEY,                    -- e.g. nointro, redump, libretro_meta
    display_name TEXT NOT NULL,
    source_type TEXT NOT NULL,                     -- dat, xml, json, api-export
    license_id TEXT,
    license_url TEXT,
    attribution_required INTEGER NOT NULL DEFAULT 0,
    priority INTEGER NOT NULL,                     -- higher wins in tie-break stage 1
    enabled INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS source_snapshots (
    snapshot_id TEXT PRIMARY KEY,                  -- stable snapshot key
    source_id TEXT NOT NULL,
    snapshot_label TEXT NOT NULL,                  -- version/date/commit
    snapshot_ref TEXT,                             -- commit SHA or URL
    fetched_at TEXT,
    checksum_sha256 TEXT,
    FOREIGN KEY (source_id) REFERENCES sources(source_id)
);

CREATE TABLE IF NOT EXISTS systems (
    system_id INTEGER PRIMARY KEY,                 -- align with Constants::Systems IDs
    internal_name TEXT NOT NULL UNIQUE,            -- NES, PSX, etc.
    display_name TEXT NOT NULL,
    manufacturer TEXT,
    generation INTEGER,
    release_year INTEGER,
    preferred_hash TEXT NOT NULL,                  -- CRC32, MD5, SHA1
    is_disc_based INTEGER NOT NULL DEFAULT 0,
    is_handheld INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS regions (
    region_code TEXT PRIMARY KEY,                  -- USA, EUR, JPN, WORLD
    display_name TEXT NOT NULL,
    group_code TEXT NOT NULL,                      -- americas, europe, asia, global
    parent_region_code TEXT,
    FOREIGN KEY (parent_region_code) REFERENCES regions(region_code)
);

CREATE TABLE IF NOT EXISTS system_regions (
    system_id INTEGER NOT NULL,
    region_code TEXT NOT NULL,
    PRIMARY KEY (system_id, region_code),
    FOREIGN KEY (system_id) REFERENCES systems(system_id),
    FOREIGN KEY (region_code) REFERENCES regions(region_code)
);

CREATE TABLE IF NOT EXISTS games (
    game_id TEXT PRIMARY KEY,                      -- canonical stable ID
    system_id INTEGER NOT NULL,
    canonical_title TEXT NOT NULL,
    primary_region_code TEXT,
    release_date TEXT,                             -- YYYY-MM-DD when known
    release_year INTEGER,
    developer TEXT,
    publisher TEXT,
    genre TEXT,
    players_max INTEGER,
    description TEXT,
    rating REAL,
    canonical_confidence REAL NOT NULL DEFAULT 0, -- 0.0..1.0 for canonical record quality
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (system_id) REFERENCES systems(system_id),
    FOREIGN KEY (primary_region_code) REFERENCES regions(region_code)
);

CREATE TABLE IF NOT EXISTS game_names (
    game_name_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    name_text TEXT NOT NULL,
    alias_type TEXT NOT NULL,                      -- canonical, dat_name, alt_name, normalized
    locale TEXT,
    source_id TEXT,
    snapshot_id TEXT,
    confidence REAL NOT NULL DEFAULT 0,
    UNIQUE (game_id, name_text, alias_type, COALESCE(locale, '')),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (snapshot_id) REFERENCES source_snapshots(snapshot_id)
);

CREATE TABLE IF NOT EXISTS game_signatures (
    signature_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    hash_type TEXT NOT NULL,                       -- CRC32, MD5, SHA1
    hash_value TEXT NOT NULL,                      -- normalized uppercase
    source_id TEXT NOT NULL,
    snapshot_id TEXT,
    source_entry_key TEXT,                         -- source-native game/rom identifier
    confidence REAL NOT NULL,                      -- 0.0..1.0
    is_primary INTEGER NOT NULL DEFAULT 0,
    UNIQUE (hash_type, hash_value),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (snapshot_id) REFERENCES source_snapshots(snapshot_id)
);

CREATE TABLE IF NOT EXISTS game_serials (
    serial_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    serial_value TEXT NOT NULL,                    -- normalized uppercase
    source_id TEXT NOT NULL,
    snapshot_id TEXT,
    source_entry_key TEXT,
    confidence REAL NOT NULL,
    UNIQUE (serial_value, game_id),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (snapshot_id) REFERENCES source_snapshots(snapshot_id)
);

CREATE TABLE IF NOT EXISTS source_items (
    source_item_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id TEXT NOT NULL,
    snapshot_id TEXT,
    external_key TEXT NOT NULL,                    -- key from source file/API
    system_hint TEXT,
    title_raw TEXT,
    region_raw TEXT,
    payload_json TEXT,
    extracted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (source_id, COALESCE(snapshot_id, ''), external_key),
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (snapshot_id) REFERENCES source_snapshots(snapshot_id)
);

CREATE TABLE IF NOT EXISTS game_facts (
    fact_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    field_name TEXT NOT NULL,                      -- title, release_date, publisher, etc.
    field_value TEXT NOT NULL,
    value_type TEXT NOT NULL,                      -- text, int, real, date, json
    source_id TEXT NOT NULL,
    snapshot_id TEXT,
    source_item_id INTEGER,
    source_priority INTEGER NOT NULL,
    confidence REAL NOT NULL,
    observed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (game_id, field_name, field_value, source_id, COALESCE(snapshot_id, '')),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (snapshot_id) REFERENCES source_snapshots(snapshot_id),
    FOREIGN KEY (source_item_id) REFERENCES source_items(source_item_id)
);

CREATE TABLE IF NOT EXISTS canonical_resolution (
    game_id TEXT NOT NULL,
    field_name TEXT NOT NULL,
    selected_fact_id INTEGER NOT NULL,
    resolved_by_rule TEXT NOT NULL,                -- rule key from merge_policy
    resolved_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (game_id, field_name),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (selected_fact_id) REFERENCES game_facts(fact_id)
);

CREATE TABLE IF NOT EXISTS merge_policy (
    policy_id INTEGER PRIMARY KEY AUTOINCREMENT,
    field_name TEXT NOT NULL,
    rule_order INTEGER NOT NULL,
    rule_key TEXT NOT NULL,
    rule_description TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1,
    UNIQUE (field_name, rule_order)
);

CREATE TABLE IF NOT EXISTS merge_conflicts (
    conflict_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    field_name TEXT NOT NULL,
    fact_ids_json TEXT NOT NULL,
    resolution_status TEXT NOT NULL DEFAULT 'unresolved',  -- unresolved, resolved, ignored
    chosen_fact_id INTEGER,
    notes TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    resolved_at TEXT,
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (chosen_fact_id) REFERENCES game_facts(fact_id)
);

CREATE INDEX IF NOT EXISTS idx_game_signatures_lookup
    ON game_signatures(hash_type, hash_value);

CREATE INDEX IF NOT EXISTS idx_game_serials_lookup
    ON game_serials(serial_value);

CREATE INDEX IF NOT EXISTS idx_game_names_lookup
    ON game_names(name_text, alias_type);

CREATE INDEX IF NOT EXISTS idx_game_facts_field
    ON game_facts(game_id, field_name, source_priority DESC, confidence DESC);

CREATE INDEX IF NOT EXISTS idx_source_items_source
    ON source_items(source_id, external_key);

CREATE INDEX IF NOT EXISTS idx_games_system_region
    ON games(system_id, primary_region_code);
```

## Define the Merge Policy Table for Phase 1

Insert these baseline rules into `merge_policy`.

| field_name | rule_order | rule_key | rule_description |
| --- | ---: | --- | --- |
| canonical_title | 1 | exact_hash_source_priority | If title comes from an exact hash-identified record, pick highest source priority. |
| canonical_title | 2 | normalized_name_similarity | If no exact hash title, pick highest normalized similarity to canonical alias set. |
| canonical_title | 3 | shortest_stable_title | Tie-break with shortest non-empty stable title variant. |
| primary_region_code | 1 | explicit_region_codes | Prefer explicit normalized region codes from source over parsed text tokens. |
| primary_region_code | 2 | region_token_parse | Use parsed region token from title only when explicit region is absent. |
| release_date | 1 | full_date_preferred | Prefer full `YYYY-MM-DD` over year-only values. |
| release_date | 2 | higher_priority_source | Tie-break using source priority. |
| release_date | 3 | newer_snapshot | If still tied, prefer newest snapshot. |
| release_year | 1 | derive_from_release_date | If canonical release_date exists, derive release_year from it. |
| release_year | 2 | max_confidence_year | Else pick highest-confidence year fact. |
| developer | 1 | exact_hash_source_priority | Prefer developer from highest-priority source for exact-hash matched record. |
| developer | 2 | most_frequent_value | Tie-break with most frequent normalized developer value. |
| publisher | 1 | exact_hash_source_priority | Prefer publisher from highest-priority source for exact-hash matched record. |
| publisher | 2 | most_frequent_value | Tie-break with most frequent normalized publisher value. |
| genre | 1 | normalized_taxonomy_match | Prefer genre value mapped to canonical taxonomy vocabulary. |
| genre | 2 | higher_priority_source | Tie-break by source priority. |
| players_max | 1 | numeric_valid_range | Accept only numeric values in valid range 1..16 before ranking. |
| players_max | 2 | highest_confidence | Prefer highest-confidence valid value. |
| description | 1 | longest_non_boilerplate | Prefer longest non-boilerplate text after normalization. |
| description | 2 | higher_priority_source | Tie-break by source priority. |
| rating | 1 | normalized_rating_scale | Convert source scales to 0..10 then compare confidence and source priority. |

Recommended default source priorities for Phase 1:

1. `nointro` / `redump`: `100`
2. `libretro_metadata`: `90`
3. `gametdb`: `80`
4. `hasheous`: `70`
5. Other optional sources: `<70`

## Align with Current Runtime Models

- Keep `systems.system_id` aligned with constants in [src/core/constants/systems.h](../../src/core/constants/systems.h).
- Keep canonical game fields compatible with `GameMetadata` in [src/metadata/metadata_provider.h](../../src/metadata/metadata_provider.h).
- Keep hash and serial lookup paths compatible with local provider indexes in [src/metadata/local_database_provider.h](../../src/metadata/local_database_provider.h) and [src/metadata/local_database_provider.cpp](../../src/metadata/local_database_provider.cpp).
- Keep release year/date enrichment compatible with [src/metadata/libretro_metadata_parser.cpp](../../src/metadata/libretro_metadata_parser.cpp).

## Validate Before Implementation

- Validate uniqueness on `(hash_type, hash_value)` using full corpus imports.
- Validate serial collision rate across systems.
- Validate region normalization against current system region codes.
- Validate merge policy determinism by running two identical builds and comparing canonical outputs.
- Validate that unresolved conflicts are emitted to `merge_conflicts` instead of silently dropped.
