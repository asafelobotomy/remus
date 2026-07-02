# Building Remus

## Prerequisites

### Required Dependencies

- **CMake** >= 3.16
- **Qt 6 development files** (Core, Gui, Sql, Network, Concurrent, Quick, QML, Quick Controls 2, Quick Layouts, Quick Dialogs 2)
- **C++17** compatible compiler (GCC 7+, Clang 5+)
- **zlib** (for CRC32 calculation)
- **libarchive** (ZIP/7z/RAR extraction)
- **Qt6Keychain** (OS credential store for provider API keys)

### Installing Dependencies

#### Ubuntu/Debian (22.04+)

CI uses Ubuntu 24.04 with Qt 6.4+. For Ubuntu 22.04, use Qt 6.4 or newer from your distro or the Qt online installer.

```bash
sudo apt update
sudo apt install build-essential cmake \
  qt6-base-dev qt6-base-private-dev qt6-base-dev-tools \
  qt6-declarative-dev qt6-declarative-private-dev qt6-declarative-dev-tools \
  libqt6sql6-sqlite libgl1-mesa-dev libxkbcommon-dev \
  zlib1g-dev libarchive-dev qtkeychain-qt6-dev \
  zip unzip p7zip-full
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

### 2. Bootstrap compendium schema (once per clone)

```bash
bash scripts/setup_compendium_db.sh
```

Creates `data/compendium/remus_compendium.db` (gitignored; systems/regions seeds only).

### 3. Create build directory

```bash
mkdir -p build
cd build
```

### 4. Configure with CMake

```bash
cmake -S .. -B .
```

Or use [`CMakePresets.json`](../../CMakePresets.json) from the repository root:

```bash
cd ..
cmake --preset default
cmake --build build -j$(nproc)
```

Presets: `default` → `build/`, `debug` → `build-debug/`, `asan` → `build-asan/`, `coverage` → `build-coverage/`. See [scripts/README.md](../../scripts/README.md).

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

### 5. Build

```bash
cmake --build . -j$(nproc)
```

### 6. Verify build

```bash
./remus-cli --version
./src/gui/remus-gui
```

Expected output includes the current `APP_VERSION` from `src/core/constants/api.h` (see root `VERSION` file).

```text
remus-cli 0.12.0
```

The GUI launches the Qt Quick shell with views for library browsing, scan/hash workflows, metadata matching, artwork, DAT management, verification, organization, conversion, patching, mods, and settings.

### 7. Run tests

```bash
ctest --test-dir build --output-on-failure
```

### 8. Build the compendium database

Remus uses a **generated** bootstrap `remus_compendium.db` (schema + seeds only; not committed — create with `scripts/setup_compendium_db.sh`). Populate it for hash-first matching via `remus-cli --build-compendium` when DAT sources or enrichment inputs change.

**One-command full refresh** (download DATs, generate manifest, build, validate, coverage report):

```bash
cmake --build build
bash scripts/build_compendium_full.sh
```

The wrapper script:

- Refreshes offline inputs via `scripts/update_compendium_offline_sources.sh` (skip with `--skip-update`)
- Generates `data/compendium/compendium-manifest-full.json`
- Runs `remus-cli --build-compendium` with **manifest checksum verification** on every enabled DAT
- **Fails** on unresolved merge conflicts unless you pass `--allow-unresolved-conflicts`
- Runs validation tiers via `scripts/validate_compendium_tier.sh ci` (skip with `--skip-validation`; migrations always run unless `--skip-migrations`)
- Runs warn-only artwork coverage via `scripts/validate_compendium_tier.sh artwork` after consolidate
- Writes `data/compendium/remus_compendium.coverage.tsv`

**Additional flags:** `--strict-offline` (Tier A mirror preflight + artwork manifest gate), `--force-enrichment`, `--recover`, `--allow-patch-skip`, `--skip-migrations`, `--skip-consolidate`.

**Detached build** (survives terminal close; parent holds flock until completion):

```bash
bash scripts/run_compendium_full_build_detached.sh
tail -f "${REMUS_COMPENDIUM_BUILD_LOG:-${TMPDIR:-/tmp}/remus_compendium_full_build.log}"
```

`collision.serial_multi_game` is scoped by `(serial_value, system_id)` to match the identity linker. If validation fails after a build, run `remus-cli --dedup-compendium` to prune region-mismatched serial rows and merge duplicate games sharing a serial on the same system.

**Manual steps** for a smaller experiment:

```bash
bash scripts/generate_compendium_manifest.sh \
  --dat-dir data/databases \
  --output data/compendium/compendium-manifest-full.json

build/remus-cli --build-compendium \
  --compendium-manifest data/compendium/compendium-manifest-full.json \
  --compendium-output data/compendium/remus_compendium.db

bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db
```

Optional enrichment inputs (skipped quietly when missing — see `.report.json`):

- `data/metadata/` (libretro)
- `data/gametdb/`
- `data/openvgdb/openvgdb.sqlite`
- `data/mame/catver.ini`, `data/mame/listxml.xml`
- `data/compendium/enrichment-credentials.json` (IGDB + RetroAchievements)

See [data/compendium/README.md](../../data/compendium/README.md) for schema migrations, incremental `--ingest-source`, and `--enrich-compendium`.

### 9. Package release artifacts

Create distributable CLI archives from the current build:

```bash
../scripts/package_cli_archive.sh
../scripts/package_appimage.sh
```

These write versioned outputs to the repository root `dist/` directory:

- `remus-cli-<version>-linux-x64.tar.gz` (+ `.sha256`)
- `Remus-<version>-x86_64.AppImage` (+ `.sha256`) — bundles `remus-gui` (desktop entry) and `remus-cli`

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

#### Full library pipeline (consumer presets)

```bash
# Scan, hash, match, enrich, bundle, and organize with ES-DE defaults
./remus-cli --library ~/roms --output ~/export --process-preset es-de

# Same pipeline with explicit paths and dry-run preview
./remus-cli --process ~/roms --process-output ~/export --dry-run-all
```

### Metadata Commands

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

### Matching Commands

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
Files by system:
─────────────────────────────────────
  NES                 : 450 files
  SNES                : 320 files
  Unknown             : 5 files
─────────────────────────────────────
  Total: 775 files
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
- **Registration**: <https://www.screenscraper.fr/inscription.php>
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
- Release packaging ships CLI tarballs plus an AppImage with both `remus-gui` and `remus-cli`; install either binary locally via `cmake --install` or from the build tree.
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

- `build/remus-cli` and `build/src/gui/remus-gui` for local development
- `dist/remus-cli-<version>-linux-x64.tar.gz` (+ `.sha256`) for release packaging
- `dist/Remus-<version>-x86_64.AppImage` (+ `.sha256`) — GUI + CLI AppImage

Releases are published manually via the GitHub Actions **Release** workflow (`workflow_dispatch`).

## Contributing

See [../CONTRIBUTING.md](../CONTRIBUTING.md) for the contribution workflow, formatting rules, and required CI checks.

## License

MIT License — see [../../LICENSE](../../LICENSE).
