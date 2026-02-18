# Constants Audit & Implementation Recommendations

**Date:** 2026-02-07  
**Status:** Comprehensive Audit Complete  
**Goal:** Identify all hardcoded values and consolidate into constants library

---

## Executive Summary

After implementing `SystemResolver` for system name mappings, a full audit reveals **7 additional categories** where constants should be used but aren't. Current codebase shows:

- ✅ **System IDs & Names**: Now using `SystemResolver` (M6 partial complete)
- ⚠️ **Provider IDs**: Constants defined but still using hardcoded strings in 40+ locations
- ❌ **Hash Algorithm Names**: Hardcoded "CRC32", "MD5", "SHA1" in 25+ locations
- ❌ **Match Methods**: Hardcoded "hash", "name", "fuzzy", "manual" in 15+ locations
- ❌ **QML Role Numbers**: Magic numbers 257-290 scattered across QML files
- ❌ **Confidence Levels**: Magic numbers 100, 90, 70, 50 duplicated across code
- ❌ **Database Schema**: Table/column names as raw strings in 100+ queries
- ❌ **File Extensions**: Defined in systems.h but rarely used

---

## Category 1: Provider Identifiers ⚠️ PARTIAL

### Current State
**Constants Defined:** `src/core/constants/providers.h`
```cpp
inline constexpr const char* HASHEOUS = "hasheous";
inline constexpr const char* SCREENSCRAPER = "screenscraper";
inline constexpr const char* THEGAMESDB = "thegamesdb";
inline constexpr const char* IGDB = "igdb";
```

**Problem:** Constants exist but code still uses hardcoded strings!

### Violations Found (40+ instances)

#### In SystemResolver (10 instances)
**File:** `src/core/system_resolver.cpp`
```cpp
// Lines 58-350: Hardcoded in provider mappings
{"thegamesdb", QStringLiteral("7")},
{"screenscraper", QStringLiteral("3")},
{"igdb", QStringLiteral("nes")},
```

**Should be:**
```cpp
{Constants::Providers::THEGAMESDB, QStringLiteral("7")},
{Constants::Providers::SCREENSCRAPER, QStringLiteral("3")},
{Constants::Providers::IGDB, QStringLiteral("nes")},
```

#### In TheGamesDB Provider (2 instances)
**File:** `src/metadata/thegamesdb_provider.cpp`
```cpp
// Line 48
QString tgdbPlatformId = SystemResolver::providerName(systemId, "thegamesdb");

// Line 298
return SystemResolver::providerName(systemId, "thegamesdb");
```

**Should be:**
```cpp
using namespace Constants::Providers;
QString tgdbPlatformId = SystemResolver::providerName(systemId, THEGAMESDB);
return SystemResolver::providerName(systemId, THEGAMESDB);
```

#### In Main GUI (4 instances)
**File:** `src/ui/main.cpp` (lines 89-116)
```cpp
orchestrator.addProvider("hasheous", hasheousProvider);
orchestrator.addProvider("thegamesdb", tgdbProvider);
orchestrator.addProvider("igdb", igdbProvider);
```

**Should use:**
```cpp
using namespace Constants::Providers;
orchestrator.addProvider(HASHEOUS, hasheousProvider);
orchestrator.addProvider(THEGAMESDB, tgdbProvider);
orchestrator.addProvider(IGDB, igdbProvider);
```

#### In Tests & Metadata Cache (24+ instances)
**Files:** 
- `tests/test_cache_deserialization.cpp` - 2 instances
- `src/metadata/hasheous_provider.cpp` - 1 instance ("igdb" external ID)
- `src/metadata/metadata_provider.h` - 4 instances in comments

### Impact
- **Type Safety:** String typos won't be caught ("hashous" vs "hasheous")
- **Maintainability:** Changing provider ID requires grep/replace
- **Autocomplete:** IDE can't suggest constants
- **Consistency:** Mix of hardcoded strings and constants

