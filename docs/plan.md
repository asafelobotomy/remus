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
- Offline-first metadata matching through the bundled compendium database.
- Online metadata fallback through Hasheous, ScreenScraper, TheGamesDB, IGDB, RetroAchievements, GameTDB, and Wikidata when configured or available.
- Template-based organization, M3U generation, and archive-aware workflows.
- CHD conversion, verification, patch application, and mod installation flows.
- Artwork download, local cache reuse, and bundle generation.

Historical milestone reports remain in [archive/milestones/README.md](archive/milestones/README.md). Treat them as implementation history, not as the source of truth for the current build surface.

## Focus the next iterations

Near-term priorities:
- Keep archive, patch, and mod workflows safe at trust boundaries.
- Keep documentation aligned with the shipped CLI and GUI surfaces.
- Improve release hygiene, including build and test ergonomics.
- Expand targeted regression coverage when a bug fix changes control flow or file handling.

## Use the current fallback model

The active provider strategy is:

1. Compendium when `data/compendium/remus_compendium.db` is present.
2. Hasheous for no-auth hash matching.
3. ScreenScraper for authenticated hash or name matching.
4. GameTDB and TheGamesDB for system-specific and general name fallback.
5. IGDB, RetroAchievements, and Wikidata when configured and applicable.

See [metadata-providers.md](metadata-providers.md) for the current provider inventory.

## Package the CLI cleanly

The supported build and release path ships both `remus-cli` and `remus-gui` as described in [setup/BUILD.md](setup/BUILD.md). Release packaging includes CLI tarballs and an AppImage with both binaries.
