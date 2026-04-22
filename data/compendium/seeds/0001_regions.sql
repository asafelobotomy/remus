BEGIN TRANSACTION;

INSERT OR REPLACE INTO regions (region_code, display_name, group_code, parent_region_code) VALUES
('GLOBAL', 'Global', 'global', NULL),
('WORLD', 'World', 'global', 'GLOBAL'),
('USA', 'United States', 'americas', 'GLOBAL'),
('EUR', 'Europe', 'europe', 'GLOBAL'),
('JPN', 'Japan', 'asia', 'GLOBAL'),
('AUS', 'Australia', 'oceania', 'GLOBAL'),
('BRA', 'Brazil', 'americas', 'GLOBAL');

COMMIT;
