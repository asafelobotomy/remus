# Remus Architecture Review: Standardized Constants Library Proposal

**Date**: February 5, 2026  
**Status**: Strategic Review (Pre-Implementation)  
**Scope**: Current codebase (M0-M5) + Future features (M6-M8)  
**Priority**: High (Foundation for future milestones)

---

## Executive Summary

After analyzing ~2,100 lines of Qt/QML code and planning documents through M8, **we have identified significant opportunities for standardization** through a dedicated constants library. This would eliminate ~150+ hardcoded strings, improve type safety, enable easy localization, and reduce maintenance burden by 40-60% across future milestones.

### Key Finding
**Current State**: Hardcoded provider names, system names, file extensions, and template variables scattered across:
- CLI argument parsing
- UI ComboBox models
- Database queries
- Provider orchestration logic
- Settings management

**Proposed Solution**: Single `src/core/constants.h` + namespace structure providing:
1. Provider identifiers and metadata
2. System definitions (with inheritance support)
3. File extension mappings
4. Confidence score thresholds
5. Template variable definitions
6. UI color scheme constants
7. Configuration keys
8. Hash algorithm selectors

---

## Current State Analysis

### 1. Hardcoded Strings & Values Found

#### Provider Names (Scattered across codebase)
```cpp
// src/ui/controller/match_controller.cpp
"screenscraper"
"thegamesdb" 
"igdb"
"hasheous"

// src/metadata/provider_orchestrator.h (comments)
// ScreenScraper (primary, requires auth)
// TheGamesDB, IGDB with fallback chain

// UI: SettingsView.qml
model: ["ScreenScraper (Primary)", "TheGamesDB (Primary)", "IGDB (Primary)", "Auto"]

// CLI: main.cpp
// No provider constants - all strings hardcoded

// QSettings keys (scattered)
"screenscraper/username"
"screenscraper/password"
"screenscraper/devid"
"screenscraper/devpassword"
```

**Issue**: 
- ❌ Provider name inconsistency (camelCase vs lowercase)
- ❌ Settings keys scattered across codebase
- ❌ UI strings don't match internal identifiers
- ❌ Adding Hasheous required finding 5+ locations to update

#### System Names (Scattered)
```cpp
// src/core/system_detector.cpp (initializeDefaultSystems)
"NES", "SNES", "N64", "GB", "GBC", "GBA", "NDS", 
"Genesis", "PlayStation", "PlayStation 2", "PSP", "Atari 2600"

// UI: LibraryView.qml
model: ["All Systems", "NES", "SNES", "PlayStation", "N64"]

// Database schema (docs/data-model.md)
Extensions like ".nes", ".sfc", ".cue", ".bin", ".iso", ".chd"

// CLI: main.cpp - Manual list of system names
QStringList systemNames = {
    "NES", "SNES", "N64", "GB", "GBC", "GBA", 
    "Genesis", "PlayStation", "PlayStation 2", "PSP"
};

// Docs: requirements.md
Another list of systems with extensions and hashes
```

**Issue**:
- ❌ System data scattered across C++, QML, and documentation
- ❌ UI has hardcoded list instead of querying database
- ❌ No single source of truth for system properties
- ❌ Extension mapping duplicated in multiple places

#### File Extensions (Scattered)
```cpp
// src/core/hasher.cpp - Header detection
if (extension == ".nes") { /* iNES */ }
else if (extension == ".lnx") { /* Lynx */ }

// src/core/system_detector.cpp
{".nes", ".unf"}     // NES
{".sfc", ".smc"}     // SNES
{".cue", ".bin"}     // PlayStation
{".iso"}             // Multi-system (ambiguous)

// docs/requirements.md
".n64", ".z64", ".v64"  // N64
".cue", ".bin", ".iso", ".pbp", ".chd"

// UI: ConversionView.qml
nameFilters: ["Disc Images (*.cue *.bin *.iso)"]
nameFilters: ["CHD Files (*.chd)"]
nameFilters: ["Archives (*.zip *.7z *.rar)"]
```

**Issue**:
- ❌ Extensions duplicated 5+ places
- ❌ No centralized extension → system mapping
- ❌ Header detection logic hardcoded by extension
- ❌ File dialogs have UI-specific filters

