# Remus Constants Library

**Type-safe, centralized constants for the Remus ROM library manager**

## Overview

This header-only library provides all application-wide constants in a single, type-safe location. It eliminates scattered hardcoded strings and enables compile-time validation of provider names, system definitions, and configuration keys.

## Modules

### providers.h
- **Provider Registry**: Metadata provider definitions (ScreenScraper, IGDB, TheGamesDB, Hasheous)
- **Capabilities**: Hash/name matching support, authentication requirements
- **Priority System**: Fallback chain ordering (higher priority = tried first)
- **Settings Keys**: QSettings keys for provider credentials

### systems.h
- **System Registry**: 30+ gaming systems with complete metadata
- **System Properties**: Extensions, preferred hash algorithms, manufacturer, generation
- **Extension Mapping**: Reverse lookup from file extension to possible systems
- **System Grouping**: Nintendo/Sega/Sony systems, disc/cartridge systems, handhelds

### templates.h
- **Template Variables**: Canonical variable names for rename templates
- **Default Templates**: No-Intro, Redump, and simple defaults
- **Validation**: Variable list for template validation and UI hints

### settings.h
- **Settings Keys**: QSettings keys for providers, metadata, organize, and performance
- **Defaults**: Centralized default values for UI and settings initialization

### api.h _(NEW)_
- **Provider API Endpoints**: Base URLs for ScreenScraper, IGDB, TheGamesDB, Hasheous
- **Endpoint Paths**: Individual API endpoint paths (games, platforms, artwork, etc.)
- **CDN URLs**: Image CDN and artwork service URLs
- **Query Parameters**: Standard HTTP headers and pagination constants

### database_schema.h _(NEW)_
- **Table Names**: All database table identifiers (systems, files, games, matches, etc.)
- **Column Names**: Organized by table for type-safe column references
- **Index Names**: Predefined database indexes
- **Schema Pragmas**: SQLite configuration constants
- **Constraints**: Column length limits and database constraints

### network.h _(NEW)_
- **HTTP Timeouts**: Request timeouts for different services (5s-60s)
- **Rate Limiting**: Per-provider rate limits (250ms-1000ms)
- **Retry Policy**: Maximum retries, backoff multiplier, retry delays
- **Connection Parameters**: Connection pooling, thread pool sizes
- **Cache Control**: Metadata and artwork cache TTLs
- **HTTP Status Codes**: Standard error codes for handling

### engines.h _(NEW)_
- **Processing Status**: Unprocessed, in-progress, processed, failed, skipped, error
- **Organize Engine**: Operation types (move, copy, symlink), collision handling
- **Verify Engine**: Verification status values (verified, good, corrupted, bad)
- **Patch Engine**: Patch format types (IPS, BPS, UPS, XDELTA)
- **Hashing Engine**: Parallelization settings, chunk sizes
- **Archive Extraction**: Archive format types, size limits
- **CHD Conversion**: CHD versions, compression methods
- **Validation**: File size constraints, operation timeouts

### errors.h _(NEW)_
- **Database Errors**: Schema creation, query execution, transactions
- **File System Errors**: File I/O, permissions, path validation
- **Hashing Errors**: Hash calculation, algorithm support
- **Metadata Provider Errors**: API failures, authentication, rate limits, parsing
- **Matching Errors**: Match not found, system support, ambiguous matches
- **Organize Errors**: File collision, undo operations
- **Verify Errors**: DAT file parsing, header corruption, verification failures
- **Patch Errors**: Incompatible patches, application failures
- **Archive Errors**: Corruption, format unsupported, extraction timeouts
- **CHD Errors**: Conversion failures, unsupported versions
- **Configuration Errors**: Settings validation, template validation

### ui_theme.h
- **Color Palette**: System-specific colors, UI theme constants
- **Typography**: Font sizes, weights, family definitions

### confidence.h
- **Match Confidence Levels**: Thresholds for display (high/medium/low)
- **Color Coding**: Green/yellow/red confidence visualization