### Recommendation: **HIGH PRIORITY**
Create `ProviderIds` utility class with validation:
```cpp
namespace Constants::Providers {

class ProviderIds {
public:
    // Validate provider ID at runtime
    static bool isValid(const QString &providerId) {
        return providerId == HASHEOUS || 
               providerId == SCREENSCRAPER ||
               providerId == THEGAMESDB ||
               providerId == IGDB;
    }
    
    // Get all provider IDs as list
    static QStringList all() {
        return {HASHEOUS, SCREENSCRAPER, THEGAMESDB, IGDB};
    }
};

} // namespace
```

---

## Category 2: Hash Algorithm Names ❌ NOT IMPLEMENTED

### Current State
**No constants defined!** 

### Violations Found (25+ instances)

#### In Hasheous Provider (4 instances)
**File:** `src/metadata/hasheous_provider.cpp`
```cpp
// Line 37-39
if (hashLength == 32) return "md5";
else if (hashLength == 40) return "sha1";

// Line 156
params.addQueryItem("hash_type", detectHashType(hash));
```

#### In ScreenScraper Provider (2 instances)
**File:** `src/metadata/screenscraper_provider.cpp`
```cpp
// Line 501-503
if (hashLength == 32) return "md5";
else if (hashLength == 40) return "sha1";
```

#### In Database (3 instances)
**File:** `src/core/database.cpp`
```cpp
// Line 828 - Hash algorithm stored as string column
preferred_hash VARCHAR(10)  // "CRC32", "MD5", "SHA1"
```

#### In Verification Engine (4 instances)
**File:** `src/core/verification_engine.h`
```cpp
// Line 44
QString hashType;  // "crc32", "md5", "sha1"
```

**File:** `src/core/verification_engine.cpp`
```cpp
// Line 261
if (hashType == "sha1" && !entry.sha1.isEmpty()) { ... }
```

#### In DAT Parser (6 instances)
**File:** `src/core/dat_parser.h`
```cpp
// Line 83
* @param hashType "crc32", "md5", or "sha1"
```

**File:** `tests/test_dat_parser.cpp`
```cpp
// Lines 312, 329, 343, 351, 370
QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "crc32");
QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "md5");
QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "sha1");
```

#### In Constants/Systems (6 instances)
**File:** `src/core/constants/systems.h`
```cpp
// Lines 160, 230, 265 (examples)
QStringLiteral("CRC32"),
QStringLiteral("MD5"),
QStringLiteral("SHA1"),
```

### Impact
- **Case Sensitivity Issues:** "md5" vs "MD5" vs "Md5"
- **Validation:** No way to check if hash type is valid
- **Hash Length Constants:** Magic numbers 32, 40, 8
- **API Inconsistency:** Providers expect lowercase, database stores uppercase

### Recommendation: **CRITICAL PRIORITY**

Create `src/core/constants/hash_algorithms.h`:
```cpp
namespace Remus::Constants {

class HashAlgorithms {
public:
    // Algorithm identifiers (lowercase for APIs)
    static constexpr const char* CRC32 = "crc32";
    static constexpr const char* MD5 = "md5";
    static constexpr const char* SHA1 = "sha1";
    
    // Display names (uppercase for UI)
    static constexpr const char* CRC32_DISPLAY = "CRC32";
    static constexpr const char* MD5_DISPLAY = "MD5";
    static constexpr const char* SHA1_DISPLAY = "SHA1";
    
    // Hash lengths
    static constexpr int CRC32_LENGTH = 8;
    static constexpr int MD5_LENGTH = 32;
    static constexpr int SHA1_LENGTH = 40;
    
    // Detect algorithm from hash string length
    static QString detectFromLength(int length) {
        switch (length) {
            case CRC32_LENGTH: return CRC32;
            case MD5_LENGTH: return MD5;
            case SHA1_LENGTH: return SHA1;
            default: return QString();
        }
    }
    
    // Validate hash string
    static bool isValidHash(const QString &hash, const QString &algorithm) {
        if (algorithm == CRC32) return hash.length() == CRC32_LENGTH;
        if (algorithm == MD5) return hash.length() == MD5_LENGTH;
        if (algorithm == SHA1) return hash.length() == SHA1_LENGTH;
        return false;
    }
    
    // Get display name from algorithm
    static QString displayName(const QString &algorithm) {
        if (algorithm == CRC32) return CRC32_DISPLAY;
        if (algorithm == MD5) return MD5_DISPLAY;
        if (algorithm == SHA1) return SHA1_DISPLAY;
        return algorithm.toUpper();
    }
};

} // namespace
```