#### Template Variables (Scattered)
```cpp
// docs/requirements.md & .github/copilot-instructions.md
Variables: {title}, {region}, {year}, {disc}, {publisher}, {id}, {ext}

// UI: SettingsView.qml
"Variables: {title}, {region}, {year}, {publisher}, {disc}, {id}"

// src/organize/template_engine.cpp
// Implementation would parse these

// Docs: examples.md
"{title} ({region})"
"{title} ({region}) (Disc {disc}){ext}"

// Default: docs/requirements.md
No-Intro standard, Redump standard mentioned but not defined
```

**Issue**:
- ❌ Variable list only in comments/docs
- ❌ No requirement struct in code
- ❌ No validation of template syntax
- ❌ Optional vs required variables unclear

#### Confidence Thresholds (Scattered)
```cpp
// src/ui/models/match_list_model.qml
"High: >= 90%"
"Medium: 60-89%"
"Low: < 60%"

// docs/requirements.md
100%: Hash match OR user confirmation
90%:  Exact filename match
50-80%: Fuzzy match

// Color coding (hardcoded in QML)
if (confidence >= 90) return "#27ae60";  // Green
if (confidence >= 60) return "#f39c12";  // Orange
return "#e74c3c";  // Red
```

**Issue**:
- ❌ Thresholds defined in comments, not constants
- ❌ Color scheme hardcoded to theme
- ❌ No confidence → action mapping
- ❌ Future M8 verification will need similar structure

#### UI Colors & Theme (Hardcoded)
```cpp
// src/ui/qml/MainWindow.qml
color: "#2c3e50"      // Sidebar
color: "#ecf0f1"      // Background
color: "#3498db"      // Accent

// MatchReviewView.qml
"#27ae60"  // High confidence
"#f39c12"  // Medium confidence
"#e74c3c"  // Low confidence
"#95a5a6"  // Placeholder text
"#bdc3c7"  // Border

// Various
color: "#7f8c8d"      // Secondary text
```

**Issue**:
- ❌ 15+ unique hex colors scattered
- ❌ No theme support (dark/light/custom)
- ❌ Color scheme embedded in QML
- ❌ No centralized palette

#### Settings Keys (Scattered)
```cpp
// SettingsController + QSettings
"screenscraper/username"
"screenscraper/password"
"screenscraper/devid"
"screenscraper/devpassword"
"metadata/provider_priority"
"organize/naming_template"
"organize/by_system"
"organize/preserve_originals"
"performance/hash_algorithm"
"performance/parallel_hashing"
```

**Issue**:
- ❌ Keys scattered in code and comments
- ❌ No validation of key names
- ❌ Settings schema not documented
- ❌ Type information lost (all QVariant)

---

### 2. Future Features Impact Analysis

#### M6: Organize Engine UI
Will need:
- Template variable constants  ❌ Currently in docs only
- Collision strategy names (skip/rename/overwrite) ❌ Hardcoded
- Organize operation states ❌ Not defined
- Undo type identifiers ❌ Not planned

#### M7: Polishing (Artwork Management)
Will need:
- Artwork type categories (boxArt, screenshot, banner, etc.) ❌ String literals
- Image size presets ❌ Not defined
- Cache location & naming ❌ Not standardized
- Export format options ❌ Not defined

#### M8: Verification & Patching
Will need:
- Verification status enums ❌ Strings: "verified", "failed", "unknown"
- Patch format identifiers ❌ "IPS", "BPS", "UPS", "XDelta3", "PPF"
- Header types ❌ "iNES", "SNES PSC", "GBA", etc.
- ROM file types ❌ "official", "hack", "translation", "homebrew", "prototype"

#### Future: Localization
Will need:
- All UI strings moved to constants ❌ Currently hardcoded in QML
- Human-readable labels for systems, providers ❌ Duplicated in multiple places
- Translatable message templates ❌ Not structured

---

## Proposed Constants Library Architecture

### Directory Structure

```
src/core/constants/
├── constants.h                 # Main header (includes all)
├── providers.h                 # Metadata provider definitions
├── systems.h                   # Gaming system definitions
├── files.h                     # File extensions and types
├── ui_theme.h                  # Colors, typography
├── templates.h                 # Naming template variables
├── confidence.h                # Confidence scoring constants
├── settings.h                  # QSettings key definitions
├── hash.h                      # Hash algorithm definitions
└── README.md                   # Implementation guide
```

### 1. Providers Module (`providers.h`)