### hash_algorithms.h
- **Hash Algorithm Metadata**: Algorithm names, output lengths, system assignments

### match_methods.h
- **Matching Methods**: Hash, exact name, fuzzy, levenshtein enum values

### constants.h
- **Main Include**: Includes all module headers
- **Version Tracking**: Constants library version number (currently v5)
- **App Metadata**: Application version, milestone tracking

## Usage Examples

### Provider Access

```cpp
#include "constants/constants.h"

using namespace Remus::Constants;

// Get provider by ID
auto info = Providers::getProviderInfo(Providers::SCREENSCRAPER);
if (info && info->requiresAuth) {
    qDebug() << "Provider requires authentication:" << info->displayName;
}

// Get all providers sorted by priority
auto providers = Providers::getProvidersByPriority();
for (const auto &providerId : providers) {
    qDebug() << Providers::getProviderDisplayName(providerId);
}

// Check capabilities
auto hashProviders = Providers::getHashSupportingProviders();
// Returns: ["hasheous", "screenscraper"]
```

### System Lookup

```cpp
#include "constants/constants.h"

using namespace Remus::Constants;

// Lookup by system ID
auto nes = Systems::getSystem(Systems::ID_NES);
qDebug() << nes->displayName;  // "Nintendo Entertainment System"
qDebug() << nes->preferredHash;  // "CRC32"

// Lookup by internal name
auto psx = Systems::getSystemByName("PlayStation");
qDebug() << psx->extensions;  // [".cue", ".bin", ".iso", ".pbp", ".chd"]
qDebug() << psx->isMultiFile;  // true

// Get systems for file extension
auto systems = Systems::getSystemsForExtension(".iso");
// Returns: [ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII, ...]

// Check if extension is ambiguous
if (Systems::isAmbiguousExtension(".iso")) {
    // Need path heuristics to determine actual system
}
```

### UI Population

```cpp
// Populate system ComboBox
QStringList systemNames = Systems::getSystemDisplayNames();
ui->systemCombo->addItems(systemNames);

// Populate provider ComboBox
auto providers = Providers::getProvidersByPriority();
for (const auto &id : providers) {
    QString displayName = Providers::getProviderDisplayName(id);
    ui->providerCombo->addItem(displayName, id);
}
```

### Settings Access

```cpp
using namespace Remus::Constants;

QSettings settings;

// Type-safe settings keys
QString username = settings.value(
    Settings::Providers::SCREENSCRAPER_USERNAME,
    QString()
).toString();

settings.setValue(
    Settings::Providers::SCREENSCRAPER_PASSWORD,
    password
);
```

### API & Network Configuration

```cpp
#include "constants/constants.h"
using namespace Remus::Constants;

// Connect to ScreenScraper API
QUrl apiUrl(QString(API::SCREENSCRAPER_BASE_URL) + 
            QString(API::SCREENSCRAPER_JEURECHERCHE_ENDPOINT));

// Apply timeouts per provider
int timeout = Network::SCREENSCRAPER_TIMEOUT_MS;  // 20 seconds
int rateLimit = Network::SCREENSCRAPER_RATE_LIMIT_MS;  // 250ms minimum

// Handle HTTP errors appropriately
int statusCode = response.statusCode();
if (statusCode == Network::HTTP_SERVICE_UNAVAILABLE) {
    // Retry with backoff
    int retryDelay = Network::INITIAL_RETRY_DELAY_MS;
} else if (statusCode == Network::HTTP_NOT_FOUND) {
    // Don't retry, no results
    return GameMetadata();  // Empty result
}
```

### Database Schema

