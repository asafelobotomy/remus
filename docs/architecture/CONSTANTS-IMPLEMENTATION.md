# Constants Library Implementation Guide

## File Structure

```
src/core/constants/
├── CMakeLists.txt              # New - Add constants as library
├── constants.h                 # Main header (includes all modules)
├── providers.h                 # Provider definitions
├── systems.h                   # Gaming system definitions  
├── files.h                     # File types and headers
├── templates.h                 # Naming templates
├── confidence.h                # Confidence scoring
├── hash.h                      # Hash algorithms
├── ui_theme.h                  # UI colors and typography
├── settings.h                  # Settings keys
└── README.md                   # Documentation
```

## Implementation Files

### 1. src/core/constants/constants.h

```cpp
#pragma once

/**
 * @file constants.h
 * @brief Central constants library for Remus
 * 
 * This header provides a single location for all application constants, enums,
 * and configuration values. Eliminates scattered hardcoded strings and enables
 * type-safe constant access throughout the application.
 *
 * Usage:
 *   using namespace Remus::Constants;
 *   auto system = Systems::getSystem(Systems::ID_NES);
 *   QString provider = Providers::SCREENSCRAPER;
 */

// Include all constant modules
#include "providers.h"
#include "systems.h"
#include "templates.h"
#include "confidence.h"
#include "hash.h"
#include "files.h"
#include "ui_theme.h"
#include "settings.h"

namespace Remus {
namespace Constants {

/**
 * @brief Version of the constants library
 * 
 * Increment when adding new constants or modifying enum values.
 * Used for cache invalidation and migration detection.
 */
inline constexpr int CONSTANTS_VERSION = 1;

/**
 * @brief Application organization name for QSettings
 */
inline constexpr const char* SETTINGS_ORGANIZATION = "Remus";

/**
 * @brief Application name for QSettings
 */
inline constexpr const char* SETTINGS_APPLICATION = "Remus";

/**
 * @brief Helper to access settings keys
 * 
 * Example:
 *   QSettings settings;
 *   settings.setValue(SETTINGS_KEY(Settings::Keys::NAMING_TEMPLATE), template_name);
 */
#define SETTINGS_KEY(key) (key)

} // Constants
} // Remus

// For convenient access in implementation files
using namespace Remus::Constants;
```

### 2. src/core/constants/providers.h