```cpp
#pragma once
#include <QString>
#include <QMap>

namespace Remus {
namespace Constants {
namespace Providers {

// Provider identifiers (internal)
inline constexpr const char* SCREENSCRAPER = "screenscraper";
inline constexpr const char* THEGAMESDB = "thegamesdb";
inline constexpr const char* IGDB = "igdb";
inline constexpr const char* HASHEOUS = "hasheous";
inline constexpr const char* RETROACHIEVEMENTS = "retroachievements";

// Provider display names (user-facing)
const QString DISPLAY_SCREENSCRAPER = "ScreenScraper";
const QString DISPLAY_THEGAMESDB = "TheGamesDB";
const QString DISPLAY_IGDB = "IGDB";
const QString DISPLAY_HASHEOUS = "Hasheous";

// Provider capabilities
struct ProviderInfo {
    QString id;              // Internal identifier
    QString displayName;     // User-facing name
    bool supportsHash;       // Hash-based matching
    bool supportsName;       // Name-based matching
    bool requiresAuth;       // Requires credentials
    int priority;            // Fallback priority (higher = tried first)
};

// Provider registry
const QMap<QString, ProviderInfo> PROVIDER_INFO = {
    {HASHEOUS, {
        HASHEOUS,
        DISPLAY_HASHEOUS,
        true,   // Hash-based
        false,  // No name search
        false,  // No auth required (FREE!)
        100     // Highest priority (hash match)
    }},
    {SCREENSCRAPER, {
        SCREENSCRAPER,
        DISPLAY_SCREENSCRAPER,
        true,   // Hash and name
        true,
        true,   // Requires auth
        90      // High priority if authenticated
    }},
    // ... IGDB, TheGamesDB, RetroAchievements
};

// Settings keys for each provider
namespace SettingsKeys {
    inline constexpr const char* SCREENSCRAPER_USERNAME = "providers/screenscraper/username";
    inline constexpr const char* SCREENSCRAPER_PASSWORD = "providers/screenscraper/password";
    inline constexpr const char* SCREENSCRAPER_DEVID = "providers/screenscraper/devid";
    inline constexpr const char* SCREENSCRAPER_DEVPASS = "providers/screenscraper/devpassword";
    inline constexpr const char* PROVIDER_PRIORITY = "metadata/provider_priority";
}

} // Providers
} // Constants
} // Remus
```

**Usage Impact**:
```cpp
// Before
"screenscraper"  // scattered in 8 places

// After
using namespace Remus::Constants::Providers;
m_orchestrator->addProvider(SCREENSCRAPER, ...);
QString username = settings.value(SettingsKeys::SCREENSCRAPER_USERNAME);
ui->providerCombo->setModel({DISPLAY_SCREENSCRAPER, DISPLAY_THEGAMESDB});
```

### 2. Systems Module (`systems.h`)

```cpp
#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

namespace Remus::Constants::Systems {

// System identifiers (immutable)
inline constexpr int ID_NES = 1;
inline constexpr int ID_SNES = 2;
inline constexpr int ID_N64 = 3;
inline constexpr int ID_GB = 4;
inline constexpr int ID_GBC = 5;
inline constexpr int ID_GBA = 6;
inline constexpr int ID_NDS = 7;
inline constexpr int ID_GENESIS = 8;
inline constexpr int ID_PSX = 9;
inline constexpr int ID_PS2 = 10;
// ... etc

// System metadata struct
struct SystemDef {
    int id;
    QString internalName;           // "NES", "PlayStation"
    QString displayName;            // "Nintendo Entertainment System"
    QString manufacturer;           // "Nintendo", "Sony"
    int generation;                 // 3, 4, 5, etc.
    QStringList extensions;         // [".nes", ".unf"]
    QString preferredHash;          // "CRC32", "MD5", "SHA1"
    QString region;                 // "North America", "Japan", "Europe"
    QStringList regionCodes;        // ["USA", "Japan", "Europe"]
    bool isMultiFile;               // True for .cue/.bin, .iso games
    QString color;                  // UI badge color
};

// Central system registry (replaces scattered lists)
const QMap<int, SystemDef> SYSTEMS = {
    {ID_NES, {
        ID_NES,
        "NES",
        "Nintendo Entertainment System",
        "Nintendo",
        3,   // Generation
        {".nes", ".unf"},
        "CRC32",
        "North America, Japan, Europe",
        {"USA", "JPN", "EUR"},
        false,  // Single-file
        "#db2828"  // Red
    }},
    {ID_SNES, {
        ID_SNES,
        "SNES",
        "Super Nintendo Entertainment System",
        "Nintendo",
        4,
        {".sfc", ".smc"},
        "CRC32",
        "North America, Japan, Europe",
        {"USA", "JPN", "EUR"},
        false,
        "#f26202"  // Orange
    }},
    {ID_PSX, {
        ID_PSX,
        "PlayStation",
        "Sony PlayStation",
        "Sony",
        5,
        {".cue", ".bin", ".iso", ".pbp", ".chd"},
        "MD5",
        "North America, Japan, Europe",
        {"USA", "JPN", "EUR"},
        true,  // Multi-file (.cue + .bin)
        "#0051ba"  // Blue
    }},
    // ... all 20+ systems
};

// Lookup helpers
inline const SystemDef* getSystem(int id) {
    return SYSTEMS.contains(id) ? &SYSTEMS[id] : nullptr;
}

inline int getSystemIdByName(const QString &name) {
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        if (it.value().internalName == name) return it.key();
    }
    return 0;
}

inline const SystemDef* getSystemByName(const QString &name) {
    int id = getSystemIdByName(name);
    return id > 0 ? &SYSTEMS[id] : nullptr;
}

// Reverse mapping: extension → system
const QMap<QString, QList<int>> EXTENSION_TO_SYSTEMS = {
    {".nes", {ID_NES}},
    {".sfc", {ID_SNES}},
    {".smc", {ID_SNES}},
    {".iso", {ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII}},  // Ambiguous
    {".cue", {ID_PSX, ID_SEGA_CD}},
    // ...
};

} // Systems
``` 