```cpp
#include "constants/constants.h"
using namespace Remus::Constants;

// Create queries using constants
QString createTable = QString(
    "CREATE TABLE IF NOT EXISTS %1 ("
    "  %2 INTEGER PRIMARY KEY, "
    "  %3 TEXT NOT NULL, "
    "  %4 INTEGER NOT NULL, "
    "  FOREIGN KEY (%4) REFERENCES %5(%2)"
    ")"
).arg(
    Database::Tables::FILES,
    Database::Columns::Files::ID,
    Database::Columns::Files::FILENAME,
    Database::Columns::Files::SYSTEM_ID,
    Database::Tables::SYSTEMS
);

// Safe index creation
query.exec(QString("CREATE INDEX IF NOT EXISTS %1 ON %2(%3)")
    .arg(Database::Indexes::FILES_SYSTEM_ID,
         Database::Tables::FILES,
         Database::Columns::Files::SYSTEM_ID));

// Query with named columns
query.prepare(QString("SELECT %1 FROM %2 WHERE %3 = ?")
    .arg(Database::Columns::FILES::FILENAME,
         Database::Tables::FILES,
         Database::Columns::FILES::IS_PROCESSED));
```

### Processing & Engine Status

```cpp
#include "constants/constants.h"
using namespace Remus::Constants;

// Update file processing status
void updateFileStatus(int fileId, const QString &status) {
    // Use predefined status values
    if (status == Engines::ProcessingStatus::PROCESSED) {
        database.updateFile(fileId, status);
    } else if (status == Engines::ProcessingStatus::FAILED) {
        logger.error("File processing failed: " + QString::number(fileId));
    }
}

// Organize operation with collision handling
bool organizeFile(const QString &source, const QString &dest) {
    if (fileExists(dest)) {
        // Use constants for collision handling
        QString mode = Engines::Organize::COLLISION_RENAME;
        return handleCollision(source, dest, mode);
    }
    return moveFile(source, dest);
}

// Verify DAT files
bool verifyWithDAT(const QString &hash) {
    auto result = datDatabase.lookup(hash);
    if (result.status == Engines::Verify::VERIFIED) {
        return true;
    } else if (result.status == Engines::Verify::CORRUPTED) {
        qWarning() << Errors::Verify::VERIFICATION_FAILED;
    }
}
```

### Error Handling

```cpp
#include "constants/constants.h"
using namespace Remus::Constants;

// Database error handling
if (!database.initialize(dbPath)) {
    qCritical() << Errors::Database::FAILED_TO_OPEN;
    return false;
}

if (!database.createSchema()) {
    qCritical() << Errors::Database::FAILED_TO_CREATE_SCHEMA;
    return false;
}

// Provider error handling
try {
    auto metadata = provider->getByHash(hash, systemId);
} catch (const std::exception &e) {
    if (QString(e.what()).contains("timeout")) {
        qWarning() << Errors::MetadataProvider::REQUEST_TIMEOUT;
    } else if (QString(e.what()).contains("auth")) {
        qWarning() << Errors::MetadataProvider::AUTHENTICATION_FAILED;
    }
}

// File operation error handling
if (!QFile::copy(source, dest)) {
    qWarning() << Errors::FileSystem::FAILED_TO_COPY_FILE;
    if (!QDir(QFileInfo(dest).absolutePath()).exists()) {
        qWarning() << Errors::FileSystem::DIRECTORY_NOT_FOUND;
    }
}
```

## Design Principles

### 1. Type Safety
All constants use typed identifiers (not raw strings):
```cpp
// Bad (old approach)
QString provider = "ScreenScraperr";  // Typo not caught

// Good (constants approach)
QString provider = Providers::SCREENSCRAPER;  // Compile-time constant
```

### 2. Single Source of Truth
No duplicate definitions across files:
```cpp
// Instead of maintaining lists in:
// - src/cli/main.cpp
// - src/ui/qml/LibraryView.qml
// - docs/requirements.md

// Single definition in systems.h used everywhere
auto systems = Systems::SYSTEMS;
```

### 3. Self-Documenting
Constants include complete metadata:
```cpp
struct ProviderInfo {
    QString id;              // Internal identifier
    QString displayName;     // User-facing name
    QString description;     // Tooltip text
    bool supportsHashMatch;  // Capabilities
    bool requiresAuth;       // Requirements
    int priority;            // Fallback order
};
```

