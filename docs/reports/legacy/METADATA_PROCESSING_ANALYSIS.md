# Metadata Processing Analysis & Constants Library Implementation

**Date:** 2026-02-07  
**Status:** Critical Issues Identified - Constants Library Required  
**Milestone:** M8 Polish → M6 Constants Consolidation Needed

## Executive Summary

After extensive debugging of the metadata processing pipeline, we've identified that the root cause of display and matching issues is **inconsistent naming and reference patterns** throughout the codebase. This document reviews what we've fixed, what's still broken, and proposes a comprehensive constants library implementation.

---

## Issues Fixed During This Session

### 1. ✅ Hash Priority Bug
**Problem:** ProcessingController was using CRC32 hash first, which Hasheous rejects (only accepts MD5/SHA1)  
**Location:** `src/ui/controllers/processing_controller.cpp:500-501`  
**Fix:** Changed hash selection order to try MD5 → SHA1 → CRC32  
**Result:** Hash lookups now use correct format for Hasheous

### 2. ✅ Metadata Storage Bug  
**Problem:** `Database::insertGame()` only saved 6/10 metadata fields (description, genres, players, rating were discarded)  
**Locations:**
- `src/core/database.cpp` - insertGame signature
- `src/ui/models/file_list_model.h` - MatchInfo struct  
- `src/ui/controllers/processing_controller.cpp` - Call sites
**Fix:** Extended insertGame() to accept all 10 fields, updated all callers  
**Result:** Complete metadata now persists to database

### 3. ✅ Provider Initialization Bug
**Problem:** Metadata providers never initialized in GUI - orchestrator created but addProvider() never called  
**Location:** `src/ui/main.cpp:70-116`  
**Fix:** Added initialization code for Hasheous, TheGamesDB, IGDB with credential checking  
**Result:** All 4 providers now active (Hasheous: 100, ScreenScraper: 90 if configured, TheGamesDB: 50, IGDB: 40)

### 4. ✅ Systems Table Empty
**Problem:** Database schema created but no systems inserted - all files had NULL system_id  
**Location:** `src/core/database.cpp:20-48`  
**Fix:** Added `populateDefaultSystems()` method, called during schema creation  
**Result:** 39 systems now initialized on first run

### 5. ✅ Settings Constants Missing
**Problem:** `IGDB_API_KEY` didn't match actual API (needed CLIENT_ID + CLIENT_SECRET), TheGamesDB key missing  
**Location:** `src/core/constants/settings.h:10-17`  
**Fix:** Added `THEGAMESDB_API_KEY`, `IGDB_CLIENT_SECRET`, removed obsolete `IGDB_API_KEY`  
**Result:** Settings controller now references correct keys

---

## Current Broken State

### 1. 🔴 System Name Display Shows "Unknown"
**Symptom:** UI sidebar shows "Unknown" instead of "Sega Genesis / Mega Drive"  
**Evidence:**
- Database has correct system: `SELECT display_name FROM systems WHERE id=10` → "Sega Genesis / Mega Drive"
- Files table has correct system_id: `SELECT system_id FROM files WHERE filename LIKE '%Sonic%'` → 10
- FileListModel added systemName field but query might be failing

**Root Cause:** Name resolution inconsistency between:
- Database internal name: `"Genesis"`
- Database display name: `"Sega Genesis / Mega Drive"`  
- Provider mapping names: TheGamesDB expects `"18"` (platform ID), not name string
- Processing controller uses: `getSystemNameForId()` which might return wrong format

**Files Involved:**
- `src/ui/models/file_list_model.cpp:406` - Calls `getSystemDisplayName()`
- `src/core/database.cpp:397-417` - New method implementation
- `src/ui/controllers/processing_controller.cpp:500-540` - Uses `getSystemNameForId()`

### 2. 🔴 All Metadata Providers Return Empty
**Symptom:** 
- Hasheous: 404 (ROM hash not in database - may be legitimate)
- TheGamesDB: Returns no results for "Sonic The Hedgehog"
- IGDB: Returns no results (credentials not configured)

**Root Causes:**
1. **TheGamesDB System Mapping:**
   - Code passes: `systemName` (e.g., "Genesis")
   - TheGamesDB expects: Platform ID `"18"` as string
   - Mapping function: `mapSystemToTheGamesDB()` uses hardcoded map
   - Issue: Internal name mismatch - database has "Genesis", code expects exact match

2. **IGDB Not Configured:**
   - Requires Twitch OAuth (client_id + client_secret)
   - No credentials in QSettings
   - Silently skips authentication, returns empty

3. **Name Cleaning Inconsistency:**
   - Processing removes: `\s*\([^)]*\)` and `\s*\[[^\]]*\]`
   - Result: "Sonic The Hedgehog (USA, Europe)" → "Sonic The Hedgehog"
   - May need region hints for some providers

**Files Involved:**
- `src/metadata/thegamesdb_provider.cpp:290-320` - System mapping
- `src/metadata/igdb_provider.cpp:29-74` - Authentication
- `src/ui/controllers/processing_controller.cpp:518-535` - Name cleaning

