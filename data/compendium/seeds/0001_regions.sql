BEGIN TRANSACTION;

INSERT OR REPLACE INTO regions (region_code, display_name, group_code, parent_region_code) VALUES
('GLOBAL', 'Global',          'global',   NULL),
('WORLD',  'World',           'global',   'GLOBAL'),
('USA',    'United States',   'americas', 'GLOBAL'),
('EUR',    'Europe',          'europe',   'GLOBAL'),
('JPN',    'Japan',           'asia',     'GLOBAL'),
('AUS',    'Australia',       'oceania',  'GLOBAL'),
('BRA',    'Brazil',          'americas', 'GLOBAL'),
-- Asia / Pacific
('ASIA',   'Asia',            'asia',     'GLOBAL'),
('KOR',    'Korea',           'asia',     'GLOBAL'),
('CHN',    'China',           'asia',     'GLOBAL'),
('TWN',    'Taiwan',          'asia',     'GLOBAL'),
-- Europe (country-level)
('FRA',    'France',          'europe',   'EUR'),
('DEU',    'Germany',         'europe',   'EUR'),
('ITA',    'Italy',           'europe',   'EUR'),
('ESP',    'Spain',           'europe',   'EUR'),
('SWE',    'Sweden',          'europe',   'EUR'),
('NLD',    'Netherlands',     'europe',   'EUR'),
('POR',    'Portugal',        'europe',   'EUR'),
('SCA',    'Scandinavia',     'europe',   'EUR'),
-- Other
('RUS',    'Russia',          'europe',   'GLOBAL'),
('LATAM',  'Latin America',   'americas', 'GLOBAL');

COMMIT;
