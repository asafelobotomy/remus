# Remus Plan

## Build the current product

Remus is a retro library manager with shared CLI and Qt Quick GUI frontends. The active product scans ROM libraries, identifies content with offline and online metadata providers, verifies files against DAT data, applies patches, and organizes outputs safely.

The active build includes `remus-cli` and `remus-gui`. The legacy TUI remains archived under `archive/gui-tui/`.

## Keep the current stack

- Interface: Qt 6 CLI application plus Qt Quick desktop GUI
- Core: C++17
- Database: SQLite
- Networking: QtNetwork
- Packaging: CMake builds with release archives
- CI: GitHub Actions

## Track what is already done

Delivered capabilities in the active codebase:

- Recursive scanning with system inference and hash generation.
- Offline-first metadata matching through a generated bootstrap compendium DB (`scripts/setup_compendium_db.sh`) and optional full catalog build.
- Compendium multi-disc set topology: ingest, verification (`--verify-set`), catalog-ordered M3U, and GUI completeness badges.
- Online metadata fallback through Hasheous, PlayMatch, ScreenScraper, TheGamesDB, IGDB, RetroAchievements, GameTDB, and Wikidata when configured or available.
- Template-based organization (including multi-disc set subfolders), M3U generation, and archive-aware workflows.
- CHD conversion, verification, patch application, and mod installation flows.
- Artwork download, local cache reuse, and bundle generation.

Historical milestone reports remain in [archive/milestones/README.md](archive/milestones/README.md). Treat them as implementation history, not as the source of truth for the current build surface.

## Focus the next iterations

Near-term priorities:

- Keep archive, patch, and mod workflows safe at trust boundaries.
- Keep documentation aligned with the shipped CLI and GUI surfaces.
- Improve release hygiene, including build and test ergonomics.
- Expand targeted regression coverage when a bug fix changes control flow or file handling.

## Use the offline-first compendium model

Remus separates **build-time** metadata ingestion from **runtime** library work:

1. **Build once:** `bash scripts/init_compendium.sh` produces `data/compendium/remus_compendium.db` with hash signatures, metadata, and local artwork blobs.
2. **Runtime (default):** match, enrich, and organize use the compendium only — no remote providers or API credentials.
3. **Extend incrementally:** `remus-cli --enrich-compendium --enrich-source <name>` merges new sources without a full rebuild.
4. **Legacy online fallback:** pass `--online-fallback` (CLI) or set `metadata/runtime_mode=online-fallback` (GUI) to restore remote gap-fill.

See [OFFLINE-FIRST-COMPENDIUM.md](architecture/OFFLINE-FIRST-COMPENDIUM.md) and [metadata-providers.md](metadata-providers.md).

## Package the CLI cleanly

The supported build and release path ships both `remus-cli` and `remus-gui` as described in [setup/BUILD.md](setup/BUILD.md). Release packaging includes CLI tarballs and an AppImage with both binaries.
