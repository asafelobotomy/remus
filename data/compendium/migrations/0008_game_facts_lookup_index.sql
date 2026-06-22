-- Speed up per-field fact replace during enrichment (game_id + field_name + source_id).
CREATE INDEX IF NOT EXISTS idx_game_facts_game_field_source ON game_facts(game_id, field_name, source_id);