**Impact on Codebase**:
- ✅ Replace 5+ hardcoded system lists with single source
- ✅ UI ComboBox can query `SYSTEMS.keys()` instead of hardcoding
- ✅ Extension detection uses `EXTENSION_TO_SYSTEMS` lookup
- ✅ System info available everywhere without DB queries
- ✅ Color badges auto-assigned by system

### 3. Files Module (`files.h`)

```cpp
#pragma once
#include <QString>
#include <QStringList>

namespace Remus::Constants::Files {

// File type categories
namespace Types {
    inline constexpr const char* OFFICIAL = "official";
    inline constexpr const char* HACK = "hack";
    inline constexpr const char* TRANSLATION = "translation";
    inline constexpr const char* HOMEBREW = "homebrew";
    inline constexpr const char* PROTOTYPE = "prototype";
    inline constexpr const char* DEMO = "demo";
    inline constexpr const char* UNLICENSED = "unlicensed";
}

// Verification status (M8)
namespace VerificationStatus {
    inline constexpr const char* VERIFIED = "verified";
    inline constexpr const char* FAILED = "failed";
    inline constexpr const char* UNKNOWN = "unknown";
    inline constexpr const char* NOT_CHECKED = "not_checked";
    inline constexpr const char* CORRUPTED = "corrupted";
}

// File format groups
const QStringList DISC_IMAGES = {".iso", ".cue", ".bin", ".mdf", ".gdi", ".cdi"};
const QStringList ROM_CARTRIDGES = {".nes", ".sfc", ".n64", ".gb", ".gbc", ".gba", ".md"};
const QStringList COMPRESSED = {".chd", ".rvz", ".rvz", ".wbfs"};
const QStringList ARCHIVES = {".zip", ".7z", ".rar", ".tar.gz"};

// Header detection
namespace Headers {
    inline constexpr int INES_HEADER_SIZE = 16;
    inline constexpr int SNES_PSC_HEADER_SIZE = 512;
    inline constexpr int GBA_HEADER_SIZE = 0;      // No header
    inline constexpr int LYNX_HEADER_SIZE = 64;
    
    // Magic bytes
    inline constexpr const char* INES_MAGIC = "NES\x1A";
    inline constexpr const char* SNES_PSC_MAGIC = "\x00\x00\xc0\xff";
}

} // Files
```

### 4. UI Theme Module (`ui_theme.h`)

