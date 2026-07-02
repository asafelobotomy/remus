PRAGMA foreign_keys = ON;
BEGIN TRANSACTION;
-- One row per DAT game block (one "disc" in Redump terms).
CREATE TABLE IF NOT EXISTS game_disc_sets (
    disc_set_id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL REFERENCES games(game_id) ON DELETE CASCADE,
    set_key TEXT NOT NULL,
    disc_number INTEGER NOT NULL DEFAULT 0,
    disc_count INTEGER NOT NULL DEFAULT 0,
    set_variant TEXT NOT NULL DEFAULT '',
    set_role TEXT NOT NULL DEFAULT 'game',
    title_disc TEXT NOT NULL,
    source_id TEXT NOT NULL REFERENCES sources(source_id),
    snapshot_id TEXT NOT NULL DEFAULT '',
    source_item_id INTEGER REFERENCES source_items(source_item_id),
    primary_content_sha1 TEXT,
    UNIQUE (
        set_key,
        disc_number,
        set_variant,
        source_id,
        snapshot_id
    )
);
-- One row per verifiable ROM/track within that disc block.
CREATE TABLE IF NOT EXISTS game_disc_tracks (
    track_id INTEGER PRIMARY KEY AUTOINCREMENT,
    disc_set_id INTEGER NOT NULL REFERENCES game_disc_sets(disc_set_id) ON DELETE CASCADE,
    track_index INTEGER NOT NULL DEFAULT 1,
    rom_name TEXT NOT NULL,
    signature_id INTEGER REFERENCES game_signatures(signature_id),
    source_entry_key TEXT NOT NULL,
    UNIQUE (disc_set_id, track_index),
    UNIQUE (disc_set_id, source_entry_key)
);
CREATE INDEX IF NOT EXISTS idx_game_disc_sets_game ON game_disc_sets(game_id);
CREATE INDEX IF NOT EXISTS idx_game_disc_sets_set_key ON game_disc_sets(set_key, disc_number);
CREATE INDEX IF NOT EXISTS idx_game_disc_tracks_disc ON game_disc_tracks(disc_set_id);
COMMIT;
