# Data Model

## Database Schema (SQLite)

### Table: systems

Stores supported gaming systems/platforms.

```sql
CREATE TABLE systems (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,          -- e.g., "NES", "SNES", "PlayStation"
    display_name TEXT NOT NULL,          -- e.g., "Nintendo Entertainment System"
    manufacturer TEXT,                   -- e.g., "Nintendo", "Sony"
    generation INTEGER,                  -- Console generation (3, 4, 5, etc.)
    release_year INTEGER,
    extensions TEXT NOT NULL,            -- JSON array: [".nes", ".unf"]
    preferred_hash TEXT NOT NULL,        -- "CRC32", "MD5", or "SHA1"
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Table: libraries

Stores user-configured library paths.

```sql
CREATE TABLE libraries (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT NOT NULL UNIQUE,
    name TEXT,                           -- User-friendly name
    enabled BOOLEAN DEFAULT 1,
    last_scanned TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Table: files

Stores scanned ROM/disc files.

```sql
CREATE TABLE files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    library_id INTEGER NOT NULL,
    original_path TEXT NOT NULL,         -- Full absolute path when first scanned
    current_path TEXT NOT NULL,          -- Current path (may change after organize)
    filename TEXT NOT NULL,
    extension TEXT NOT NULL,
    file_size INTEGER NOT NULL,          -- In bytes
    system_id INTEGER,                   -- Foreign key to systems
    crc32 TEXT,
    md5 TEXT,
    sha1 TEXT,
    hash_calculated BOOLEAN DEFAULT 0,
    is_primary BOOLEAN DEFAULT 1,        -- False for .bin files in .cue+.bin sets
    parent_file_id INTEGER,              -- Reference to .cue file for .bin files
    is_m3u_playlist BOOLEAN DEFAULT 0,   -- True for .m3u playlist files
    m3u_content TEXT,                    -- Contents of M3U file (list of disc paths)
    is_archive BOOLEAN DEFAULT 0,        -- True for .zip, .7z, .rar files
    extracted_from TEXT,                 -- Original archive path if extracted
    verification_status TEXT DEFAULT 'unknown', -- "verified", "failed", "unknown", "not_checked"
    file_type TEXT DEFAULT 'official',   -- "official", "hack", "translation", "homebrew", "prototype"
    is_patched BOOLEAN DEFAULT 0,        -- Indicates if file was created via patching
    last_modified TIMESTAMP,
    scanned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (library_id) REFERENCES libraries(id) ON DELETE CASCADE,
    FOREIGN KEY (system_id) REFERENCES systems(id),
    FOREIGN KEY (parent_file_id) REFERENCES files(id) ON DELETE CASCADE
);

CREATE INDEX idx_files_current_path ON files(current_path);
CREATE INDEX idx_files_system_id ON files(system_id);
CREATE INDEX idx_files_hashes ON files(crc32, md5, sha1);
```

### Table: games

Stores game metadata from providers.

```sql
CREATE TABLE games (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    system_id INTEGER NOT NULL,
    region TEXT,                         -- e.g., "USA", "EUR", "JPN"
    publisher TEXT,
    developer TEXT,
    release_date TEXT,                   -- ISO 8601 date
    genres TEXT,                         -- Comma-separated list (e.g., "Platform, Action")
    players TEXT,                        -- e.g., "1-2"
    description TEXT,
    rating REAL,                         -- 0.0 to 10.0
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (system_id) REFERENCES systems(id)
);

CREATE INDEX idx_games_title ON games(title);
CREATE INDEX idx_games_system ON games(system_id);
```

### Table: metadata_sources

Stores metadata from different providers for each game.

```sql
CREATE TABLE metadata_sources (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id INTEGER NOT NULL,
    provider TEXT NOT NULL,              -- "screenscraper", "thegamesdb", "igdb"
    provider_id TEXT NOT NULL,           -- ID in the provider's system
    provider_data TEXT,                  -- JSON blob of raw provider data
    fetched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE,
    UNIQUE(provider, provider_id)
);

CREATE INDEX idx_metadata_sources_game ON metadata_sources(game_id);
CREATE INDEX idx_metadata_sources_provider ON metadata_sources(provider, provider_id);
```

### Table: matches

Stores file-to-game matches.

```sql
CREATE TABLE matches (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    game_id INTEGER NOT NULL,
    match_method TEXT NOT NULL,          -- "hash", "filename", "fuzzy", "manual"
    confidence REAL NOT NULL,            -- 0.0 to 100.0
    is_confirmed BOOLEAN DEFAULT 0,      -- User confirmed match
    is_rejected BOOLEAN DEFAULT 0,       -- User rejected this match
    matched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE,
    UNIQUE(file_id, game_id)
);

CREATE INDEX idx_matches_file ON matches(file_id);
CREATE INDEX idx_matches_game ON matches(game_id);
CREATE INDEX idx_matches_confidence ON matches(confidence);
```

### Table: artwork

Stores downloaded artwork/media.

```sql
CREATE TABLE artwork (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id INTEGER NOT NULL,
    type TEXT NOT NULL,                  -- "boxart", "screenshot", "banner", "logo", "video"
    url TEXT,                            -- Original URL
    local_path TEXT,                     -- Path to cached file
    width INTEGER,
    height INTEGER,
    provider TEXT,
    downloaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE
);

CREATE INDEX idx_artwork_game ON artwork(game_id);
CREATE INDEX idx_artwork_type ON artwork(type);
```

### Table: operations

Stores history of file operations for undo.

```sql
CREATE TABLE operations (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    operation_type TEXT NOT NULL,        -- "rename", "move", "copy", "chd_convert", "chd_extract", "archive_extract"
    file_id INTEGER NOT NULL,
    old_path TEXT NOT NULL,
    new_path TEXT NOT NULL,
    old_size INTEGER,                    -- Original file size (for space tracking)
    new_size INTEGER,                    -- New file size
    additional_data TEXT,                -- JSON blob for operation-specific data
    is_reverted BOOLEAN DEFAULT 0,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    reverted_at TIMESTAMP,
    FOREIGN KEY (file_id) REFERENCES files(id)
);

CREATE INDEX idx_operations_file ON operations(file_id);
CREATE INDEX idx_operations_date ON operations(executed_at);
CREATE INDEX idx_operations_type ON operations(operation_type);
```

### Table: conversion_jobs

Tracks CHD conversion and other file transformation jobs.

```sql
CREATE TABLE conversion_jobs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    job_type TEXT NOT NULL,              -- "chd_compress", "chd_extract", "archive_extract", "m3u_generate"
    status TEXT NOT NULL,                -- "pending", "running", "completed", "failed"
    source_file_id INTEGER NOT NULL,
    target_path TEXT,
    progress REAL DEFAULT 0.0,           -- 0.0 to 100.0
    original_size INTEGER,
    compressed_size INTEGER,
    compression_ratio REAL,
    error_message TEXT,
    started_at TIMESTAMP,
    completed_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (source_file_id) REFERENCES files(id)
);

CREATE INDEX idx_conversion_jobs_status ON conversion_jobs(status);
CREATE INDEX idx_conversion_jobs_type ON conversion_jobs(job_type);
```

### Table: settings

Stores application settings.

```sql
CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Table: provider_credentials

Stores API credentials for metadata providers.

```sql
CREATE TABLE provider_credentials (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    provider TEXT NOT NULL UNIQUE,       -- "screenscraper", "thegamesdb", "igdb"
    username TEXT,
    password TEXT,
    api_key TEXT,
    oauth_token TEXT,
    oauth_refresh_token TEXT,
    token_expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Table: cache

Generic cache for API responses.

```sql
CREATE TABLE cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    cache_key TEXT NOT NULL UNIQUE,
    cache_value TEXT NOT NULL,           -- JSON blob
    expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_cache_key ON cache(cache_key);
CREATE INDEX idx_cache_expires ON cache(expires_at);
```

## Initial Data

### Pre-populated Systems

The application will ship with pre-configured system definitions:

```json
[
  {
    "name": "NES",
    "display_name": "Nintendo Entertainment System",
    "manufacturer": "Nintendo",
    "generation": 3,
    "release_year": 1983,
    "extensions": [".nes", ".unf"],
    "preferred_hash": "CRC32"
  },
  {
    "name": "SNES",
    "display_name": "Super Nintendo Entertainment System",
    "manufacturer": "Nintendo",
    "generation": 4,
    "release_year": 1990,
    "extensions": [".sfc", ".smc"],
    "preferred_hash": "CRC32"
  },
  {
    "name": "PlayStation",
    "display_name": "Sony PlayStation",
    "manufacturer": "Sony",
    "generation": 5,
    "release_year": 1994,
    "extensions": [".cue", ".bin", ".iso", ".pbp", ".chd"],
    "preferred_hash": "MD5"
  }
  // ... more systems
]
```

## API Data Models

### Metadata Provider Response (Normalized)

```typescript
interface GameMetadata {
    provider: string;              // "screenscraper" | "thegamesdb" | "igdb"
    provider_id: string;
    title: string;
    system: string;
    region?: string;
    publisher?: string;
    developer?: string;
    release_date?: string;         // ISO 8601
    genres?: string[];
    players?: string;
    description?: string;
    rating?: number;
    artwork: {
        type: string;              // "boxart" | "screenshot" | "banner" | "logo"
        url: string;
        width?: number;
        height?: number;
    }[];
}
```

### Match Result

```typescript
interface MatchResult {
    file_id: number;
    game_id: number;
    match_method: "hash" | "filename" | "fuzzy" | "manual";
    confidence: number;            // 0-100
    game_metadata: GameMetadata;
}
```

### File Scan Result

```typescript
interface ScanResult {
    path: string;
    filename: string;
    extension: string;
    file_size: number;
    detected_system?: string;
    crc32?: string;
    md5?: string;
    sha1?: string;
}
```

## Relationships

```
libraries (1) ──> (N) files
systems (1) ──> (N) files
systems (1) ──> (N) games
systems (1) ──> (N) verification_dats
systems (1) ──> (N) patches
files (1) ──> (N) matches
files (1) ──> (N) operations
files (1) ──> (N) verification_results
files (1) ──> (N) patch_applications (as base_file)
files (1) ──> (N) patch_applications (as patched_file)
files (1) ──> (N) files (parent_file_id for multi-file games)
games (1) ──> (N) matches
games (1) ──> (N) metadata_sources
games (1) ──> (N) artwork
patches (1) ──> (N) patch_applications
verification_dats (1) ──> (N) verification_results
```