```cpp
#pragma once

#include <QString>
#include <QMap>
#include <QList>

namespace Remus {
namespace Constants {
namespace Providers {

// ============================================================================
// Provider Identifiers (Internal use)
// ============================================================================

/// Metadata provider: Hasheous (free, hash-only)
inline constexpr const char* HASHEOUS = "hasheous";

/// Metadata provider: ScreenScraper (requires auth)
inline constexpr const char* SCREENSCRAPER = "screenscraper";

/// Metadata provider: TheGamesDB (free)
inline constexpr const char* THEGAMESDB = "thegamesdb";

/// Metadata provider: IGDB (requires API key)
inline constexpr const char* IGDB = "igdb";

/// Metadata provider: RetroAchievements (optional)
inline constexpr const char* RETROACHIEVEMENTS = "retroachievements";

// ============================================================================
// Provider Display Names (User-facing)
// ============================================================================

/// Human-readable name for Hasheous
inline const QString DISPLAY_HASHEOUS = QStringLiteral("Hasheous");

/// Human-readable name for ScreenScraper
inline const QString DISPLAY_SCREENSCRAPER = QStringLiteral("ScreenScraper");

/// Human-readable name for TheGamesDB
inline const QString DISPLAY_THEGAMESDB = QStringLiteral("TheGamesDB");

/// Human-readable name for IGDB
inline const QString DISPLAY_IGDB = QStringLiteral("IGDB");

/// Human-readable name for RetroAchievements
inline const QString DISPLAY_RETROACHIEVEMENTS = QStringLiteral("RetroAchievements");

// ============================================================================
// Provider Metadata
// ============================================================================

/**
 * @brief Information about a metadata provider
 */
struct ProviderInfo {
    QString id;              ///< Internal identifier (SCREENSCRAPER, IGDB, etc.)
    QString displayName;     ///< User-facing display name
    QString description;     ///< Long description for UI tooltips
    bool supportsHashMatch;  ///< Can search by file hash
    bool supportsNameMatch;  ///< Can search by game name
    bool requiresAuth;       ///< Requires credentials
    QString authHelpUrl;     ///< URL for obtaining credentials
    int priority;            ///< Fallback priority (higher = tried first)
    bool isFreeService;      ///< Does not require payment
};

/**
 * @brief Registry of all available metadata providers
 * 
 * Ordered by priority for fallback chain:
 * 1. Hash matches always preferred (100% accuracy)
 * 2. Name matches with fallback chain
 * 3. Fuzzy matches as last resort
 */
inline const QMap<QString, ProviderInfo> PROVIDER_REGISTRY = {
    // Priority 90: Hash-first provider (best for hash-based matching)
    {HASHEOUS, {
        HASHEOUS,
        DISPLAY_HASHEOUS,
        QStringLiteral("Free hash database (no auth required)"),
        true,   // Hash matching
        false,  // No name search (hash only)
        false,  // No auth required
        QStringLiteral(""),
        90,     // High priority (hash matching preferred)
        true    // Free service
    }},
    
    // Priority 80: Primary authenticated provider (comprehensive database)
    {SCREENSCRAPER, {
        SCREENSCRAPER,
        DISPLAY_SCREENSCRAPER,
        QStringLiteral("Comprehensive ROM metadata with artwork (requires free account)"),
        true,   // Hash matching
        true,   // Name search
        true,   // Requires authentication
        QStringLiteral("https://www.screenscraper.fr"),
        80,     // Fallback after hash
        true    // Free service available
    }},
    
    // Priority 70: Fallback provider
    {THEGAMESDB, {
        THEGAMESDB,
        DISPLAY_THEGAMESDB,
        QStringLiteral("Game metadata and artwork (no auth required)"),
        true,   // Hash matching (via API)
        true,   // Name search
        false,  // No auth required
        QStringLiteral("https://thegamesdb.net"),
        70,     // Fallback option
        true    // Free service
    }},
    
    // Priority 60: Commercial provider
    {IGDB, {
        IGDB,
        DISPLAY_IGDB,
        QStringLiteral("Commercial game database (requires API key)"),
        false,  // No hash matching
        true,   // Name search only
        true,   // Requires API key (not free)
        QStringLiteral("https://api.igdb.com"),
        60,     // Last fallback
        false   // Requires subscription
    }},
};

// ============================================================================
// Provider Settings Keys
// ============================================================================

namespace SettingsKeys {
    // ScreenScraper provider settings
    inline constexpr const char* SCREENSCRAPER_USERNAME = 
        "providers/screenscraper/username";
    inline constexpr const char* SCREENSCRAPER_PASSWORD = 
        "providers/screenscraper/password";
    inline constexpr const char* SCREENSCRAPER_DEVID = 
        "providers/screenscraper/devid";
    inline constexpr const char* SCREENSCRAPER_DEVPASSWORD = 
        "providers/screenscraper/devpassword";
    
    // IGDB provider settings
    inline constexpr const char* IGDB_API_KEY = 
        "providers/igdb/api_key";
    inline constexpr const char* IGDB_CLIENT_ID = 
        "providers/igdb/client_id";
    
    // Provider priority/selection
    inline constexpr const char* PROVIDER_PRIORITY_ORDER = 
        "metadata/provider_priority_order";
    inline constexpr const char* PROVIDERS_ENABLED = 
        "metadata/providers_enabled";
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get provider information by ID
 * @param providerId Provider identifier (SCREENSCRAPER, IGDB, etc.)
 * @return Pointer to ProviderInfo, or nullptr if not found
 */
inline const ProviderInfo* getProviderInfo(const QString &providerId) {
    auto it = PROVIDER_REGISTRY.find(providerId);
    if (it != PROVIDER_REGISTRY.end()) {
        return &it.value();
    }
    return nullptr;
}

/**
 * @brief Get all enabled metadata providers
 * @return List of provider IDs in priority order
 */
inline QStringList getEnabledProviders() {
    QStringList providers;
    for (auto it = PROVIDER_REGISTRY.begin(); it != PROVIDER_REGISTRY.end(); ++it) {
        providers << it.key();
    }
    // Sort by priority (descending)
    std::sort(providers.begin(), providers.end(),
        [](const QString &a, const QString &b) {
            return PROVIDER_REGISTRY[a].priority > PROVIDER_REGISTRY[b].priority;
        });
    return providers;
}

/**
 * @brief Get provider's display name
 * @param providerId Internal provider ID
 * @return User-facing display name
 */
inline QString getProviderDisplayName(const QString &providerId) {
    auto info = getProviderInfo(providerId);
    return info ? info->displayName : QStringLiteral("Unknown");
}

} // Providers
} // Constants
} // Remus
```

