# Metadata Display Fix

## Problem Summary
When ROMs were matched to games, the **description, rating, genre, players, region, and developer** fields were not displaying in the UI sidebar, even though:
- The UI components existed and were properly wired
- The `FileListModel` had all necessary roles defined
- The `GameMetadata` struct contained all the fields
- The database schema included all required columns

## Root Cause
The `Database::getAllMatches()` method only queried a **subset of game metadata fields** from the database:

**Before (Incomplete Query):**
```sql
SELECT m.id, m.file_id, m.game_id, m.match_method, m.confidence, 
       m.is_confirmed, m.is_rejected,
       g.title, g.publisher, g.release_date
FROM matches m
LEFT JOIN games g ON m.game_id = g.id
```

**Missing fields:** developer, description, genres, players, region, rating

## Fix Applied
Updated `Database::getAllMatches()` in [database.cpp](src/core/database.cpp) (line 657) to query **all metadata fields**:

**After (Complete Query):**
```sql
SELECT m.id, m.file_id, m.game_id, m.match_method, m.confidence, 
       m.is_confirmed, m.is_rejected,
       g.title, g.publisher, g.release_date, g.developer, g.description,
       g.genres, g.players, g.region, g.rating
FROM matches m
LEFT JOIN games g ON m.game_id = g.id
```

**Populated MatchResult fields:**
- `result.developer` (line 694)
- `result.description` (line 695)
- `result.genre` (line 696)
- `result.players` (line 697)
- `result.region` (line 698)
- `result.rating` (line 699)

## Data Flow Verification

### 1. Database → Model
```
Database::getAllMatches()  
  → Returns QMap<int, MatchResult> with complete metadata
  
FileListModel::loadFiles()
  → Calls getAllMatches()
  → Passes results to groupFiles()
  
FileListModel::groupFiles()
  → Populates FileGroupEntry.matchInfo from MatchResult (lines 491-506)
```

### 2. Model → View
```
FileListModel::data()
  → Returns entry.matchInfo.description (MatchDescriptionRole)
  → Returns entry.matchInfo.genre (MatchGenreRole)
  → Returns entry.matchInfo.rating (MatchRatingRole)
  → etc.

LibraryView.qml::updateDetailPanel()
  → Reads roles from FileListModel
  → Sets properties on ProcessedSidebar

ProcessedSidebar.qml
  → Displays description with expand/collapse
  → Shows details grid: genre, players, region, rating
  → All UI components already exist and work correctly
```

## Testing

### Manual Test Data Inserted
```sql
-- Game record
INSERT INTO games (title, system_id, region, publisher, developer, release_date, 
                   description, genres, players, rating) 
VALUES ('Sonic The Hedgehog', 10, 'USA', 'Sega', 'Sonic Team', '1991-06-23', 
        'A landmark platform game featuring Sonic...', 'Platform, Action', '1', 8.5);

-- Match record (links file ID 11 to game ID 1)
INSERT INTO matches (file_id, game_id, match_method, confidence, is_confirmed) 
VALUES (11, 1, 'manual', 100.0, 1);
```

### Verification Query
```bash
sqlite3 ~/.local/share/Remus/Remus/remus.db \
"SELECT m.file_id, g.title, g.description, g.genres, g.rating 
 FROM matches m JOIN games g ON m.game_id = g.id WHERE m.file_id = 11;"
```

**Expected output:**
```
11|Sonic The Hedgehog|A landmark platform game featuring Sonic...|Platform, Action|8.5
```

## Documentation Fix
Updated [data-model.md](docs/data-model.md) to correct column name:
- Changed: `genre TEXT  -- JSON array`
- To: `genres TEXT  -- Comma-separated list (e.g., "Platform, Action")`

This matches the actual database schema (`PRAGMA table_info(games)` shows column 8 as "genres").

## Metadata Provider Status

### Current Provider Results (Sonic ROM Test)
1. **Hasheous** (priority 100): 404 errors (ROM not in database)
2. **TheGamesDB** (priority 50): No results (requires API key)
3. **IGDB** (priority 40): Not configured (requires Twitch credentials)
4. **ScreenScraper** (priority 90): Disabled

### Provider Configuration

#### TheGamesDB
Requires API key (free):
1. Register at https://thegamesdb.net/
2. Get API key from account settings
3. Configure in Remus settings: `setApiKey(key)`

#### IGDB
Requires Twitch credentials:
1. Register Twitch app: https://dev.twitch.tv/console/apps
2. Get Client ID and Client Secret
3. Configure in Remus: `setCredentials(clientId, clientSecret)`

#### ScreenScraper
Optional (requires account for better rate limits):
- Free tier: 1 request/sec
- Registered: Higher limits
- Configure via settings controller

## Next Steps for Full Metadata Display

1. **Configure at least one provider** (TheGamesDB recommended - easiest setup)
2. **Process a ROM** with configured provider
3. **Verify metadata displays** in ProcessedSidebar:
   - Description with expand/collapse
   - Details grid showing publisher, developer, genre, players, region, rating
   - Match confidence badge
4. **Test artwork display** (coverArtPath property exists, needs provider data)
5. **Test screenshot gallery** (screenshotPaths array exists in sidebar)

## Files Modified
- `src/core/database.cpp` - Fixed `getAllMatches()` query (added 6 fields)
- `docs/data-model.md` - Fixed `genre` → `genres` column name

## Build & Test
```bash
cd /home/solon/Documents/remus/build
make -j$(nproc)
./remus-gui
```

Select Sonic ROM in library view → metadata should now display in right sidebar.
