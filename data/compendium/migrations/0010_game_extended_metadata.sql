-- G7: artwork, series, age rating, and alternate titles on games.
PRAGMA foreign_keys = ON;
BEGIN TRANSACTION;
ALTER TABLE games
ADD COLUMN cover_url TEXT;
ALTER TABLE games
ADD COLUMN series TEXT;
ALTER TABLE games
ADD COLUMN age_rating TEXT;
ALTER TABLE games
ADD COLUMN alternate_titles TEXT;
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
        'Prefer cover_url from highest-priority source.',
        1
    ),
    (
        'series',
        1,
        'higher_priority_source',
        'Prefer series from highest-priority source.',
        1
    ),
    (
        'age_rating',
        1,
        'higher_priority_source',
        'Prefer age_rating from highest-priority source.',
        1
    ),
    (
        'alternate_titles',
        1,
        'higher_priority_source',
        'Prefer alternate_titles JSON from highest-priority source.',
        1
    );
COMMIT;
