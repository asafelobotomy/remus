# Building Remus

## Prerequisites

### Required Dependencies
- **CMake** >= 3.16
- **Qt 6 development files** (Core, Gui, Sql, Network, Concurrent, Quick, QML, Quick Controls 2, Quick Layouts, Quick Dialogs 2)
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **zlib** (for CRC32 calculation)
- **libarchive** (ZIP/7z/RAR extraction)
- **Qt6Keychain** (OS credential store for provider API keys)

### Installing Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install build-essential cmake \
  qt6-base-dev qt6-base-private-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools \
  libqt6sql6-sqlite libgl1-mesa-dev libxkbcommon-dev \
  zlib1g-dev libarchive-dev qtkeychain-qt6-dev
```

#### Fedora
```bash
sudo dnf install cmake qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel \
  zlib-devel libarchive-devel qt6-qtkeychain-devel gcc-c++
```

#### Arch Linux
```bash
sudo pacman -S cmake qt6-base qt6-declarative zlib libarchive qt6-keychain gcc
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
cmake -S .. -B .
```

The default configure builds both `remus-cli` and `remus-gui`. The legacy TUI remains archived under `archive/gui-tui/` and is not part of the active build.

#### Optional build acceleration flags
```bash
# Enable compiler cache (default ON when ccache is installed)
cmake -DREMUS_ENABLE_CCACHE=ON ..

# Enable precompiled headers (default ON)
cmake -DREMUS_ENABLE_PCH=ON ..

# Enable unity/jumbo compilation units (default OFF)
cmake -DREMUS_ENABLE_UNITY_BUILD=ON ..

# Optional: build with C++20 instead of C++17
cmake -DREMUS_ENABLE_CXX20=ON ..
```

#### Recommended build profiles (benchmark-backed)

**Profile A — Fast clean builds (CI/rebuild-heavy sessions)**
```bash
cmake -DREMUS_ENABLE_CCACHE=OFF \
  -DREMUS_ENABLE_PCH=ON \
  -DREMUS_ENABLE_UNITY_BUILD=ON \
  ..
```
Use when repeatedly doing full clean builds; this profile gave the best clean-build speed in local benchmarks.

**Profile B — Fast iterative rebuilds across fresh build directories**
```bash
cmake -DREMUS_ENABLE_CCACHE=ON \
  -DREMUS_ENABLE_PCH=OFF \
  -DREMUS_ENABLE_UNITY_BUILD=OFF \
  ..
```
Use when you create multiple build directories and recompile similar code. In this environment, `ccache` produced significant warm-build gains with PCH disabled.

If Qt 6 is not found automatically, specify the path:
```bash
cmake -DCMAKE_PREFIX_PATH=/usr/lib/qt6 ..
```

### Code style

Format before committing (clang-format version in `.clang-format-version`):

```bash
find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
```

Enable compiler warnings during development:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DREMUS_ENABLE_WARNINGS=ON
```

### 4. Build
```bash
cmake --build . -j$(nproc)
```

### 5. Verify build

```bash
./remus-cli --version
./src/gui/remus-gui
```

Expected output:

```text
remus-cli 0.10.1
```

The GUI launches the Qt Quick shell with views for library browsing, scan/hash workflows, metadata matching, artwork, DAT management, verification, organization, conversion, patching, mods, and settings.

### 6. Package a CLI release archive

Create a distributable CLI archive from the current build:

```bash
../scripts/package_cli_archive.sh
```

The script writes a versioned tarball and SHA256 file to the repository root `dist/` directory.

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
├── CMakeLists.txt          # Top-level build configuration
├── src/
│   ├── cli/                # CLI entry point and commands
│   ├── core/               # Scanning, hashing, database, patching, bundling
│   ├── gui/                # Qt Quick desktop application
│   ├── metadata/           # Provider clients, cache, and enrichment logic
│   └── services/           # Shared application services
├── tests/                  # Unit and integration tests registered with CTest
├── scripts/                # Packaging, audit, and pipeline helpers
└── docs/                   # Setup guides, architecture notes, plans, and reports
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
sudo dnf install zlib-devel
```

## Metadata Providers

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

## Delivery Focus

- Ship and verify both the CLI (`remus-cli`) and GUI (`remus-gui`) by default — both are part of the standard build at 0.10.1.
- Use simple release archives until the full GUI workflow is stable.
- The GUI shell launches a Qt Quick interface with views for library browsing, scan/hash workflows, metadata matching, artwork, DAT management, verification, organisation, conversion, patching, mods, and settings.

## Testing

Use the canonical repository-local test paths described in [../guides/TEST-DATA-POLICY.md](../guides/TEST-DATA-POLICY.md).

To test with sample ROMs:
```bash
# Create local ROM input directory
mkdir -p roms/NES
cd roms/NES

# Add some ROM files (you must provide your own)
# ...

# Scan test directory
cd ../../build
./remus-cli --scan ../roms/NES --hash --db test.db
./remus-cli --list --db test.db

# Keep any follow-up notes in a tracked log
printf '%s\n' '[open] describe the failing case here' >> ../test_output/attention.log
```

## Release Outputs

- `build/remus-cli` for local development
- `dist/remus-cli-<version>-linux-x64.tar.gz` for release packaging
- `dist/remus-cli-<version>-linux-x64.tar.gz.sha256` for checksum verification

## Contributing

M1 is the foundation. Once core scanning is stable, we'll proceed to metadata layer (M2).

Feedback welcome on:
- System detection accuracy
- Hash calculation performance
- Multi-file set detection (CUE+BIN)
- Database schema

## License

*To be determined*