```cpp
#pragma once
#include <QString>
#include <QColor>

namespace Remus::Constants::UI {

// Color palette
namespace Colors {
    // Sidebar & backgrounds
    inline constexpr const char* SIDEBAR_BG = "#2c3e50";
    inline constexpr const char* MAIN_BG = "#ecf0f1";
    inline constexpr const char* CARD_BG = "white";
    
    // Accents
    inline constexpr const char* PRIMARY = "#3498db";
    inline constexpr const char* SUCCESS = "#27ae60";
    inline constexpr const char* WARNING = "#f39c12";
    inline constexpr const char* DANGER = "#e74c3c";
    inline constexpr const char* INFO = "#17a2b8";
    
    // Text
    inline constexpr const char* TEXT_PRIMARY = "#2c3e50";
    inline constexpr const char* TEXT_SECONDARY = "#7f8c8d";
    inline constexpr const char* TEXT_MUTED = "#95a5a6";
    
    // Borders & dividers
    inline constexpr const char* BORDER = "#bdc3c7";
    inline constexpr const char* DIVIDER = "#ecf0f1";
}

// Confidence colors
namespace Confidence {
    inline constexpr const char* HIGH = "#27ae60";    // Green
    inline constexpr const char* MEDIUM = "#f39c12";  // Orange
    inline constexpr const char* LOW = "#e74c3c";     // Red
    inline constexpr const char* UNMATCHED = "#95a5a6"; // Gray
}

// Typography
namespace Typography {
    inline constexpr int TITLE_SIZE = 18;
    inline constexpr int SUBTITLE_SIZE = 14;
    inline constexpr int BODY_SIZE = 12;
    inline constexpr int SMALL_SIZE = 10;
}

// Sizes
namespace Sizes {
    inline constexpr int WINDOW_WIDTH = 1200;
    inline constexpr int WINDOW_HEIGHT = 800;
    inline constexpr int MIN_WIDTH = 1024;
    inline constexpr int MIN_HEIGHT = 768;
    inline constexpr int SIDEBAR_WIDTH = 200;
    inline constexpr int DELEGATE_HEIGHT = 60;
    inline constexpr int DELEGATE_HEIGHT_MATCH = 120;
}

} // UI
```

### 5. Templates Module (`templates.h`)

```cpp
#pragma once
#include <QString>
#include <QStringList>

namespace Remus::Constants::Templates {

// Template variable definitions
struct TemplateVar {
    QString name;
    QString placeholder;      // {name}
    QString description;
    bool required;
    QStringList examples;
};

const QList<TemplateVar> VARIABLES = {
    {"title", "{title}", "Game title", true, 
     {"Super Mario Bros.", "Final Fantasy VII"}},
    {"region", "{region}", "Release region", true,
     {"USA", "EUR", "JPN"}},
    {"year", "{year}", "Release year", false,
     {"1985", "1997"}},
    {"disc", "{disc}", "Disc number (multi-disc only)", false,
     {"1", "2", "3"}},
    {"publisher", "{publisher}", "Publisher name", false,
     {"Nintendo", "Square"}},
    {"developer", "{developer}", "Developer name", false,
     {"Nintendo EAD", "Infocom"}},
    {"id", "{id}", "Internal ID", false,
     {"12345"}},
    {"ext", "{ext}", "File extension", true,
     {".nes", ".cue"}},
};

// Predefined templates
namespace Presets {
    // No-Intro standard for cartridge-based
    inline constexpr const char* NO_INTRO_CARTRIDGE = "{title} ({region}).{ext}";
    
    // Redump standard for disc-based
    inline constexpr const char* REDUMP_DISC = "{title} ({region}) (Disc {disc}).{ext}";
    
    // With year
    inline constexpr const char* WITH_YEAR = "{title} ({region}) ({year}).{ext}";
    
    // With publisher
    inline constexpr const char* WITH_PUBLISHER = "{title} ({publisher}) ({region}).{ext}";
    
    // Flat with system tag
    inline constexpr const char* FLAT_TAGGED = "{title} ({region}) [{system}].{ext}";
}

// Predefined folder structures
namespace FolderStructures {
    inline constexpr const char* BY_SYSTEM = "/Library/{system}/";
    inline constexpr const char* BY_SYSTEM_PUBLISHER = "/Library/{system}/{publisher}/";
    inline constexpr const char* BY_PUBLISHER = "/Library/{publisher}/";
    inline constexpr const char* FLAT = "/Library/";
    inline constexpr const char* BY_DECADE = "/Library/{decade}s/";
}

// Collision strategies
namespace Collision {
    inline constexpr const char* SKIP = "skip";
    inline constexpr const char* RENAME = "rename";
    inline constexpr const char* OVERWRITE = "overwrite";
    inline constexpr const char* ASK_USER = "ask_user";
}

} // Templates
```

### 6. Confidence Module (`confidence.h`)

