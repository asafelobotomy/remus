PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS compendium_builds (
    build_id TEXT PRIMARY KEY,
    schema_version INTEGER NOT NULL,
    built_at TEXT NOT NULL,
    source_manifest_json TEXT NOT NULL,
    notes TEXT
);

CREATE TABLE IF NOT EXISTS sources (
    source_id TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    source_type TEXT NOT NULL,
    license_id TEXT,
    license_url TEXT,
    attribution_required INTEGER NOT NULL DEFAULT 0,
    priority INTEGER NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS source_snapshots (
    snapshot_id TEXT PRIMARY KEY,
    source_id TEXT NOT NULL,
    snapshot_label TEXT NOT NULL,
    snapshot_ref TEXT,
    fetched_at TEXT,
    checksum_sha256 TEXT,
    FOREIGN KEY (source_id) REFERENCES sources(source_id)
);

CREATE TABLE IF NOT EXISTS systems (
    system_id INTEGER PRIMARY KEY,
    internal_name TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    manufacturer TEXT,
    generation INTEGER,
    release_year INTEGER,
    preferred_hash TEXT NOT NULL,
    is_disc_based INTEGER NOT NULL DEFAULT 0,
    is_handheld INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS regions (
    region_code TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    group_code TEXT NOT NULL,
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
    game_id TEXT PRIMARY KEY,
    system_id INTEGER NOT NULL,
    canonical_title TEXT NOT NULL,
    primary_region_code TEXT,
    release_date TEXT,
    release_year INTEGER,
    developer TEXT,
    publisher TEXT,
    genre TEXT,
    players_max INTEGER,
    description TEXT,
    rating REAL,
    canonical_confidence REAL NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (system_id) REFERENCES systems(system_id),
    FOREIGN KEY (primary_region_code) REFERENCES regions(region_code)
);

CREATE TABLE IF NOT EXISTS game_names (
    game_name_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    name_text TEXT NOT NULL,
    alias_type TEXT NOT NULL,
    locale TEXT NOT NULL DEFAULT '',
    source_id TEXT,
    snapshot_id TEXT NOT NULL DEFAULT '',
    confidence REAL NOT NULL DEFAULT 0,
    UNIQUE (game_id, name_text, alias_type, locale, snapshot_id),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id)
);

CREATE TABLE IF NOT EXISTS game_signatures (
    signature_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    hash_type TEXT NOT NULL,
    hash_value TEXT NOT NULL,
    source_id TEXT NOT NULL,
    snapshot_id TEXT,
    source_entry_key TEXT,
    confidence REAL NOT NULL,
    is_primary INTEGER NOT NULL DEFAULT 0,
    UNIQUE (hash_type, hash_value),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (snapshot_id) REFERENCES source_snapshots(snapshot_id)
);

CREATE TABLE IF NOT EXISTS game_serials (
    serial_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    serial_value TEXT NOT NULL,
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
    snapshot_id TEXT NOT NULL DEFAULT '',
    external_key TEXT NOT NULL,
    system_hint TEXT,
    title_raw TEXT,
    region_raw TEXT,
    payload_json TEXT,
    extracted_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (source_id, snapshot_id, external_key),
    FOREIGN KEY (source_id) REFERENCES sources(source_id)
);

CREATE TABLE IF NOT EXISTS game_facts (
    fact_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    field_name TEXT NOT NULL,
    field_value TEXT NOT NULL,
    value_type TEXT NOT NULL,
    source_id TEXT NOT NULL,
    snapshot_id TEXT NOT NULL DEFAULT '',
    source_item_id INTEGER,
    source_priority INTEGER NOT NULL,
    confidence REAL NOT NULL,
    observed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (game_id, field_name, field_value, source_id, snapshot_id),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id),
    FOREIGN KEY (source_item_id) REFERENCES source_items(source_item_id)
);

CREATE TABLE IF NOT EXISTS canonical_resolution (
    game_id TEXT NOT NULL,
    field_name TEXT NOT NULL,
    selected_fact_id INTEGER NOT NULL,
    resolved_by_rule TEXT NOT NULL,
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
    resolution_status TEXT NOT NULL DEFAULT 'unresolved',
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

COMMIT;