### 3. 🔴 Duplicate File Constraint Errors
**Symptom:** `"Failed to update file original path: UNIQUE constraint failed: files.original_path, files.filename"`  
**Root Cause:** Processing pipeline tries to update file that already exists (from previous scan)  
**Impact:** Non-fatal but indicates state management issues

---

## Constants Scattered Across Codebase

### Current State (Fragmented)

#### System Names (3+ Representations)
1. **Database `systems.name`** (internal): `"Genesis"`, `"PlayStation"`, etc.
2. **Database `systems.display_name`** (UI): `"Sega Genesis / Mega Drive"`, `"Sony PlayStation"`, etc.
3. **Provider platform IDs**:
   - TheGamesDB: `"18"` (Genesis), `"10"` (PlayStation), etc.
   - ScreenScraper: Different numeric IDs
   - IGDB: Different string names
4. **Constants::Systems::ID_GENESIS** = 10 (internal numeric ID)

#### Provider Identifiers (2 Representations)
1. **String constants** in `Constants::Providers`:
   - `HASHEOUS = "hasheous"`
   - `SCREENSCRAPER = "screenscraper"`
   - `THEGAMESDB = "thegamesdb"`
   - `IGDB = "igdb"`
2. **Settings keys** in `Constants::Settings::Providers`:
   - `SCREENSCRAPER_USERNAME` = `"screenscraper/username"`
   - Inconsistent with provider IDs

#### File Extensions (2 Locations)
1. **`Constants::Systems::SYSTEMS` registry** - Extension to system ID mapping
2. **`SystemDetector::initializeDefaultSystems()`** - Hardcoded in constructor

#### Settings Paths (Scattered)
- `Constants::Settings::Providers::*` - Provider credentials
- `Constants::Settings::Metadata::*` - Provider priorities
- `Constants::Settings::Organize::*` - File organization
- No unified namespace structure

---

## Proposed Constants Library Architecture

### Design Pattern: Centralized Registry with Type Safety

```cpp
namespace Remus::Constants {

// ============================================================================
// 1. SYSTEM REGISTRY (Single Source of Truth)
// ============================================================================

struct SystemDescriptor {
    int id;                              // Internal numeric ID (10 = Genesis)
    QString internalName;                // Database key ("Genesis")
    QString displayName;                 // UI display ("Sega Genesis / Mega Drive")
    QMap<QString, QString> providerNames;// Provider-specific mappings
    QStringList extensions;              // File extensions
    QString preferredHash;               // Hash algorithm
};

// Example:
inline const SystemDescriptor SYSTEM_GENESIS = {
    .id = 10,
    .internalName = "Genesis",
    .displayName = "Sega Genesis / Mega Drive",
    .providerNames = {
        {"thegamesdb", "18"},
        {"screenscraper", "1"},
        {"igdb", "Genesis"}
    },
    .extensions = {".md", ".gen", ".smd", ".bin", ".68k"},
    .preferredHash = "MD5"
};

// ============================================================================
// 2. PROVIDER REGISTRY (With Auth Requirements)
// ============================================================================

struct ProviderDescriptor {
    QString id;                          // "thegamesdb"
    QString displayName;                 // "TheGamesDB"
    bool requiresAuth;                   // true/false
    QStringList authFields;              // ["api_key"] or ["client_id", "client_secret"]
    int defaultPriority;                 // 50
    bool supportsHashMatching;           // false
    bool supportsNameSearch;             // true
};

inline const ProviderDescriptor PROVIDER_THEGAMESDB = {
    .id = "thegamesdb",
    .displayName = "TheGamesDB",
    .requiresAuth = false,               // Optional API key
    .authFields = {"api_key"},
    .defaultPriority = 50,
    .supportsHashMatching = false,
    .supportsNameSearch = true
};

// ============================================================================
// 3. SETTINGS PATHS (Type-Safe Accessors)
// ============================================================================

class Settings {
public:
    // Provider credentials with validation
    static QString getProviderAuthKey(const QString &providerId, const QString &field);
    
    // Example: Settings::getProviderAuthKey("thegamesdb", "api_key")
    // Returns: "thegamesdb/api_key"
};

// ============================================================================
// 4. NAME RESOLUTION (Unified API)
// ============================================================================

class SystemResolver {
public:
    // Get display name for UI
    static QString displayName(int systemId);
    static QString displayName(const QString &internalName);
    
    // Get provider-specific name for API calls
    static QString providerName(int systemId, const QString &providerId);
    static QString providerName(const QString &internalName, const QString &providerId);
    
    // Get internal name for database queries
    static QString internalName(int systemId);
    
    // Get system by extension (with ambiguity handling)
    static SystemDescriptor byExtension(const QString &ext, const QString &pathHint = "");
};

} // namespace Remus::Constants
```

---

## Implementation Plan

