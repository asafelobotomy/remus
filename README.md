# Remus - Retro Game Library Manager

[![CI](https://github.com/asafelobotomy/remus/actions/workflows/ci.yml/badge.svg)](https://github.com/asafelobotomy/remus/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-0.10.1-blue.svg)](CHANGELOG.md)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A C++17 retro game library manager for scanning, organizing, and managing ROM libraries with automatic metadata fetching, verification, conversion, patching, and mod workflows across both the command line and a Qt Quick desktop GUI.

The active build now ships a shared CLI and Qt Quick GUI. The legacy TUI remains archived under `archive/gui-tui/`.

## Features

### Implemented in the active build

- Support for 23+ retro gaming systems across cartridge, optical-disc, and archive workflows.
- Hash-first scanning and matching with CRC32, MD5, and SHA1.
- Provider orchestration with offline and online fallback:
  - Bundled compendium database
  - Hasheous
  - ScreenScraper
  - GameTDB
  - TheGamesDB
  - IGDB
  - RetroAchievements
  - Wikidata
- Template-driven organization with dry-run support and collision handling.
- Archive extraction for ZIP, 7z, and RAR inputs.
- CHD conversion, M3U generation, verification, and patching workflows.
- Artwork download and local bundle generation.
- SQLite-backed metadata and cache storage.

## Project Status

Current version: 0.10.1

Remus now builds `remus-cli` and `remus-gui` by default from the shared C++17/Qt 6 codebase. The legacy TUI remains preserved under `archive/gui-tui/` as historical reference code.

Current delivery focus:

- CLI workflow coverage
- metadata and organization reliability
- verification, patching, and mod support
- documentation and build hygiene

Historical milestone reports remain in [docs/archive/milestones/](docs/archive/milestones/).

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
- **[Verification & Patching](docs/verification-and-patching.md)** - Compendium-backed verification and ROM patching

### Development

- **[Architecture Docs](docs/architecture/)** - Design documents and implementation plans
- **[Milestone Reports](docs/archive/milestones/)** - Detailed completion reports and phase summaries

## Quick Start

**The default configure builds both `remus-cli` and `remus-gui`.** See [docs/setup/BUILD.md](docs/setup/BUILD.md) for the supported build and release flow.

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

# Launch the desktop GUI
./src/gui/remus-gui

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

- **Interface:** CLI (`remus-cli`) and Qt Quick GUI (`remus-gui`)
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
./build/src/gui/remus-gui
```

The active build produces both the CLI and the Qt Quick desktop GUI. The legacy TUI remains archived under `archive/gui-tui/`.

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