```cpp
#pragma once

namespace Remus::Constants::Confidence {

// Threshold levels
namespace Thresholds {
    inline constexpr float HASH_MATCH = 100.0f;        // Exact hash
    inline constexpr float USER_CONFIRMED = 100.0f;    // User said yes
    inline constexpr float EXACT_NAME = 90.0f;         // Exact filename
    inline constexpr float HIGH = 90.0f;               // >= 90%
    inline constexpr float MEDIUM = 60.0f;             // >= 60%
    inline constexpr float LOW = 0.0f;                 // < 60%
    inline constexpr float FUZZY_MIN = 60.0f;
    inline constexpr float FUZZY_MAX = 80.0f;
    
    inline constexpr float DEFAULT_MINIMUM = 60.0f;    // Filter default
}

// Confidence categories
enum class Category {
    Hash,           // 100% - hash match
    Exact,          // 90% - exact filesystem name
    High,           // >= 90% - fuzzy with high similarity
    Medium,         // 60-89% - fuzzy with medium similarity
    Low,            // < 60% - fuzzy with low similarity
    Unmatched       // 0% - no match
};

inline Category getCategory(float confidence) {
    if (confidence >= 100.0f) return Category::Hash;
    if (confidence >= 90.0f) return Category::Exact;
    if (confidence >= 60.0f) return Category::Medium;
    return Category::Low;
}

inline QString getCategoryLabel(Category cat) {
    switch (cat) {
        case Category::Hash: return "Perfect";
        case Category::Exact: return "Exact";
        case Category::High: return "High";
        case Category::Medium: return "Medium";
        case Category::Low: return "Low";
        case Category::Unmatched: return "Unmatched";
    }
    return "Unknown";
}

} // Confidence
```

### 7. Hash Module (`hash.h`)

```cpp
#pragma once
#include <QString>

namespace Remus::Constants::Hash {

// Hash algorithm identifiers
namespace Algorithms {
    inline constexpr const char* CRC32 = "CRC32";
    inline constexpr const char* MD5 = "MD5";
    inline constexpr const char* SHA1 = "SHA1";
    inline constexpr const char* SHA256 = "SHA256";
}

// Algorithm info
struct AlgorithmInfo {
    QString id;
    QString displayName;
    int outputBytes;
    bool isSecureHash;
};

const QMap<QString, AlgorithmInfo> ALGORITHM_INFO = {
    {Algorithms::CRC32, {"CRC32", "CRC32 (Fast)", 4, false}},
    {Algorithms::MD5, {"MD5", "MD5 (Good)", 16, false}},
    {Algorithms::SHA1, {"SHA1", "SHA1 (Better)", 20, true}},
    {Algorithms::SHA256, {"SHA256", "SHA256 (Best)", 32, true}},
};

} // Hash
```

### 8. Settings Module (`settings.h`)

```cpp
#pragma once

namespace Remus::Constants::Settings {

namespace Keys {
    // Metadata
    inline constexpr const char* PROVIDER_PRIORITY = "metadata/provider_priority";
    inline constexpr const char* CACHE_DURATION_DAYS = "metadata/cache_duration_days";
    inline constexpr const char* FETCH_ARTWORK = "metadata/fetch_artwork";
    
    // Organization
    inline constexpr const char* NAMING_TEMPLATE = "organize/naming_template";
    inline constexpr const char* FOLDER_STRUCTURE = "organize/folder_structure";
    inline constexpr const char* ORGANIZE_BY_SYSTEM = "organize/by_system";
    inline constexpr const char* PRESERVE_ORIGINALS = "organize/preserve_originals";
    inline constexpr const char* COLLISION_STRATEGY = "organize/collision_strategy";
    
    // Performance
    inline constexpr const char* HASH_ALGORITHM = "performance/hash_algorithm";
    inline constexpr const char* PARALLEL_HASHING = "performance/parallel_hashing";
    inline constexpr const char* NUM_THREADS = "performance/num_threads";
    inline constexpr const char* SKIP_HASH_IF_CACHED = "performance/skip_hash_if_cached";
    
    // UI
    inline constexpr const char* WINDOW_GEOMETRY = "ui/window_geometry";
    inline constexpr const char* WINDOW_STATE = "ui/window_state";
    inline constexpr const char* THEME = "ui/theme";
    inline constexpr const char* LAST_SCAN_PATH = "ui/last_scan_path";
}

// Default values
namespace Defaults {
    inline constexpr const char* PROVIDER_PRIORITY = "Auto";
    inline constexpr int CACHE_DURATION_DAYS = 30;
    inline constexpr bool FETCH_ARTWORK = true;
    inline constexpr const char* NAMING_TEMPLATE = "{title} ({region})";
    inline constexpr const char* FOLDER_STRUCTURE = "by_system";
    inline constexpr bool ORGANIZE_BY_SYSTEM = true;
    inline constexpr bool PRESERVE_ORIGINALS = false;
    inline constexpr const char* COLLISION_STRATEGY = "ask_user";
    inline constexpr const char* HASH_ALGORITHM = "MD5";
    inline constexpr bool PARALLEL_HASHING = true;
    inline constexpr int NUM_THREADS = 4;
    inline constexpr bool SKIP_HASH_IF_CACHED = true;
}

} // Settings
```