**Usage Example:**
```cpp
// Before
if (hashLength == 32) return "md5";
params.addQueryItem("hash_type", "md5");

// After
using namespace Constants;
if (hashLength == HashAlgorithms::MD5_LENGTH) 
    return HashAlgorithms::MD5;
params.addQueryItem("hash_type", HashAlgorithms::MD5);
```

---

## Category 3: Match Methods ❌ NOT IMPLEMENTED

### Current State
**No constants defined!**

### Violations Found (15+ instances)

#### In Provider Orchestrator (6 instances)
**File:** `src/metadata/provider_orchestrator.cpp`
```cpp
// Lines 134, 142, 174, 189, 220, 234
emit tryingProvider(providerName, "hash");
emit providerSucceeded(providerName, "hash");
emit tryingProvider(providerName, "name");
emit providerSucceeded(providerName, "name");
metadata.matchMethod = "hash";
emit tryingProvider(providerName, "name");
```

#### In Processing Controller (1 instance)
**File:** `src/ui/controllers/processing_controller.cpp`
```cpp
// Line 514
QString matchMethod = "none";
```

#### In Tests (2 instances)
**File:** `tests/test_cache_deserialization.cpp`
```cpp
// Line 37
original.matchMethod = "hash";
```

**File:** `tests/test_matching_engine.cpp`
```cpp
// Lines 64, 96
int confidence = MatchingEngine::calculateConfidence("hash", 0.0f);
int confidence = MatchingEngine::calculateConfidence("manual", 0.0f);
```

#### In QML (1 instance)
**File:** `src/ui/qml/components/UnprocessedSidebar.qml`
```cpp
// Line 357
text: root.currentMatch ? (root.currentMatch.matchMethod === "hash" ? "via Hash" : "via Name") : ""
```

### Impact
- **UI Display:** Need consistent labels ("Hash Match", "Name Match", etc.)
- **Database Storage:** Match method stored as string
- **Validation:** No enum or validation for valid match types
- **Filtering:** Can't easily filter by match method

### Recommendation: **HIGH PRIORITY**

Create `src/core/constants/match_methods.h`:
```cpp
namespace Remus::Constants {

class MatchMethods {
public:
    // Method identifiers (database storage)
    static constexpr const char* HASH = "hash";
    static constexpr const char* NAME = "name";
    static constexpr const char* FUZZY = "fuzzy";
    static constexpr const char* MANUAL = "manual";
    static constexpr const char* NONE = "none";
    
    // Display names (UI)
    static constexpr const char* HASH_DISPLAY = "Hash Match";
    static constexpr const char* NAME_DISPLAY = "Name Match";
    static constexpr const char* FUZZY_DISPLAY = "Fuzzy Match";
    static constexpr const char* MANUAL_DISPLAY = "Manual";
    static constexpr const char* NONE_DISPLAY = "Not Matched";
    
    // Get display name from method
    static QString displayName(const QString &method) {
        if (method == HASH) return HASH_DISPLAY;
        if (method == NAME) return NAME_DISPLAY;
        if (method == FUZZY) return FUZZY_DISPLAY;
        if (method == MANUAL) return MANUAL_DISPLAY;
        return NONE_DISPLAY;
    }
    
    // Validate method
    static bool isValid(const QString &method) {
        return method == HASH || method == NAME || 
               method == FUZZY || method == MANUAL || method == NONE;
    }
};

} // namespace
```

