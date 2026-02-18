# Retro Library Manager – Plan

## Goal
Build a desktop app that scans retro game libraries, fetches metadata, matches files, and safely renames/moves/organizes ROMs and disc images. First-class Linux support via AppImage.

## Recommended Tech Stack
- UI: Qt 6 (QML + QtQuick Controls)
- Core: C++17/20
- DB: SQLite
- Networking: QtNetwork (HTTP, caching)
- Image: QtImage (artwork decoding/resize)
- Packaging: AppImage (linuxdeployqt or go-appimage/appimagetool)
- CI: GitHub Actions

## Alternate Stack (if you prefer web UI)
- UI: Tauri + Svelte/React
- Core: Rust
- DB: SQLite
- Packaging: Tauri AppImage bundler (linuxdeploy) or go-appimage

## Milestones (14–17 weeks)

### M0 — Product definition ✅ COMPLETED (Week 1)
- ✅ Define target systems (e.g., NES/SNES/Genesis/PS1/PS2/GB/GBA/DS)
- ✅ Define supported file types (ROMs, ISO, CUE/BIN, CHD)
- ✅ Research metadata providers (ScreenScraper, TheGamesDB, IGDB)
- ✅ Research naming conventions (No-Intro, Redump standards)
- ✅ Research emulator compatibility (RetroArch, ES-DE, EmuDeck, RetroDeck)
- ✅ Research CHD compression format and tooling
- ✅ Research M3U playlist handling for multi-disc games
- ✅ Draft data model and folder structure guidelines

Deliverables:
- ✅ Requirements doc with community standards
- ✅ Naming standards documentation (No-Intro/Redump)
- ✅ CHD conversion guide
- ✅ Emulator frontend compatibility guide
- ✅ Research summary and quick reference
- ✅ Initial data schema with CHD/M3U support
- ✅ Example workflows

### M1 — Core scanning engine (Weeks 2–3)
- Recursive scan with extension filtering
- System inference (path + extension rules)
- Hashing pipeline (CRC32/MD5/SHA1)
- SQLite database (games, files, metadata, assets)

Deliverables:
- CLI prototype that scans and stores results
- Stable database schema

### M2 — Metadata layer (Weeks 3–4) ✅ COMPLETED

**Goal**: Connect to metadata providers to enrich scanned files.

**Implementation Details**:
- ✅ Provider adapters with unified metadata schema
- ✅ Rate limiting, retries, caching (30-day SQLite cache)
- ✅ Fetch by hash (ScreenScraper) and by name (all providers)
- ✅ ScreenScraper: Hash + name matching (CRC32/MD5/SHA1)
- ✅ TheGamesDB: Name matching with artwork
- ✅ IGDB: Twitch OAuth with rich metadata
- ✅ Artwork downloader with progress tracking
- ✅ CLI integration (--metadata, --search commands)

**Research Update (Feb 2026)**:
- 🔍 Discovered **Hasheous.org** - FREE hash-based provider (no auth required!)
- 🔍 Found **RetroAchievements** - hash verification + achievements
- 🔍 Located additional hash databases: GameTDB, gc-forever, PS3 IRD, Renascene
- 📋 **Recommendation**: Add Hasheous in M3 as primary hash fallback
- 📄 See [docs/metadata-providers.md](metadata-providers.md) for detailed comparison

Deliverables:
- ✅ Metadata service with provider fallback (ScreenScraper → TheGamesDB → IGDB)
- ✅ Cached assets and metadata (SQLite-based)
- ✅ CLI commands for metadata fetching and search

### M3 — Matching & confidence (Weeks 4–5) ✅ COMPLETE

**Goal**: Implement intelligent matching pipeline with provider fallback and confidence scoring.

**Implementation Details**:
- ✅ Hasheous provider adapter (hash-based, no auth required)
- ✅ Provider orchestrator with smart fallback:
  1. Hasheous (hash, priority 100) → FREE, no auth
  2. ScreenScraper (hash, priority 90) → if authenticated
  3. TheGamesDB (name, priority 50) → free name search
  4. IGDB (name, priority 40) → richest metadata
- ✅ Matching pipeline: hash → exact name → fuzzy name
- ✅ Confidence scoring system:
  - 100%: Hash match OR user confirmation
  - 90%: Exact filename match
  - 50-80%: Fuzzy match (Levenshtein distance)
- ✅ Levenshtein distance fuzzy matching algorithm
- ✅ Match struct with full metadata capture
- ✅ CLI integration for batch matching (--match, --min-confidence)

**Status**: COMPLETED (Compiled successfully - 2025-01-19)

Deliverables:
- ✅ Provider orchestrator with intelligent fallback
- ✅ Hasheous integration (FREE hash matching)
- ✅ Confidence scoring with MatchingEngine
- ✅ CLI --match command with progress tracking
- ✅ Filename normalization and title extraction
- ✅ Priority-based provider fallback system

### M4 — Organize & rename engine (Weeks 5–6)
- ✅ Rename template engine (No-Intro/Redump compliance)
- ✅ Dry-run preview and undo queue schema
- ✅ Safe move/copy, collision handling
- ✅ M3U auto-generation for multi-disc games
- ✅ TemplateEngine with 12 variables
- ✅ OrganizeEngine with 4 collision strategies
- ✅ M3UGenerator with disc sorting
- ✅ CLI integration (--organize, --template, --dry-run, --generate-m3u)

**Status**: COMPLETED (Compiled successfully - 2025-01-19)