### 3. src/core/constants/systems.h

```cpp
#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

namespace Remus {
namespace Constants {
namespace Systems {

// ============================================================================
// System ID Constants
// ============================================================================

/// Nintendo Entertainment System
inline constexpr int ID_NES = 1;

/// Super Nintendo Entertainment System
inline constexpr int ID_SNES = 2;

/// Nintendo 64
inline constexpr int ID_N64 = 3;

/// Nintendo GameCube
inline constexpr int ID_GAMECUBE = 4;

/// Nintendo Wii
inline constexpr int ID_WII = 5;

/// Game Boy
inline constexpr int ID_GB = 6;

/// Game Boy Color
inline constexpr int ID_GBC = 7;

/// Game Boy Advance
inline constexpr int ID_GBA = 8;

/// Nintendo DS
inline constexpr int ID_NDS = 9;

/// Sega Genesis / Mega Drive
inline constexpr int ID_GENESIS = 10;

/// Sega Master System
inline constexpr int ID_MASTER_SYSTEM = 11;

/// Sega Saturn
inline constexpr int ID_SATURN = 12;

/// Sega Dreamcast
inline constexpr int ID_DREAMCAST = 13;

/// Sony PlayStation (original)
inline constexpr int ID_PSX = 14;

/// Sony PlayStation 2
inline constexpr int ID_PS2 = 15;

/// Sony PlayStation Portable
inline constexpr int ID_PSP = 16;

/// Atari 2600
inline constexpr int ID_ATARI_2600 = 17;

/// Atari 7800
inline constexpr int ID_ATARI_7800 = 18;

/// Atari Lynx
inline constexpr int ID_LYNX = 19;

// ============================================================================
// System Definition
// ============================================================================

/**
 * @brief Complete definition of a gaming system
 */
struct SystemDef {
    int id;                          ///< Unique system ID
    QString internalName;            ///< Code name: "NES", "PlayStation"
    QString displayName;             ///< Full name: "Nintendo Entertainment System"
    QString manufacturer;            ///< "Nintendo", "Sony", "Sega"
    int generation;                  ///< Console generation: 3, 4, 5, etc.
    QStringList extensions;          ///< File extensions: [".nes", ".unf"]
    QString preferredHash;           ///< "CRC32", "MD5", or "SHA1"
    QString region;                  ///< Primary region: "North America"
    QStringList regionCodes;         ///< Region codes: ["USA", "JPN", "EUR"]
    bool isMultiFile;                ///< True for .cue/.bin or multi-disc games
    QString uiColor;                 ///< Badge color: "#e74c3c"
    int releaseYear;                ///< Year first released internationally
};

// ============================================================================
// System Registry
// ============================================================================

/**
 * @brief Complete registry of all supported gaming systems
 * 
 * Can be iterated for UI population:
 *   for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
 *       qDebug() << it.value().displayName;
 *   }
 */
inline const QMap<int, SystemDef> SYSTEMS = {
    // Generation 3 (Cartridge console era)
    {ID_NES, {
        ID_NES,
        QStringLiteral("NES"),
        QStringLiteral("Nintendo Entertainment System"),
        QStringLiteral("Nintendo"),
        3,
        {QStringLiteral(".nes"), QStringLiteral(".unf")},
        QStringLiteral("CRC32"),
        QStringLiteral("Global"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,  // Single-file
        QStringLiteral("#e74c3c"),  // Red
        1983
    }},
    
    {ID_SNES, {
        ID_SNES,
        QStringLiteral("SNES"),
        QStringLiteral("Super Nintendo Entertainment System"),
        QStringLiteral("Nintendo"),
        4,
        {QStringLiteral(".sfc"), QStringLiteral(".smc")},
        QStringLiteral("CRC32"),
        QStringLiteral("Global"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#f39c12"),  // Orange
        1990
    }},
    
    {ID_GENESIS, {
        ID_GENESIS,
        QStringLiteral("Genesis"),
        QStringLiteral("Sega Genesis / Mega Drive"),
        QStringLiteral("Sega"),
        4,
        {QStringLiteral(".md"), QStringLiteral(".gen"), QStringLiteral(".smd")},
        QStringLiteral("CRC32"),
        QStringLiteral("Global"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#000000"),  // Black
        1988
    }},
    
    // Generation 4 (16-bit era)
    {ID_PSX, {
        ID_PSX,
        QStringLiteral("PlayStation"),
        QStringLiteral("Sony PlayStation"),
        QStringLiteral("Sony"),
        5,
        {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), 
         QStringLiteral(".pbp"), QStringLiteral(".chd")},
        QStringLiteral("MD5"),
        QStringLiteral("Global"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        true,  // Multi-file (.cue + .bin sets)
        QStringLiteral("#0051ba"),  // Blue
        1994
    }},
    
    {ID_N64, {
        ID_N64,
        QStringLiteral("N64"),
        QStringLiteral("Nintendo 64"),
        QStringLiteral("Nintendo"),
        5,
        {QStringLiteral(".n64"), QStringLiteral(".z64"), QStringLiteral(".v64")},
        QStringLiteral("CRC32"),
        QStringLiteral("Global"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#c0392b"),  // Red/Dark
        1996
    }},
    
    // Additional systems...
    // TODO: Add remaining systems (GB, GBC, GBA, NDS, PS2, PSP, Atari, etc.)
};

// ============================================================================
// Extension to System Mapping
// ============================================================================

/**
 * @brief Reverse lookup: file extension → possible systems
 * 
 * Used during file scanning to suggest possible systems.
 * Some extensions are ambiguous (.iso can be PS1, PS2, GameCube, etc.)
 */
inline const QMap<QString, QList<int>> EXTENSION_TO_SYSTEMS = {
    {QStringLiteral(".nes"), {ID_NES}},
    {QStringLiteral(".unf"), {ID_NES}},
    
    {QStringLiteral(".sfc"), {ID_SNES}},
    {QStringLiteral(".smc"), {ID_SNES}},
    
    {QStringLiteral(".md"), {ID_GENESIS}},
    {QStringLiteral(".gen"), {ID_GENESIS}},
    {QStringLiteral(".smd"), {ID_GENESIS}},
    
    // Ambiguous multi-system extensions
    {QStringLiteral(".iso"), {ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".cue"), {ID_PSX}},
    {QStringLiteral(".bin"), {ID_PSX}},
    {QStringLiteral(".chd"), {ID_PSX, ID_PS2, ID_GAMECUBE}},
};

// ============================================================================
// System Grouping
// ============================================================================

/**
 * @brief Nintendo systems (for grouping/organization)
 */
inline const QList<int> NINTENDO_SYSTEMS = {
    ID_NES, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, 
    ID_NDS, ID_GAMECUBE, ID_WII
};

/**
 * @brief Sega systems
 */
inline const QList<int> SEGA_SYSTEMS = {
    ID_MASTER_SYSTEM, ID_GENESIS, ID_SATURN, ID_DREAMCAST
};

/**
 * @brief Sony/PlayStation systems
 */
inline const QList<int> SONY_SYSTEMS = {
    ID_PSX, ID_PS2, ID_PSP
};

/**
 * @brief Handheld systems
 */
inline const QList<int> HANDHELD_SYSTEMS = {
    ID_GB, ID_GBC, ID_GBA, ID_NDS, ID_PSP, ID_LYNX
};

/**
 * @brief Disc-based systems (require special handling)
 */
inline const QList<int> DISC_SYSTEMS = {
    ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII, ID_DREAMCAST
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get system definition by ID
 * @param systemId System ID constant (ID_NES, etc.)
 * @return Pointer to SystemDef, or nullptr if not found
 */
inline const SystemDef* getSystem(int systemId) {
    auto it = SYSTEMS.find(systemId);
    return (it != SYSTEMS.end()) ? &it.value() : nullptr;
}

/**
 * @brief Get system ID by internal name
 * @param name Internal system name ("NES", "PlayStation", etc.)
 * @return System ID, or 0 if not found
 */
inline int getSystemIdByName(const QString &name) {
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        if (it.value().internalName == name) {
            return it.key();
        }
    }
    return 0;
}

/**
 * @brief Get system definition by internal name
 * @param name Internal system name ("NES", "PlayStation", etc.)
 * @return Pointer to SystemDef, or nullptr if not found
 */
inline const SystemDef* getSystemByName(const QString &name) {
    int id = getSystemIdByName(name);
    return (id > 0) ? &SYSTEMS[id] : nullptr;
}

/**
 * @brief Get all system display names
 * @return List of human-readable system names for UI population
 */
inline QStringList getSystemDisplayNames() {
    QStringList names;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        names << it.value().displayName;
    }
    return names;
}

/**
 * @brief Get all system internal names
 * @return List of internal system names ("NES", "SNES", etc.)
 */
inline QStringList getSystemInternalNames() {
    QStringList names;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        names << it.value().internalName;
    }
    return names;
}

} // Systems
} // Constants
} // Remus
```