---

## Category 4: QML Role Numbers ❌ MAGIC NUMBERS

### Current State
**Magic numbers scattered throughout QML!**

### Violations Found (22 instances in one file!)

**File:** `src/ui/qml/LibraryView.qml` (lines 713-735)
```javascript
var fid = fileListModel.data(idx, 257); // IdRole
sidebarData.filename = fileListModel.data(idx, 258) || "";
sidebarData.filePath = fileListModel.data(idx, 259) || "";
sidebarData.extensions = fileListModel.data(idx, 261) || "";
sidebarData.fileSize = fileListModel.data(idx, 262) || 0;
sidebarData.systemId = fileListModel.data(idx, 263) || 0;
sidebarData.isInsideArchive = fileListModel.data(idx, 276) || false;
sidebarData.matchedTitle = fileListModel.data(idx, 282) || "";
sidebarData.matchPublisher = fileListModel.data(idx, 283) || "";
sidebarData.matchYear = fileListModel.data(idx, 284) || 0;
sidebarData.matchConfidence = fileListModel.data(idx, 280) || 0;
sidebarData.matchMethod = fileListModel.data(idx, 281) || "";
sidebarData.matchDeveloper = fileListModel.data(idx, 285) || "";
sidebarData.matchGenre = fileListModel.data(idx, 286) || "";
sidebarData.matchRegion = fileListModel.data(idx, 287) || "";
sidebarData.matchDescription = fileListModel.data(idx, 288) || "";
sidebarData.matchRating = fileListModel.data(idx, 289) || 0;
sidebarData.coverArtPath = fileListModel.data(idx, 290) || "";
sidebarData.hashCRC32 = fileListModel.data(idx, 264) || "";
sidebarData.hashMD5 = fileListModel.data(idx, 265) || "";
sidebarData.hashSHA1 = fileListModel.data(idx, 266) || "";
```

**Problem:** Numbers don't match actual enum values!

### Root Cause Analysis

**Enum Definition:** `src/ui/models/file_list_model.h`
```cpp
enum FileRole {
    IdRole = Qt::UserRole + 1,  // 257
    FilenameRole,                // 258
    PathRole,                    // 259
    ExtensionRole,               // 260
    ExtensionsRole,              // 261
    FileSizeRole,                // 262
    SystemRole,                  // 263
    SystemNameRole,              // 264 (NEW!)
    // ...
};
```

When we added `SystemNameRole`, all subsequent role numbers shifted!

### Impact
- **Fragile:** Adding new roles breaks all hardcoded numbers
- **Maintenance Nightmare:** Must update QML every time model changes
- **Error-Prone:** 264 could mean SystemNameRole or Crc32Role depending on version
- **No Validation:** QML accepts any number

### Recommendation: **CRITICAL PRIORITY**

**Solution 1: Use Role Names (BEST)**
QML can access roles by name instead of number:
```javascript
// Instead of:
sidebarData.filename = fileListModel.data(idx, 258);

// Use:
let item = fileListModel.get(i);  // Get row as object
sidebarData.filename = item.filename;
sidebarData.systemId = item.systemId;
sidebarData.systemName = item.systemName;
```

**Solution 2: Expose Enum to QML**
Register enum with QML engine:
```cpp
// In main.cpp
qmlRegisterUncreatableMetaObject(
    FileListModel::staticMetaObject,
    "Remus.Models",
    1, 0,
    "FileRole",
    "Access to FileRole enum"
);
```

```javascript
// In QML
import Remus.Models 1.0

sidebarData.filename = fileListModel.data(idx, FileRole.FilenameRole);
```

