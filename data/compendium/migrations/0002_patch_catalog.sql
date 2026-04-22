PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- Patch catalog sources table.
-- Replaces the runtime `patch_verification_dats` table that was populated by importing
-- raw DAT files.  Catalog sources live inside the bundled compendium artifact so no
-- separate import step is required at runtime.
CREATE TABLE IF NOT EXISTS patch_catalog_sources (
    source_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    system_name TEXT    NOT NULL,
    catalog_name    TEXT    NOT NULL,
    catalog_version TEXT,
    catalog_source  TEXT,           -- e.g. "no-intro", "redump", "community"
    catalog_description TEXT,
    entry_count INTEGER NOT NULL DEFAULT 0,
    created_at  TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (system_name, catalog_name)
);

-- Individual patch/translation ROM entries.
-- Mirrors the columns from the runtime `patch_dat_entries` table so that existing
-- verification logic can switch data source without changing the result structs.
CREATE TABLE IF NOT EXISTS patch_entries (
    entry_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id   INTEGER NOT NULL,
    game_name   TEXT    NOT NULL,
    rom_name    TEXT    NOT NULL,
    rom_size    INTEGER,
    crc32       TEXT,
    md5         TEXT,
    sha1        TEXT,
    description TEXT,
    status      TEXT,
    base_title  TEXT,
    patch_name  TEXT,
    file_type   TEXT,
    FOREIGN KEY (source_id) REFERENCES patch_catalog_sources(source_id) ON DELETE CASCADE
);

-- Fast hash-based lookups (same pattern as game_signatures).
CREATE INDEX IF NOT EXISTS idx_patch_entries_crc32
    ON patch_entries(crc32);

CREATE INDEX IF NOT EXISTS idx_patch_entries_md5
    ON patch_entries(md5);

CREATE INDEX IF NOT EXISTS idx_patch_entries_sha1
    ON patch_entries(sha1);

CREATE INDEX IF NOT EXISTS idx_patch_entries_source
    ON patch_entries(source_id);

CREATE INDEX IF NOT EXISTS idx_patch_catalog_sources_system
    ON patch_catalog_sources(system_name);

COMMIT;