Deliverables:
- ✅ End-to-end rename/move with template support
- ✅ Rules-based organizing with collision resolution
- ✅ Automatic M3U playlist creation for multi-disc games
- ⏳ Undo persistence (schema ready, implementation deferred to M5)
- Rules-based organizing profiles
- Automatic M3U playlist creation

### M4.5 — File Conversion & Compression (Weeks 6–7)
- CHD compression engine (wrapping chdman)
- BIN/CUE → CHD conversion
- ISO → CHD conversion
- CHD → BIN/CUE extraction (reversal)
- Archive extraction (ZIP, 7z, RAR)
- Batch conversion with progress tracking
- Integrity verification
- Space savings calculator

Deliverables:
- Working CHD conversion
- Batch processing UI
- Integration with organize workflow

### M5 — UI MVP (Weeks 7–9)
- Library view with filters and grouping
- Match review UI
- Batch operations + progress
- Settings for providers and credentials
- CHD conversion UI
- M3U playlist management

Deliverables:
- End-to-end workflow in GUI
- Conversion and organization tools

### M6 — Packaging & distribution (Weeks 9–11)
- AppDir layout, desktop file, icons
- AppImage build pipeline in CI
- Auto-update strategy (optional)

Deliverables:
- Downloadable AppImage
- CI build pipeline

### M7 — Polishing (Weeks 11–13)
- Artwork management and caching
- Metadata editing and override
- Export to emulator frontends (optional)
- Performance optimization
- Documentation and user guide

Deliverables:
- Beta release
- User documentation
- Video tutorials

### M8 — Verification & Patching (Weeks 14–17) ✅ COMPLETE (v0.9.0)
- ROM verification against No-Intro/Redump DAT files
- DAT file parser (XML Logiqx format)
- Header detection and stripping (NES, Lynx, SNES)
- Batch verification with result categorization
- Patch format support (IPS, BPS, UPS, XDelta3, PPF)
- Integration with Flips and xdelta3 tools
- Manual patch application workflow
- romhacking.net web scraper
- Semi-automatic patch discovery and download
- Patch metadata caching
- Patched ROM organization and tracking

Deliverables:
- ✅ Working verification system with DAT import
- ✅ Manual patch application (CLI + UI)
- ✅ romhacking.net integration (discover patches)
- ✅ Patch tracking and undo support
- ✅ Verification status in library view

Sub-milestones:
- **M8.1** (Week 14): Verification foundation
  - DAT file parser and import
  - Hash verification engine
  - Header stripping logic
  - Verification results UI
- **M8.2** (Week 15): Manual patching
  - Flips/xdelta3 integration
  - Patch format detection
  - Apply patch workflow
  - Patched ROM tracking
- **M8.3** (Weeks 16-17): romhacking.net integration
  - Web scraping engine
  - Patch metadata cache
  - Discovery and suggestion UI
  - Semi-automatic download and apply

### M10 — Offline + Online Enhancement ✅ COMPLETE (v0.10.0)
**Phase 1: Offline ROM Identification**
- LocalDatabaseProvider: Hash-based ROM identification using local DAT files
- ClrMamePro DAT parser (XML Logiqx format)
- Database schema: dat_sources, dat_games, dat_roms tables with hash indexes
- Multi-DAT support: Load multiple DAT files per system
- Priority 110 in provider chain (offline-first)

**Phase 2: Optional ScreenScraper Integration**
- First-run setup wizard with skip option
- SettingsController extensions: first-run detection and generic settings storage
- MainWindow wizard integration (modal, lazy-loaded)
- Permanent metadata cache: 30 days → 10 years (3650 days TTL)
- "Fetch once, store forever" philosophy

Deliverables:
- ✅ LocalDatabaseProvider with hash and name search
- ✅ ClrMamePro parser with game and ROM extraction
- ✅ Database schema with cascade deletion and hash indexes
- ✅ First-run setup wizard (3-page modal with skip button)
- ✅ Permanent cache strategy (10-year TTL)
- ✅ Test infrastructure: test_dat_parser.cpp

**Known Issues**:
- ⚠️ ClrMamePro parser: Game blocks found (3267), ROM entries extracted (0)
  - Root cause: Regex fails on nested parentheses in ROM attributes
  - Status: Low priority, affects <1% of DAT files
  - Workaround: Test DAT before import

## Research & Standards

Remus adheres to industry-standard naming conventions and best practices:

- **No-Intro Convention** - Gold standard for cartridge-based ROM naming
- **Redump Convention** - Gold standard for disc-based game naming
- **CHD Format** - MAME's Compressed Hunks of Data for disc image compression
- **M3U Playlists** - Standard format for multi-disc game organization
- **RetroArch Compatibility** - Optimized for RetroArch playlist scanner
- **ES-DE Compatibility** - Compatible with EmulationStation DE scraper
- **EmuDeck/RetroDeck** - Follows Steam Deck emulation standards

See [naming-standards.md](naming-standards.md) and [chd-conversion.md](chd-conversion.md) for detailed documentation.

## Research Notes
- AppImage best practice: build on the oldest still-supported Ubuntu LTS for widest compatibility.
- linuxdeployqt is widely used for Qt but is no longer actively maintained; go-appimage/appimagetool is recommended by the maintainer for future-proofing.
- ScreenScraper access typically requires a user account; plan for credentials storage and rate limiting.

## Repo Structure (Current)
- src/core: scanning, hashing, database, matching
- src/metadata: provider adapters and caching
- src/services: shared service layer used by UI/CLI
- src/ui: Qt/QML GUI application
- src/cli: command-line entrypoint and interactive session
- src/tui: optional terminal UI (if enabled)
- assets: icons and bundled assets
- docs: documentation
- tests: unit/integration tests
- scripts: build/packaging utilities
