BEGIN TRANSACTION;

INSERT OR REPLACE INTO merge_policy (field_name, rule_order, rule_key, rule_description, active) VALUES
('canonical_title', 1, 'exact_hash_source_priority', 'If title comes from an exact hash-identified record, pick highest source priority.', 1),
('canonical_title', 2, 'normalized_name_similarity', 'If no exact hash title exists, pick highest normalized similarity to canonical alias set.', 1),
('canonical_title', 3, 'shortest_stable_title', 'If still tied, pick shortest non-empty stable title variant.', 1),

('primary_region_code', 1, 'explicit_region_codes', 'Prefer explicit normalized region codes from source over parsed title tokens.', 1),
('primary_region_code', 2, 'region_token_parse', 'Use parsed region token from title only when explicit region is absent.', 1),

('release_date', 1, 'full_date_preferred', 'Prefer full YYYY-MM-DD values over year-only values.', 1),
('release_date', 2, 'higher_priority_source', 'Tie-break release_date by source priority.', 1),
('release_date', 3, 'newer_snapshot', 'If still tied, prefer newest snapshot.', 1),

('release_year', 1, 'derive_from_release_date', 'If canonical release_date exists, derive release_year from it.', 1),
('release_year', 2, 'max_confidence_year', 'Otherwise, pick highest-confidence release_year fact.', 1),

('developer', 1, 'exact_hash_source_priority', 'Prefer developer from highest-priority source for exact-hash matched record.', 1),
('developer', 2, 'most_frequent_value', 'Tie-break developer using most frequent normalized value.', 1),

('publisher', 1, 'exact_hash_source_priority', 'Prefer publisher from highest-priority source for exact-hash matched record.', 1),
('publisher', 2, 'most_frequent_value', 'Tie-break publisher using most frequent normalized value.', 1),

('genre', 1, 'normalized_taxonomy_match', 'Prefer genre mapped to canonical taxonomy vocabulary.', 1),
('genre', 2, 'higher_priority_source', 'Tie-break genre by source priority.', 1),

('players_max', 1, 'numeric_valid_range', 'Accept only numeric player counts in valid range 1..16 before ranking.', 1),
('players_max', 2, 'highest_confidence', 'Prefer highest-confidence valid players_max value.', 1),

('description', 1, 'longest_non_boilerplate', 'Prefer longest non-boilerplate description text.', 1),
('description', 2, 'higher_priority_source', 'Tie-break description by source priority.', 1),

('rating', 1, 'normalized_rating_scale', 'Convert source ratings to 0..10 and compare confidence and source priority.', 1);

COMMIT;