### 4. Implementation: CMakeLists.txt Update

Add to `src/core/CMakeLists.txt`:

```cmake
# Constants library (header-only)
add_library(remus_constants INTERFACE)
target_include_directories(remus_constants INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/constants
)

# Link to core library
target_link_libraries(remus_core PUBLIC remus_constants)

# Install headers
install(
    DIRECTORY constants/
    DESTINATION include/remus/core/constants
    FILES_MATCHING PATTERN "*.h"
)
```

---

## Integration Checklist

### Phase 1: Foundation (3-4 hours)
- [ ] Create `src/core/constants/` directory
- [ ] Create all header files (constants.h, providers.h, systems.h, etc.)
- [ ] Update CMakeLists.txt to expose constants
- [ ] Verify compilation (no errors)
- [ ] Create unit tests for lookup functions
- [ ] Update .github/copilot-instructions.md with constants reference

**Test Cases**:
```cpp
void test_system_lookup() {
    auto sys = Constants::Systems::getSystem(Constants::Systems::ID_NES);
    ASSERT_EQ(sys->internalName, "NES");
    ASSERT_EQ(sys->preferredHash, "CRC32");
}

void test_provider_lookup() {
    auto provider = Constants::Providers::getProviderInfo(
        Constants::Providers::SCREENSCRAPER);
    ASSERT_EQ(provider->requiresAuth, true);
}

void test_extension_mapping() {
    auto systems = Constants::Systems::EXTENSION_TO_SYSTEMS[".nes"];
    ASSERT_EQ(systems.size(), 1);
    ASSERT_EQ(systems[0], Constants::Systems::ID_NES);
}
```