**Migrationplan:**
1. Expose FileRole enum to QML
2. Update LibraryView.qml to use enum
3. Remove all magic numbers
4. Add warning comment: "DO NOT USE MAGIC NUMBERS"

---

## Category 5: Confidence Levels ❌ MAGIC NUMBERS

### Current State
**Magic numbers for confidence scores!**

### Violations Found

#### In Database (1 instance)
**File:** `src/core/database.cpp`
```cpp
// Line 828
query.prepare("UPDATE matches SET is_confirmed = 1, is_rejected = 0, confidence = 100 WHERE file_id = ?");
```

#### In Processing Controller (1 instance)
**File:** `src/ui/controllers/processing_controller.cpp`
```cpp
// Line 517
confidence = 100;
```

#### In Metadata Editor Controller (1 instance)
**File:** `src/ui/controllers/metadata_editor_controller.cpp`
```cpp
// Line 420
query.prepare("UPDATE matches SET user_confirmed = ?, confidence = CASE WHEN ? THEN 100 ELSE confidence END WHERE id = ?");
```

#### In UI (Multiple instances)
**File:** Various QML files
- Green badge for ≥ 90% confidence
- Orange badge for 60-89% confidence
- Red badge for < 60% confidence

### Current Confidence System
From `src/core/constants/confidence.h`:
```cpp
enum class ConfidenceLevel {
    Perfect = 100,   // Hash match OR user confirmation
    High = 90,       // Exact filename match
    Medium = 70,     // Close fuzzy match (80%+ similarity)
    Low = 50         // Distant fuzzy match (60-80% similarity)
};
```

**Problem:** Enum exists but not used! Code uses raw numbers.

### Impact
- **Inconsistency:** Some places use 100, others 90, arbitrary thresholds
- **UI Coupling:** QML hardcodes 90, 60 thresholds
- **No Semantic Meaning:** What does 87 vs 92 mean?

### Recommendation: **MEDIUM PRIORITY**

**Expose Confidence Enum Everywhere:**
```cpp
// In confidence.h - add helper
namespace Constants::Confidence {

inline int valueFor(ConfidenceLevel level) {
    return static_cast<int>(level);
}

inline ConfidenceLevel levelFor(int value) {
    if (value >= 100) return ConfidenceLevel::Perfect;
    if (value >= 90) return ConfidenceLevel::High;
    if (value >= 70) return ConfidenceLevel::Medium;
    if (value >= 50) return ConfidenceLevel::Low;
    return ConfidenceLevel::None;
}

inline QString displayName(ConfidenceLevel level) {
    switch (level) {
        case ConfidenceLevel::Perfect: return "Perfect";
        case ConfidenceLevel::High: return "High";
        case ConfidenceLevel::Medium: return "Medium";
        case ConfidenceLevel::Low: return "Low";
        default: return "None";
    }
}

} // namespace
```

**Usage:**
```cpp
// Instead of:
confidence = 100;
query.prepare("... confidence = 100 ...");

// Use:
using namespace Constants::Confidence;
confidence = valueFor(ConfidenceLevel::Perfect);
query.prepare(QString("... confidence = %1 ...").arg(valueFor(ConfidenceLevel::Perfect)));
```

---

## Category 6: Database Schema ❌ RAW SQL STRINGS

### Current State
**Table and column names as raw strings in 100+ locations!**

### Sample Violations

**Files Table:**
```sql
SELECT * FROM files WHERE id = ?
INSERT INTO files (filename, path, original_path, ...) VALUES (?, ?, ?, ...)
UPDATE files SET is_processed = ? WHERE id = ?
```

**Systems Table:**
```sql
SELECT id, name, display_name FROM systems WHERE id = ?
INSERT INTO systems (name, display_name, ...) VALUES (?, ?, ...)
```

