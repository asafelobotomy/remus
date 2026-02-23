# Remus - Retro Game Library Manager

[![CI](https://github.com/asafelobotomy/remus/actions/workflows/ci.yml/badge.svg)](https://github.com/asafelobotomy/remus/actions/workflows/ci.yml)
[![AppImage](https://github.com/asafelobotomy/remus/actions/workflows/appimage.yml/badge.svg)](https://github.com/asafelobotomy/remus/actions/workflows/appimage.yml)
[![Version](https://img.shields.io/badge/version-0.10.1-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

A desktop application for scanning, organizing, and managing retro game ROM libraries with automatic metadata fetching and smart file organization.

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
- 🖥️ Qt 6 GUI with Library, Match Review, Conversion, Settings views
- 📦 AppImage packaging with auto-update support
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

### Technical Reference
- **[Database Schema](docs/data-model.md)** - SQLite tables and relationships
- **[Requirements Spec](docs/requirements.md)** - Functional and technical requirements
- **[Naming Standards](docs/naming-standards.md)** - No-Intro/Redump conventions
- **[Verification & Patching](docs/verification-and-patching.md)** - DAT files and ROM patching

### Development
- **[Architecture Docs](docs/architecture/)** - Design documents and implementation plans
- **[Milestone Reports](docs/milestones/)** - Detailed completion reports and phase summaries

## Quick Start

**CLI, GUI, and TUI are all fully functional!** See [docs/setup/BUILD.md](docs/setup/BUILD.md) for complete build instructions and usage examples.

```bash
# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Scan and hash ROMs
./remus-cli --scan ~/roms/NES --hash

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
```

### TUI Setup & Run

The terminal UI is optional and requires notcurses at build time.

```bash
# Arch
sudo pacman -S notcurses

# Debian/Ubuntu
sudo apt install libnotcurses-dev

# Fedora
sudo dnf install notcurses-devel

# Configure with TUI enabled
mkdir -p build && cd build
cmake -DREMUS_BUILD_TUI=ON ..
make -j$(nproc)

# Run the terminal UI (from inside build/)
./src/tui/remus-tui

# Or from repo root
cd ..
./build/src/tui/remus-tui
```

If you configure without `-DREMUS_BUILD_TUI=ON`, the `remus-tui` target is not built.

**Requirements:** Qt 6, CMake 3.16+, C++17 compiler (optional C++20 mode supported), zlib, libarchive

**Build performance tip:** See [docs/setup/BUILD.md](docs/setup/BUILD.md) for benchmark-backed build profiles:
- **Fast clean rebuilds:** `PCH=ON` + `UNITY=ON`
- **Fast iterative rebuilds across fresh build dirs:** `CCACHE=ON` + `PCH=OFF`

## Tech Stack

- **UI:** Qt 6 (QML + QtQuick Controls)
- **TUI:** Qt 6 (terminal UI — `remus-tui`)
- **Core:** C++17
- **Database:** SQLite
- **Networking:** QtNetwork
- **Packaging:** AppImage (go-appimage/appimagetool)
- **CI/CD:** GitHub Actions

## Building from Source

See **[docs/setup/BUILD.md](docs/setup/BUILD.md)** for detailed build instructions for Linux, macOS, and Windows.

Quick build:
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./src/ui/remus-gui  # GUI application
./remus-cli --help  # CLI application
# Optional TUI (requires -DREMUS_BUILD_TUI=ON at configure time)
./src/tui/remus-tui
```

## Contributing

Contributions are welcome! To contribute:

1. Check the [project roadmap](docs/plan.md) to see what's being worked on
2. Read the [architecture documentation](docs/architecture/) to understand the codebase
3. Fork the repository and create a feature branch
4. Submit a pull request with clear description of changes

For major changes, please open an issue first to discuss the proposed changes.

## License

MIT License - See LICENSE file for details

## Acknowledgments

Inspired by TinyMediaManager and MediaElch for media organization, and tools like RetroPie and Skraper for retro game metadata management.