### Phase 2: UI Integration (2-3 hours)
- [ ] Create `ui_theme.h` with color constants
- [ ] Implement `confidence.h` with thresholds
- [ ] Update MatchListModel to use Confidence::Thresholds
- [ ] Update MatchReviewView to use Colors::Confidence
- [ ] Create color mapping utility class

**Code Example**:
```cpp
// Before: hardcoded color
color: confidence >= 90 ? "#27ae60" : confidence >= 60 ? "#f39c12" : "#e74c3c"

// After: constants
using namespace Remus::Constants;
color: Confidence::getColor(confidence)
```

### Phase 3: Settings & Templates (2-3 hours)
- [ ] Create `templates.h` with variable definitions
- [ ] Create `settings.h` with key definitions
- [ ] Implement validation in TemplateEngine
- [ ] Refactor SettingsController to use constants
- [ ] Update UI forms to reference template variables

**Code Example**:
```cpp
// Before: settings key scattered
settings.setValue("organize/naming_template", template_value);

// After: type-safe constant
settings.setValue(
    Constants::Settings::Keys::NAMING_TEMPLATE, 
    template_value
);
```

### Phase 4: Cleanup (2-3 hours)
- [ ] Refactor SystemDetector to use Constants::Systems
- [ ] Refactor CLI initialization
- [ ] Remove all hardcoded system/provider lists
- [ ] Update documentation
- [ ] Verify all tests pass

### Phase 5: Future-Proofing (1-2 hours)
- [ ] Add `hash.h` with algorithm definitions
- [ ] Add `files.h` with file types
- [ ] Prepare stubs for M8 (verification.h, patching.h)
- [ ] Create constants/README.md
- [ ] Update architecture documentation

---

## Compilation & Testing

### Build Commands
```bash
cd /home/solon/Documents/remus/build
cmake ..  # Regenerate build system
make      # Compile (should add 0.5s to build time)
ctest -V  # Run tests including constant lookup tests
```

### Expected Results
- **Compilation**: Successful (header-only library adds minimal overhead)
- **Build time**: +0.3-0.5 seconds (header analysis only)
- **Binary size**: Unchanged (no code generation yet)
- **Runtime**: Identical (all constexpr/inline)

