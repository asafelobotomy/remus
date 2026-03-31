# Remus Plan

## Build the current product

Remus is a CLI-first retro library manager. The active product scans ROM libraries, identifies content with offline and online metadata providers, verifies files against DAT data, applies patches, and organizes outputs safely.

GUI and TUI code remain in the repository as archived frontends. They are not part of the default build, CI, or release path. See [archive/FRONTEND-STATUS.md](archive/FRONTEND-STATUS.md).

## Keep the current stack

- Interface: Qt 6 CLI application
- Core: C++17
- Database: SQLite
- Networking: QtNetwork
- Packaging: CMake builds with release archives
- CI: GitHub Actions

## Track what is already done

Delivered capabilities in the active codebase:
- Recursive scanning with system inference and hash generation.
- Offline-first metadata matching through local DAT databases.
- Online metadata fallback through Hasheous, ScreenScraper, TheGamesDB, IGDB, RetroAchievements, GameTDB, and Wikidata when configured or available.
- Template-based organization, M3U generation, and archive-aware workflows.
- CHD conversion, verification, patch application, and mod installation flows.
- Artwork download, local cache reuse, and bundle generation.

Historical milestone reports remain in [milestones/README.md](milestones/README.md). Treat them as implementation history, not as the source of truth for the current build surface.

## Focus the next iterations

Near-term priorities:
- Keep archive, patch, and mod workflows safe at trust boundaries.
- Keep documentation aligned with the CLI-only build and the shipped commands.
- Improve release hygiene, including build and test ergonomics.
- Expand targeted regression coverage when a bug fix changes control flow or file handling.

## Use the current fallback model

The active provider strategy is:
1. Local DAT databases when local data is present.
2. Hasheous for no-auth hash matching.
3. ScreenScraper for authenticated hash or name matching.
4. GameTDB and TheGamesDB for system-specific and general name fallback.
5. IGDB, RetroAchievements, and Wikidata when configured and applicable.

See [metadata-providers.md](metadata-providers.md) for the current provider inventory.

## Package the CLI cleanly

The supported build and release path is the CLI build described in [setup/BUILD.md](setup/BUILD.md). If frontend work resumes later, restore it as a separate delivery track instead of treating archived code as part of the default product.
