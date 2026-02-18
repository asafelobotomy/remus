# Building Remus (M1-M2 - Scanning & Metadata)

## Prerequisites

### Required Dependencies
- **CMake** >= 3.16
- **Qt 6** (Core, Sql, Network modules)
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **zlib** (for CRC32 calculation)

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libqt6sql6-sqlite zlib1g-dev
```

#### Fedora
```bash
sudo dnf install cmake qt6-qtbase-devel qt6-qtbase-private-devel zlib-devel gcc-c++
```

#### Arch Linux
```bash
sudo pacman -S cmake qt6-base zlib gcc
```

## Building from Source

### 1. Clone the repository
```bash
cd ~/Documents/remus
```

### 2. Create build directory
```bash
mkdir -p build
cd build
```

### 3. Configure with CMake
```bash
cmake ..
```

If Qt 6 is not found automatically, specify the path:
```bash
cmake -DCMAKE_PREFIX_PATH=/usr/lib/qt6 ..
```

### 4. Build
```bash
make -j$(nproc)
```

### 5. Verify build
```bash
./remus-cli --version
```

Expected output:
```
remus-cli 0.1.0
```

## Running the CLI

### Scanning Commands

#### Scan a directory
```bash
./remus-cli --scan ~/roms/NES
```

#### Scan and calculate hashes
```bash
./remus-cli --scan ~/roms/NES --hash
```

#### List scanned files by system
```bash
./remus-cli --list
```

#### Use custom database location
```bash
./remus-cli --scan ~/roms/SNES --db ~/my-library.db --hash
```

### Metadata Commands (M2)

#### Search for a game by name
```bash
./remus-cli --search "Super Mario Bros" --system NES
```

#### Search with TheGamesDB
```bash
./remus-cli --search "Final Fantasy" --provider thegamesdb
```

#### Fetch metadata by hash (ScreenScraper)
```bash
# Requires ScreenScraper account
./remus-cli --metadata 3337ec46 --system NES \
  --provider screenscraper \
  --ss-user "your_username" \
  --ss-pass "your_password" \
  --ss-devid "your_devid" \
  --ss-devpass "your_devpassword"
```

#### Fetch metadata by hash (Hasheous - NO AUTH REQUIRED!)
```bash
# FREE hash matching, no API key needed
./remus-cli --metadata 811b027eaf99c2def7b933c5208636de --provider hasheous
```

### Matching Commands (M3)

#### Match files with intelligent provider fallback
```bash
# Automatically tries: Hasheous → ScreenScraper → TheGamesDB → IGDB
./remus-cli --scan ~/roms/NES --hash --match
```

#### Match with minimum confidence threshold
```bash
# Only accept matches with 80% or higher confidence
./remus-cli --match --min-confidence 80
```

#### Match with ScreenScraper authentication
```bash
./remus-cli --scan ~/roms --hash --match \
  --ss-user "username" \
  --ss-pass "password" \
  --min-confidence 70
```

## Example Workflow

```bash
# 1. Scan NES collection
./remus-cli --scan ~/roms/NES --hash --db remus.db

# 2. Scan SNES collection (adds to same database)
./remus-cli --scan ~/roms/SNES --hash --db remus.db

# 3. View summary
./remus-cli --list --db remus.db
```

Expected output:
```
╔════════════════════════════════════════╗
║  Remus - Retro Game Library Manager   ║
║  M1: Core Scanning Engine (CLI)       ║
╚════════════════════════════════════════╝

Files by system:
─────────────────────────────────────
  NES                 : 450 files
  SNES                : 320 files
  Unknown             : 5 files
─────────────────────────────────────
  Total: 775 files

Done!
```

## Project Structure

```
remus/
├── CMakeLists.txt          # Main CMake configuration
├── src/
│   ├── core/               # Core scanning engine
│   │   ├── scanner.h/cpp   # File scanner
│   │   ├── system_detector.h/cpp  # System detection
│   │   ├── hasher.h/cpp    # Hash calculation
│   │   ├── database.h/cpp  # SQLite database
│   │   └── CMakeLists.txt
│   └── cli/
│       └── main.cpp        # CLI entry point
├── tests/
│   └── CMakeLists.txt
└── docs/
    └── [documentation]
```

## Database Schema

The CLI creates a SQLite database with the following tables:

- **systems**: Supported gaming systems
- **libraries**: Scanned library paths
- **files**: ROM/disc files with hashes and metadata

To inspect the database:
```bash
sqlite3 remus.db
sqlite> SELECT name, COUNT(*) FROM files JOIN systems ON files.system_id = systems.id GROUP BY name;
```

## Troubleshooting

### Qt 6 not found
If CMake cannot find Qt 6:
```bash
# Find Qt installation
find /usr -name "Qt6Config.cmake" 2>/dev/null

# Set CMAKE_PREFIX_PATH
cmake -DCMAKE_PREFIX_PATH=/path/to/qt6 ..
```

### zlib not found
```bash
# Ubuntu/Debian
sudo apt install zlib1g-dev

# Fedora
sudMetadata Providers

### ScreenScraper (Recommended)
- **Hash-based matching**: ✅ (CRC32, MD5, SHA1)
- **Authentication**: Required
- **Rate limit**: 1 req/2s, 10k req/day
- **Registration**: https://www.screenscraper.fr/inscription.php
- **Features**: Most comprehensive database, supports No-Intro/Redump hashes

### TheGamesDB
- **Hash-based matching**: ❌ (name-based only)
- **Authentication**: Optional API key
- **Rate limit**: 1 req/s
- **Registration**: Free, no account needed
- **Features**: Good for name-based searches

### IGDB
- **Hash-based matching**: ❌ (name-based only)
- **Authentication**: Required (Twitch credentials)
- **Rate limit**: 4 req/s
- **Registration**: Requires Twitch developer account
- **Features**: Comprehensive, modern database

## Next Steps (M4-M8)

- **M4**: Organize and rename engine with No-Intro/Redump compliance
- **M4.5**: CHD conversion and compression
- **M5**: Qt GUI with library view and batch operations
- **M6**: AppImage packaging and distribution
- **M7**: Polishing and optimization
- **M8**: Verification and patching (DAT files, romhacking.net integration)

## Testing

To test with sample ROMs:
```bash
# Create test directory
mkdir -p test_roms/NES
cd test_roms/NES

# Add some ROM files (you must provide your own)
# ...

# Scan test directory
cd ../../build
./remus-cli --scan ../test_roms/NES --hash --db test.db
./remus-cli --list --db test.db
```

## Next Steps (M2-M7)

- **M2**: Metadata fetching from ScreenScraper, TheGamesDB, IGDB
- **M3**: Matching pipeline with confidence scoring
- **M4**: Organize and rename engine
- **M4.5**: CHD conversion
- **M5**: Qt GUI
- **M6**: AppImage packaging
- **M7**: Polishing

## Contributing

M1 is the foundation. Once core scanning is stable, we'll proceed to metadata layer (M2).

Feedback welcome on:
- System detection accuracy
- Hash calculation performance
- Multi-file set detection (CUE+BIN)
- Database schema

## License

*To be determined*
