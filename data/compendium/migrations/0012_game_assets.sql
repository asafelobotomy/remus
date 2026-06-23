PRAGMA foreign_keys = ON;
BEGIN TRANSACTION;
CREATE TABLE IF NOT EXISTS game_assets (
    game_id TEXT NOT NULL,
    asset_type TEXT NOT NULL CHECK (asset_type IN ('box', 'snap', 'title', 'logo')),
    storage_path TEXT NOT NULL,
    content_sha256 TEXT NOT NULL,
    byte_size INTEGER,
    width INTEGER,
    height INTEGER,
    mime_type TEXT NOT NULL DEFAULT 'image/webp',
    source_id TEXT NOT NULL,
    source_path TEXT,
    snapshot_id TEXT NOT NULL DEFAULT '',
    confidence REAL NOT NULL DEFAULT 1.0,
    PRIMARY KEY (game_id, asset_type),
    FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE,
    FOREIGN KEY (source_id) REFERENCES sources(source_id)
);
CREATE INDEX IF NOT EXISTS idx_game_assets_sha ON game_assets(content_sha256);
CREATE TABLE IF NOT EXISTS blob_inventory (
    content_sha256 TEXT PRIMARY KEY,
    storage_path TEXT NOT NULL UNIQUE,
    mime_type TEXT NOT NULL,
    byte_size INTEGER NOT NULL,
    ref_count INTEGER NOT NULL DEFAULT 0
);
INSERT
    OR IGNORE INTO sources (
        source_id,
        display_name,
        source_type,
        license_id,
        license_url,
        attribution_required,
        priority,
        enabled
    )
VALUES (
        'remus-thumbnails',
        'Remus consolidated artwork',
        'artwork',
        NULL,
        NULL,
        1,
        15,
        1
    );
INSERT
    OR REPLACE INTO merge_policy (
        field_name,
        rule_order,
        rule_key,
        rule_description,
        active
    )
VALUES (
        'cover_url',
        1,
        'higher_priority_source',
        'Prefer cover_url; remus-thumbnails (local) beats CDN.',
        1
    );
COMMIT;