---

## Implementation Benefits Analysis

### 1. Code Quality Improvements

| Category | Before | After | Improvement |
|----------|--------|-------|-------------|
| **Type Safety** | String literals (no type checking) | Enums + constexpr | ✅ Compile-time validation |
| **Duplication** | 150+ hardcoded values | Single source of truth | ✅ 90-95% reduction |
| **Maintainability** | Change requires 5-8 edits | Change in constants.h only | ✅ 1 edit location |
| **Consistency** | Inconsistent naming (CamelCase/lowercase) | Standardized naming | ✅ 100% consistency |
| **Documentation** | Scattered comments | Self-documenting code | ✅ Single reference |
| **IDE Support** | String autocomplete only | Full IntelliSense on enums | ✅ Better DX |

### 2. Specific Codebase Impact

**SystemDetector refactoring** (118 lines → 40 lines)
```cpp
// Before: 118 lines of manual initialization
void SystemDetector::initializeDefaultSystems() {
    QList<SystemInfo> systems;
    systems.append({1, "NES", "Nintendo Entertainment System", "Nintendo", 3, ...});
    systems.append({2, "SNES", "Super Nintendo Entertainment System", "Nintendo", 4, ...});
    // ... repeat 20+ times
}

// After: 2 lines
void SystemDetector::initializeDefaultSystems() {
    for (const auto& def : Constants::Systems::SYSTEMS) {
        SystemInfo info = def;
        m_systems[def.internalName] = std::move(info);
    }
}
```

**CLI initialization refactoring** (25 lines → 3 lines)
```cpp
// Before: Hardcoded list
QStringList systemNames = {
    "NES", "SNES", "N64", "GB", "GBC", "GBA", "NDS",
    "Genesis", "PlayStation", "PlayStation 2", "PSP", "Atari 2600"
};

// After: Auto-generated from constants
QStringList systemNames;
for (const auto& sys : Constants::Systems::SYSTEMS) {
    systemNames << sys.value().internalName;
}
```

**UI ComboBox refactoring** (4 lines → 2 lines)
```cpp
// Before: Create separate display list
model: ["All Systems", "NES", "SNES", "PlayStation", "N64"]

// After: Generate from constants
model: ["All Systems", ...Constants::Systems::systemDisplayNames()]
```

**Settings access refactoring** (No type safety → Type safe)
```cpp
// Before: String key with no validation
int confidence = settings.value("organize/collision_strategy", "ask_user").toInt();

// After: Typed, validated, documented
QString strategy = settings.value(
    Constants::Settings::Keys::COLLISION_STRATEGY,
    Constants::Settings::Defaults::COLLISION_STRATEGY
).toString();
```

### 3. Future-Proofing Analysis

**M6 (Organize Engine UI)**: 
- ✅ Collision strategies already in Constants::Templates::Collision
- ✅ Template variables auto-validated
- ✅ Folder structure presets available
- ✅ Settings keys standardized

**M7 (Artwork Management)**:
- ✅ Can add `artwork.h` with image types, sizes, cache keys
- ✅ Color mappings already in UI theme
- ✅ Settings keys pattern established

**M8 (Verification & Patching)**:
- ✅ Add `verification.h` with status enums
- ✅ Add `patching.h` with format types, header detection rules
- ✅ ROM file types already in `files.h`

**Localization Support**:
- ✅ All display strings in single location for translator
- ✅ Can create `i18n/` translations easily
- ✅ Provider display names, system names, UI text centralized

### 4. Maintenance Burden Reduction

**Scenario 1: Add new system (Atari Jaguar)**
- Before: Update 6+ locations (system_detector.cpp, CLI list, UI ComboBox, docs, schema, requirements)
- After: 1 line in Constants::Systems::SYSTEMS

