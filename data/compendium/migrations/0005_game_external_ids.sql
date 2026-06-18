-- Denormalized external IDs for runtime lookup without joining game_facts.
ALTER TABLE games ADD COLUMN igdb_id TEXT;
ALTER TABLE games ADD COLUMN ra_game_id TEXT;

CREATE INDEX IF NOT EXISTS idx_games_igdb_id ON games(igdb_id) WHERE igdb_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_games_ra_game_id ON games(ra_game_id) WHERE ra_game_id IS NOT NULL;