### 4. Zero Runtime Overhead
Header-only library with inline constexpr:
```cpp
inline constexpr const char* SCREENSCRAPER = "screenscraper";
inline constexpr int ID_NES = 1;

// Compiler optimizes to direct string/int usage
```

## Integration

### CMake
```cmake
target_link_libraries(your_target PRIVATE remus-constants)
```

### Include
```cpp
#include "constants/constants.h"
using namespace Remus::Constants;
```

## Testing

Unit tests cover:
- Provider registry integrity
- Provider lookup functions
- System registry completeness
- System lookup by ID and name
- Extension to system mapping
- Ambiguous extension detection
- System grouping correctness

Run tests:
```bash
cd build
make test_constants
ctest --output-on-failure
```

## Maintenance

### Adding a New System

1. Add system ID constant:
```cpp
inline constexpr int ID_NEW_SYSTEM = 24;
```

2. Add to SYSTEMS registry:
```cpp
{ID_NEW_SYSTEM, {
    ID_NEW_SYSTEM,
    QStringLiteral("NewSystem"),
    QStringLiteral("New Gaming System"),
    QStringLiteral("Manufacturer"),
    6,  // Generation
    {QStringLiteral(".ext1"), QStringLiteral(".ext2")},
    QStringLiteral("MD5"),
    {QStringLiteral("USA"), QStringLiteral("JPN")},
    false,
    QStringLiteral("#3498db"),
    2020
}},
```

3. Update EXTENSION_TO_SYSTEMS mapping:
```cpp
{QStringLiteral(".ext1"), {ID_NEW_SYSTEM}},
```

4. Add to appropriate grouping (DISC_SYSTEMS, HANDHELD_SYSTEMS, etc.)

That's it! The new system automatically appears in:
- CLI system list
- UI ComboBoxes
- System detector
- Database initialization

### Adding a New Provider

1. Add provider ID constant:
```cpp
inline constexpr const char* NEW_PROVIDER = "newprovider";
```

2. Add display name:
```cpp
inline const QString DISPLAY_NEW_PROVIDER = QStringLiteral("New Provider");
```

3. Add to PROVIDER_REGISTRY:
```cpp
{NEW_PROVIDER, {
    NEW_PROVIDER,
    DISPLAY_NEW_PROVIDER,
    QStringLiteral("Provider description"),
    true,   // Supports hash?
    true,   // Supports name search?
    false,  // Requires auth?
    QStringLiteral("https://provider.com"),
    70,     // Priority
    true    // Free service?
}},
```

4. Add settings keys:
```cpp
namespace SettingsKeys {
    inline constexpr const char* NEW_PROVIDER_API_KEY = 
        "providers/newprovider/api_key";
}
```

## Version History

- **v1**: Initial implementation (Phase 1)
  - Provider registry (4 providers)
  - System registry (30 systems)
  - Extension mappings
  - Helper functions

- **v2**: Phase 3 additions
  - Template system with variables
  - Settings keys organization
  - Confidence levels
  - Hash algorithm metadata

- **v3**: Network, API, Engine, and Error Constants
  - API endpoints (ScreenScraper, IGDB, TheGamesDB, Hasheous)
  - Database schema (tables, columns, indexes)
  - Network configuration (timeouts, rate limits, retry policy)
  - Processing engines (organize, verify, patch, CHD, archive, hashing)
  - Error messages (database, filesystem, providers, matching, operations)

## Future Enhancements

Phase 5+ may add:
- `compression.h` - Compression algorithm metadata
- `artwork.h` - Artwork types and size constants
- `ui_icons.h` - Icon resource paths and IDs
- `performance.h` - Performance tuning parameters
- `logging.h` - Logging levels and categories

## License

Part of the Remus project. See top-level LICENSE file.
