-- Speed source_items ↔ game_signatures bridge checks (validation, PlayMatch, disc backfill).
CREATE INDEX IF NOT EXISTS idx_game_signatures_source_entry_key
    ON game_signatures(source_entry_key);
