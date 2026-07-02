PRAGMA foreign_keys = OFF;
-- Allow the same DAT external key on multiple disc sets (e.g. nointro + libretro-dats
-- snapshots for the same ROM). Global uniqueness on source_entry_key left orphan sets.
CREATE TABLE game_disc_tracks_new (
    track_id INTEGER PRIMARY KEY AUTOINCREMENT,
    disc_set_id INTEGER NOT NULL REFERENCES game_disc_sets(disc_set_id) ON DELETE CASCADE,
    track_index INTEGER NOT NULL DEFAULT 1,
    rom_name TEXT NOT NULL,
    signature_id INTEGER REFERENCES game_signatures(signature_id),
    source_entry_key TEXT NOT NULL,
    UNIQUE (disc_set_id, track_index),
    UNIQUE (disc_set_id, source_entry_key)
);
INSERT INTO game_disc_tracks_new (
        track_id,
        disc_set_id,
        track_index,
        rom_name,
        signature_id,
        source_entry_key
    )
SELECT track_id,
    disc_set_id,
    track_index,
    rom_name,
    signature_id,
    source_entry_key
FROM game_disc_tracks;
DROP TABLE game_disc_tracks;
ALTER TABLE game_disc_tracks_new
    RENAME TO game_disc_tracks;
CREATE INDEX IF NOT EXISTS idx_game_disc_tracks_disc ON game_disc_tracks(disc_set_id);
PRAGMA foreign_keys = ON;