### Validation
```cpp
// Verify all systems are accessible
size_t system_count = Constants::Systems::SYSTEMS.size();
assert(system_count == 20);  // Update per actual count

// Verify all providers registered
size_t provider_count = Constants::Providers::PROVIDER_REGISTRY.size();
assert(provider_count == 5);
```

---

## Migration Guide

For refactoring existing code to use constants:

### Example 1: System Names

**Before**:
```cpp
void initializeSystems() {
    m_systems["NES"] = new NESSystem();
    m_systems["SNES"] = new SNESSystem();
    // ... 18 more systems
}
```

**After**:
```cpp
void initializeSystems() {
    using namespace Constants::Systems;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        const auto& def = it.value();
        m_systems[def.internalName] = createSystem(it.key());
    }
}
```

### Example 2: Provider Access

**Before**:
```cpp
QString providerName = "screenscraper";
if (providerName == "screenscraper") {
    // ScreenScraper-specific logic
} else if (providerName == "igdb") {
    // IGDB logic
}
```

**After**:
```cpp
using namespace Constants::Providers;
const auto* info = getProviderInfo(providerId);
if (info && info->requiresAuth) {
    // Auth-required logic
}
```

### Example 3: UI ComboBox Population

**Before**:
```qml
ComboBox {
    model: ["NES", "SNES", "PlayStation", "N64"]  // Hardcoded
}
```

**After**:
```qml
ComboBox {
    model: LibraryController.systemNames  // Populated from constants
}
```

```cpp
// In LibraryController
QStringList getSystemNames() {
    return Constants::Systems::getSystemInternalNames();
}
```

---

## Performance Notes

- **Compilation**: Header-only library (no separate compilation unit)
- **Runtime**: All constexpr/inline (zero overhead, compiler optimizes away)
- **Lookup Speed**: O(log n) for map lookups (typically 20-50 systems max)
- **Memory**: Constants stored in read-only section (.rodata)

---

## Future Extensions

### M6 (Organize Engine UI)
```cpp
// Add to constants/templates.h
namespace Collision {
    inline constexpr const char* SKIP = "skip";
    inline constexpr const char* RENAME = "rename";
    inline constexpr const char* OVERWRITE = "overwrite";
}
```

### M7 (Artwork Management)
```cpp
// Create constants/artwork.h
struct ArtworkType {
    QString id;
    QString displayName;
    QSize thumbnailSize;
    QSize fullSize;
};

const QMap<QString, ArtworkType> ARTWORK_TYPES = {
    {"boxFront", {"boxFront", "Box Art", {240, 320}, {600, 800}}},
    {"screenshot", {"screenshot", "Screenshot", {320, 240}, {1280, 960}}},
    // ...
};
```

### M8 (Verification & Patching)
```cpp
// Create constants/verification.h
namespace VerificationStatus {
    inline constexpr const char* VERIFIED = "verified";
    inline constexpr const char* FAILED = "failed";
    inline constexpr const char* UNKNOWN = "unknown";
}

// Create constants/patching.h
namespace PatchFormat {
    inline constexpr const char* IPS = "ips";
    inline constexpr const char* BPS = "bps";
    inline constexpr const char* UPS = "ups";
    inline constexpr const char* XDELTA3 = "xdelta3";
    inline constexpr const char* PPF = "ppf";
}
```

---

## Documentation Update

Update `.github/copilot-instructions.md` to include:

```markdown
## Constants Library Reference

All application-wide constants are centralized in `src/core/constants/`:

- **Providers**: Metadata provider IDs, display names, priorities
- **Systems**: 20+ gaming systems with metadata (extensions, hashes, etc.)
- **Templates**: Named template variables, naming presets
- **Confidence**: Match confidence thresholds and color mappings
- **Settings**: QSettings key definitions with defaults
- **UI Theme**: Color palette, typography, sizes

### Usage Example

```cpp
#include "src/core/constants/constants.h"

using namespace Remus::Constants;

// Get system info
const auto* system = Systems::getSystem(Systems::ID_NES);
qDebug() << system->displayName;  // "Nintendo Entertainment System"

// Access provider info
auto providers = Providers::getEnabledProviders();

// Settings key validation
settings.setValue(Settings::Keys::NAMING_TEMPLATE, value);
```

### Adding New Constants

1. Determine category (provider, system, UI, etc.)
2. Add to appropriate header file (providers.h, systems.h, etc.)
3. Create lookup helper function
4. Update unit tests
5. Reference in copilot-instructions.md
```