**Matches Table:**
```sql
SELECT * FROM matches WHERE file_id = ?
INSERT INTO matches (file_id, game_id, confidence, ...) VALUES (?, ?, ?, ...)
UPDATE matches SET is_confirmed = 1 WHERE file_id = ?
```

### Impact
- **Refactoring Risk:** Renaming column requires 50+ file changes
- **Typos:** "confidance" vs "confidence" won't compile
- **No Autocomplete:** IDE can't suggest column names
- **Schema Migration:** Hard to track which queries need updating

### Recommendation: **LOW PRIORITY** (but high value)

Create `src/core/constants/database_schema.h`:
```cpp
namespace Remus::Constants::Database {

// Table names
namespace Tables {
    inline constexpr const char* FILES = "files";
    inline constexpr const char* GAMES = "games";
    inline constexpr const char* MATCHES = "matches";
    inline constexpr const char* SYSTEMS = "systems";
    inline constexpr const char* METADATA_SOURCES = "metadata_sources";
    // ... etc
}

// Column names
namespace Columns {
    namespace Files {
        inline constexpr const char* ID = "id";
        inline constexpr const char* FILENAME = "filename";
        inline constexpr const char* PATH = "path";
        inline constexpr const char* ORIGINAL_PATH = "original_path";
        inline constexpr const char* SYSTEM_ID = "system_id";
        inline constexpr const char* IS_PROCESSED = "is_processed";
        // ... etc
    }
    
    namespace Systems {
        inline constexpr const char* ID = "id";
        inline constexpr const char* NAME = "name";
        inline constexpr const char* DISPLAY_NAME = "display_name";
        // ... etc
    }
}

// Query builder helpers
class QueryBuilder {
public:
    static QString selectAll(const QString &table) {
        return QString("SELECT * FROM %1").arg(table);
    }
    
    static QString selectById(const QString &table) {
        return QString("SELECT * FROM %1 WHERE id = ?").arg(table);
    }
};

} // namespace
```

**Usage:**
```cpp
// Instead of:
query.prepare("SELECT * FROM files WHERE id = ?");

// Use:
using namespace Constants::Database;
query.prepare(QueryBuilder::selectById(Tables::FILES));
// or
query.prepare(QString("SELECT * FROM %1 WHERE %2 = ?")
    .arg(Tables::FILES, Columns::Files::ID));
```

---

## Category 7: File Extensions ⚠️ DEFINED BUT UNDERUSED

### Current State
**Extensions defined in systems.h but rarely referenced!**

**Defined in:** `src/core/constants/systems.h`
```cpp
{QStringLiteral(".md"), QStringLiteral(".gen"), QStringLiteral(".smd")},  // Genesis
{QStringLiteral(".iso"), QStringLiteral(".cue"), QStringLiteral(".bin")}, // PSX
```

**Map exists:** `EXTENSION_TO_SYSTEMS` in systems.h

### Violations Found

#### In Archive Extractor (4+ instances)
**File:** `src/core/archive_extractor.cpp`
```cpp
if (ext == ".zip" || ext == ".7z" || ext == ".rar")
```

#### In CHD Converter (2 instances)
**File:** `src/core/chd_converter.cpp`
```cpp
if (ext == ".cue" || ext == ".iso")
```

#### In Scanner (Unknown - need to check)
**File:** `src/core/scanner.cpp`

### Impact
- **Duplication:** Same extension lists in multiple places
- **Inconsistency:** One place checks ".ISO", another checks ".iso"
- **Missing Systems:** Easy to forget a system's extensions

### Recommendation: **LOW PRIORITY**