### Phase 1: Constants Library Foundation (Priority: CRITICAL)
**Goal:** Create single source of truth for all names and IDs  
**Files to Create:**
1. `src/core/constants/system_registry.h` - SystemDescriptor + SYSTEM_* constants
2. `src/core/constants/provider_registry.h` - ProviderDescriptor + PROVIDER_* constants
3. `src/core/constants/system_resolver.{h,cpp}` - Name resolution utilities
4. `src/core/constants/settings_paths.h` - Type-safe settings key generation

**Migration:**
- Move all system definitions from `systems.h` to `system_registry.h`
- Consolidate provider info from `providers.h` to `provider_registry.h`
- Replace hardcoded system maps in `thegamesdb_provider.cpp`, etc.

### Phase 2: Database Layer Updates
**Goal:** Use SystemResolver for all name lookups  
**Files to Update:**
1. `src/core/database.cpp` - `getSystemDisplayName()` use resolver
2. `src/ui/models/file_list_model.cpp` - System name population
3. `src/ui/controllers/processing_controller.cpp` - Provider name mapping

### Phase 3: Provider Layer Updates
**Goal:** Use provider-specific names from resolver  
**Files to Update:**
1. `src/metadata/thegamesdb_provider.cpp` - Replace `mapSystemToTheGamesDB()`
2. `src/metadata/screenscraper_provider.cpp` - Use resolver for platform ID
3. `src/metadata/igdb_provider.cpp` - Use resolver for system name
4. `src/metadata/provider_orchestrator.cpp` - Pass correct system names

### Phase 4: UI Layer Integration
**Goal:** Consistent display names everywhere  
**Files to Update:**
1. `src/ui/qml/components/*.qml` - Use system display names
2. `src/ui/controllers/*.cpp` - Use resolver for all name operations

### Phase 5: Testing & Validation
**Goal:** Verify end-to-end name resolution  
**Test Cases:**
1. Scan ROM with `.md` extension → Database stores "Genesis" → UI shows "Sega Genesis / Mega Drive"
2. Match Sonic ROM → ProcessingController passes "18" to TheGamesDB API
3. Settings controller returns correct keys for all providers

---

## Immediate Next Steps

1. **Investigate Current "Unknown" Display:**
   ```bash
   # Check what's actually in database
   sqlite3 ~/.local/share/Remus/Remus/remus.db \
     "SELECT f.filename, f.system_id, s.name, s.display_name 
      FROM files f LEFT JOIN systems s ON f.system_id = s.id 
      WHERE f.filename LIKE '%Sonic%';"
   
   # Check if getSystemDisplayName is being called
   grep -n "getSystemDisplayName" src/ui/models/file_list_model.cpp
   ```

2. **Implement Minimal SystemResolver (Quick Fix):**
   - Create `src/core/system_resolver.{h,cpp}` with basic name mapping
   - Replace FileListModel's direct database call with resolver
   - Replace ProcessingController's getSystemNameForId() with resolver

3. **Add Debug Logging:**
   - Log system name at each pipeline stage (scan → hash → match → display)
   - Verify name transformations are working

4. **Full Constants Library (M6 Proper):**
   - Follow architecture above for complete implementation
   - Migrate all hardcoded strings to registry
   - Add comprehensive unit tests

---

## Dependencies & Blockers

### Critical Path Items:
1. ✅ Systems table populated
2. ✅ Providers initialized  
3. 🔴 Name resolution (blocking metadata matching)
4. 🔴 Provider API mapping (blocking metadata fetching)
5. 🔴 Settings key consistency (blocking auth configuration)

### External Dependencies:
- TheGamesDB API may require key (need to test without)
- IGDB requires Twitch OAuth setup (user configuration needed)
- Hasheous 404s may be legitimate (ROM not in database)

---

## Success Criteria

### For "Unknown" Display Fix:
- [ ] UI shows "Sega Genesis / Mega Drive" for `.md` files
- [ ] Debug logs show correct name at each stage
- [ ] System filtering works in UI

### For Metadata Fetching:
- [ ] TheGamesDB receives correct platform ID ("18" for Genesis)
- [ ] At least one provider returns results for Sonic
- [ ] Metadata persists to database with all fields
- [ ] UI displays publisher, developer, description

### For Constants Library:
- [ ] Single source of truth for all system/provider names
- [ ] Type-safe accessor methods
- [ ] No hardcoded strings in provider code
- [ ] Comprehensive unit tests for name resolution

---

## Conclusion

The codebase has reached a complexity threshold where ad-hoc fixes are introducing more confusion. **Implementing the M6 constants library is now critical** to ensure system stability and maintainability. The architecture proposed above provides:

1. **Single Source of Truth:** All names defined once, referenced everywhere
2. **Type Safety:** Compiler-checked accessor methods prevent typos
3. **Provider Independence:** Each provider's specific requirements encapsulated
4. **Testability:** Name resolution can be unit tested in isolation
5. **Maintainability:** Adding new systems/providers requires one registry update

Recommend pausing feature work and prioritizing M6 implementation before proceeding with metadata enhancements.