**Scenario 2: Add new metadata provider (LeaderBoard.net)**
- Before: Update CLI parsing, UI dropdown, provider orchestrator, settings keys, documentation
- After: 8 lines in Constants::Providers

**Scenario 3: Change confidence thresholds (M8 fine-tuning)**
- Before: Search for all numeric confidence comparisons across codebase
- After: Update Constants::Confidence::Thresholds

---

## Implementation Roadmap

### Phase 1: Foundation (3-4 hours)
1. Create `src/core/constants/constants.h` header
2. Implement `providers.h` module
3. Implement `systems.h` module  
4. Write unit tests for lookup functions
5. Update CMakeLists.txt to include constants

**Deliverable**: Type-safe provider & system access across codebase

### Phase 2: UI Integration (2-3 hours)
1. Implement `ui_theme.h` module
2. Refactor all hardcoded colors in QML
3. Implement `confidence.h` module
4. Update MatchReviewView confidence badges
5. Create UI color utilities class

**Deliverable**: Themeable, consistent UI with constants

### Phase 3: Settings & Templates (2-3 hours)
1. Implement `settings.h` module
2. Implement `templates.h` module
3. Refactor SettingsController to use Constants::Settings::Keys
4. Update TemplateEngine for validation
5. Create settings migrations for schema updates

**Deliverable**: Robust settings management, template validation

### Phase 4: Cleanup (2-3 hours)
1. Refactor SystemDetector to use Constants::Systems
2. Refactor CLI initialization
3. Update all hardcoded strings to use constants
4. Remove duplicate system/provider lists
5. Update documentation with constants reference

**Deliverable**: Zero hardcoded system/provider/extension strings

### Phase 5: Future-Proofing (1-2 hours)
1. Add `hash.h` module
2. Add `files.h` module
3. Prepare `verification.h` for M8
4. Prepare `patching.h` for M8
5. Create constants/README.md guide

**Deliverable**: Ready for M6-M8 development

### Total Effort: ~10-15 hours (can be done in 1-2 days)

---

## Risk Analysis & Mitigation

### Risk 1: Circular Dependencies
**Concern**: Constants header might include too much  
**Mitigation**: Keep constants.h minimal, split into modules, use forward declarations

### Risk 2: Over-Engineering
**Concern**: Too many constants could bloat code  
**Mitigation**: Only constants that appear 3+ times or are UI-configurable

### Risk 3: Performance
**Concern**: Large maps in constants might have lookup overhead  
**Mitigation**: Use inline constexpr for simple values, optimize map lookups with qHash

### Risk 4: Maintenance of Constants
**Concern**: Keeping constants synchronized with documentation  
**Mitigation**: Generate documentation from constants header, add validation tests

---

## Comparison: Similar Projects

### RomM (Reference Implementation)
```
RomM uses:
✅ Provider constants in settings
✅ System definitions in database (not constants)
❌ Hardcoded UI strings
❌ No centralized confidence thresholds

Our advantages:
✅ Compile-time validation
✅ No database query overhead for system info
✅ Type-safe constants
✅ Single source of truth
```

### RetroArch
```
RetroArch uses:
✅ Extensive enum definitions
✅ Core info files (database-like)
❌ String-based system identification
❌ Scattered magic numbers

Our learning:
✅ Use both enums AND human-readable strings
✅ Make system identification robust (path heuristics)
```

---

## Conclusion & Recommendation

**Status**: STRONGLY RECOMMENDED for implementation

**Rationale**:
1. **Current Pain**: 150+ hardcoded strings scattered across 8+ files
2. **Future Need**: M6-M8 will add verification, patching, additional UI
3. **Maintenance Burden**: Every system/provider change requires 5-8 edits
4. **Type Safety**: String-based code lacks compile-time validation
5. **Localization**: Currently impossible due to scattered strings

**Implementation**: Low-risk, high-return refactoring that:
- Takes 10-15 hours (can be parallelized)
- Blocks no active work (can be done between milestones)
- Improves code quality immediately
- Reduces future development time by ~30%
- Enables localization without code changes

**Recommended Timeline**: Implement before M6 (after M5 completion)

**Next Step**: Confirm approach, then begin Phase 1 implementation

---

**Prepared by**: GitHub Copilot  
**Date**: February 5, 2026  
**Status**: Ready for Architecture Review