Create helper methods in systems.h:
```cpp
namespace Constants::Systems {

class Extensions {
public:
    // Check if extension is a ROM
    static bool isRomExtension(const QString &ext) {
        return EXTENSION_TO_SYSTEMS.contains(ext.toLower());
    }
    
    // Check if extension is an archive
    static bool isArchiveExtension(const QString &ext) {
        static const QSet<QString> archives = {
            ".zip", ".7z", ".rar", ".tar", ".gz"
        };
        return archives.contains(ext.toLower());
    }
    
    // Check if extension is disc-based (multi-file)
    static bool isDiscExtension(const QString &ext) {
        static const QSet<QString> discExts = {
            ".cue", ".gdi", ".ccd", ".toc"
        };
        return discExts.contains(ext.toLower());
    }
    
    // Get system by extension with validation
    static int systemFor(const QString &ext) {
        auto systems = EXTENSION_TO_SYSTEMS.value(ext.toLower());
        return systems.isEmpty() ? 0 : systems.first();
    }
};

} // namespace
```

---

## Implementation Priority Matrix

### Phase 1: CRITICAL (Do First)
1. **QML Role Numbers** → Use role names instead of magic numbers (LibraryView.qml fix)
2. **Hash Algorithm Names** → Create HashAlgorithms class, replace all instances

### Phase 2: HIGH (Do Next)
3. **Provider IDs** → Replace hardcoded strings with Constants::Providers::*
4. **Match Methods** → Create MatchMethods class, replace strings

### Phase 3: MEDIUM (Nice to Have)
5. **Confidence Levels** → Expose ConfidenceLevel enum helpers
6. **File Extensions** → Create Extensions helper class

### Phase 4: LOW (Future Refactor)
7. **Database Schema** → Create DatabaseSchema constants (long-term maintenance)

---

## Estimated Impact

### Before Constants (Current State)
- **40+ hardcoded provider strings**
- **25+ hardcoded hash algorithm strings**
- **15+ hardcoded match method strings**
- **22 magic role numbers in QML**
- **100+ raw SQL strings**
- **Type safety**: None
- **Validation**: Manual string comparisons
- **Maintainability**: Poor (grep/replace risky)

### After Constants (Target State)
- **0 hardcoded magic strings**
- **0 magic numbers**
- **Type safety**: Compile-time checks
- **Validation**: Automatic via constants
- **Maintainability**: Excellent (rename constant, recompile)
- **Autocomplete**: Full IDE support
- **Documentation**: Self-documenting code

---

## Success Metrics

### Phase 1 Success (QML + Hash)
- [ ] Zero magic numbers in QML files
- [ ] All hash algorithm references use HashAlgorithms::*
- [ ] Hash length constants used everywhere
- [ ] LibraryView.qml updated to use role names

### Phase 2 Success (Providers + Matches)
- [ ] All provider ID references use Constants::Providers::*
- [ ] All match method references use MatchMethods::*
- [ ] SystemResolver uses provider constants
- [ ] Tests use constants

### Phase 3 Success (Full Consolidation)
- [ ] Confidence enum exposed and used everywhere
- [ ] Extension helpers used in Scanner/Extractor/Converter
- [ ] Zero grep results for `"hasheous"` (except in constants.h)
- [ ] Zero grep results for `"md5"` (except in constants.h)

---

## Next Steps

1. **Review this document** with stakeholders
2. **Prioritize phases** based on immediate needs
3. **Create implementation branches** for each phase
4. **Update M6 milestone** with these specific tasks
5. **Write unit tests** for new constant classes
6. **Update documentation** as constants are implemented

---

## Conclusion

The **SystemResolver** implementation (M6 partial) proved the value of constants:
- Eliminated 3 conflicting hardcoded maps
- Reduced bugs (ID 10 "PlayStation" → "Genesis" fix)
- Simplified provider mapping
- Improved maintainability

Applying this same pattern to the 7 remaining categories will:
- **Prevent bugs** (type safety, validation)
- **Speed development** (autocomplete, refactoring)
- **Improve code quality** (self-documenting, DRY principle)
- **Enable future features** (easy to add new providers, systems, match methods)

**Recommendation:** Prioritize **Phase 1 (QML + Hash)** before proceeding with metadata provider fixes, as these constants are needed for reliable testing and debugging.
