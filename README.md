# Remus - Retro Game Library Manager

[![CI](https://github.com/asafelobotomy/remus/actions/workflows/ci.yml/badge.svg)](https://github.com/asafelobotomy/remus/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.10.1-blue.svg)](CHANGELOG.md)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A command-line application for scanning, organizing, and managing retro game ROM libraries with automatic metadata fetching and smart file organization.

The project is CLI-first. GUI and TUI code remain in the repository as archived frontends while active delivery work focuses on the command-line workflow.

## Features

### ✅ Implemented (M0-M10.1)
- 🎮 Support for 23+ retro gaming systems (NES, SNES, PlayStation, Genesis, etc.)
- 🔍 Intelligent file scanning with hash-based game identification (CRC32/MD5/SHA1)
- 🌐 Multiple metadata providers:
  - **Hasheous** ⭐ - FREE hash matching (no auth required!)
  - **ScreenScraper** - Hash + name matching
  - **TheGamesDB** - Name matching
  - **IGDB** - Rich metadata with Twitch OAuth
- 🎯 Provider orchestration with intelligent fallback
- 📊 Confidence scoring (hash 100%, exact name 90%, fuzzy 50-80%)
- 💾 Local metadata caching (30-day SQLite cache)
- 🚀 Rate limiting per provider
- 📁 Template-based file organization (No-Intro/Redump naming conventions)
- 🎨 Customizable naming templates with 13 variables
- 🔒 Dry-run preview mode before applying changes
- ⚙️ Smart collision handling (Skip/Overwrite/Rename/Ask)
- 💿 M3U playlist auto-generation for multi-disc games
- 💾 CHD compression (30-60% space savings)
- 📦 Archive extraction (ZIP, 7z, RAR)
- ⚡ Centralized constants library (type-safe, 150+ constants)
- 🎨 Artwork management with local caching
- ✏️ Metadata editing and user override
- 📤 Export to RetroArch, EmulationStation, CSV, JSON
- ✅ ROM verification against No-Intro/Redump DAT files
- 🔧 ROM patching support (IPS, BPS, UPS, XDelta3 formats)
- 🎯 Header detection (NES, SNES, Lynx, etc.)

## Project Status

**Current Version:** 0.10.1  
**Milestone:** M10.1 - Offline + Optional Online Metadata ✅ **COMPLETE**

| Milestone | Description | Status |
|-----------|-------------|--------|
| M0 | Product Definition | ✅ |
| M1 | Core Scanning Engine | ✅ |
| M2 | Metadata Layer | ✅ |
| M3 | Matching & Confidence | ✅ |
| M4 | Organize & Rename | ✅ |
| M4.5 | File Conversion | ✅ |
| M5 | UI MVP | ✅ |
| M6 | Constants Library | ✅ |
| M7 | Packaging & CI/CD | ✅ |
| M8 | Polish | ✅ |
| M9 | Verification & Patching | ✅ |
| M10 | Offline + Optional Online Metadata | ✅ |
| M10.1 | First-run setup + permanent cache | ✅ |

## Documentation

**📖 [Complete Documentation Index](docs/README.md)**

### Quick Links
- **[Build Instructions](docs/setup/BUILD.md)** - Build from source
- **[Changelog](CHANGELOG.md)** - Version history
- **[Project Roadmap](docs/plan.md)** - Development milestones
- **[Examples & Workflows](docs/examples.md)** - Practical usage examples
- **[Metadata Providers Guide](docs/metadata-providers.md)** - Provider comparison and setup
- **[Test Data Policy](docs/guides/TEST-DATA-POLICY.md)** - Canonical `roms/` and `test_output/` locations

### Technical Reference
- **[Database Schema](docs/data-model.md)** - SQLite tables and relationships
- **[Requirements Spec](docs/requirements.md)** - Functional and technical requirements
- **[Naming Standards](docs/naming-standards.md)** - No-Intro/Redump conventions
- **[Verification & Patching](docs/verification-and-patching.md)** - DAT files and ROM patching

### Development
- **[Architecture Docs](docs/architecture/)** - Design documents and implementation plans
- **[Milestone Reports](docs/milestones/)** - Detailed completion reports and phase summaries

## Quick Start

**CLI-first build is the default path.** See [docs/setup/BUILD.md](docs/setup/BUILD.md) for the supported build and release flow.

Repository-local path policy:

- Put local ROM inputs under `roms/`.
- Use `test_output/` for small processed-ROM test runs and tracked review notes.
- Keep `test_output/` to 5 active cases or fewer.
- Record anything that needs follow-up in `test_output/attention.log`.

```bash
# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Scan and hash ROMs
./remus-cli --scan ../roms/NES --hash

# Match with intelligent fallback (M3)
./remus-cli --match --min-confidence 70

# Use Hasheous (FREE, no auth!)
./remus-cli --metadata 811b027eaf99c2def7b933c5208636de --provider hasheous

# List results
./remus-cli --list

# NEW in M4: Organize files with No-Intro naming
./remus-cli --organize ~/roms/organized --template "{title} ({region}){ext}"

# Preview changes without modifying files
./remus-cli --organize ~/roms/organized --dry-run

# Generate M3U playlists for multi-disc games
./remus-cli --generate-m3u --m3u-dir ~/roms/playlists

# Use custom Redump template
./remus-cli --organize ~/roms/psx --template "{title} ({region}) (Disc {disc}){ext}"

# Build self-contained bundles with metadata, box art, and a .remus.md marker
./remus-cli --bundle ~/roms/bundles --bundle-format zip

# Reuse previously-downloaded artwork instead of downloading it again
./remus-cli --bundle ~/roms/bundles --bundle-art-dir ~/roms/art-cache

# Browse mod catalogs without a frontend
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-systems
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-author "Test"
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-type translation --mod-min-rating 3.5
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-format ips --mod-min-downloads 800
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-source-url example --json
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-system "Super Nintendo" --mod-sort downloads

# Require exact ROM matches instead of system-level fallback
./remus-cli --mod-catalog tests/fixtures/test_mod_catalog.json --mod-list 42 --mod-no-system-fallback
```

**Requirements:** Qt 6 base development files, CMake 3.16+, C++17 compiler (optional C++20 mode supported), zlib, libarchive

**Build performance tip:** See [docs/setup/BUILD.md](docs/setup/BUILD.md) for benchmark-backed build profiles:
- **Fast clean rebuilds:** `PCH=ON` + `UNITY=ON`
- **Fast iterative rebuilds across fresh build dirs:** `CCACHE=ON` + `PCH=OFF`

## Tech Stack

- **Interface:** CLI (`remus-cli`)
- **Core:** C++17
- **Database:** SQLite
- **Networking:** QtNetwork
- **Packaging:** tar.gz release archives
- **CI/CD:** GitHub Actions

## Building from Source

See **[docs/setup/BUILD.md](docs/setup/BUILD.md)** for detailed build instructions for Linux, macOS, and Windows.

Quick build:
```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/remus-cli --help
```

GUI and TUI sources remain in the repository as archived frontends. They are not part of the default build, CI, or release packaging while CLI delivery is the priority. See [docs/archive/FRONTEND-STATUS.md](docs/archive/FRONTEND-STATUS.md).

## Contributing

Contributions are welcome! To contribute:

1. Check the [project roadmap](docs/plan.md) to see what's being worked on
2. Read the [architecture documentation](docs/architecture/) to understand the codebase
3. Fork the repository and create a feature branch
4. Submit a pull request with clear description of changes

For major changes, please open an issue first to discuss the proposed changes.

## License

MIT License.

## Acknowledgments

Inspired by TinyMediaManager and MediaElch for media organization, and tools like RetroPie and Skraper for retro game metadata management.
