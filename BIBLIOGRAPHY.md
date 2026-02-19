# Bibliography — Remus

Every file in the project is catalogued here. Update this file whenever a file is created, renamed, deleted, or its purpose changes significantly.

## Scaffold files (created by copilot-instructions-template setup on 2026-02-19)

| File | Purpose | LOC |
|------|---------|-----|
| `.github/copilot-instructions.md` | AI agent guidance (Lean/Kaizen methodology + project conventions) | — |
| `.github/agents/setup.agent.md` | Model-pinned Setup agent — Claude Sonnet 4.6 (onboarding & template ops) | — |
| `.github/agents/coding.agent.md` | Model-pinned Coding agent — GPT-5.3-Codex (implementation & refactoring) | — |
| `.github/agents/review.agent.md` | Model-pinned Review agent — Claude Opus 4.6 (code review & architecture) | — |
| `.github/agents/fast.agent.md` | Model-pinned Fast agent — Claude Haiku 4.5 (quick tasks & lookups) | — |
| `.copilot/workspace/IDENTITY.md` | Agent self-description | — |
| `.copilot/workspace/SOUL.md` | Agent values & reasoning patterns | — |
| `.copilot/workspace/USER.md` | Observed user profile | — |
| `.copilot/workspace/TOOLS.md` | Effective tool usage patterns + extension registry | — |
| `.copilot/workspace/MEMORY.md` | Memory system strategy | — |
| `.copilot/workspace/BOOTSTRAP.md` | Permanent setup origin record | — |
| `CHANGELOG.md` | Keep-a-Changelog (pre-existing; setup entry appended) | — |
| `JOURNAL.md` | ADR-style development journal | — |
| `BIBLIOGRAPHY.md` | This file — complete file map | — |
| `METRICS.md` | Kaizen baseline snapshot table | — |

## Project source files (baseline: 2026-02-19)

### CLI (`src/cli/`)

| File | Purpose | LOC |
|------|---------|-----|
| `src/cli/main.cpp` | CLI entry point | — |
| `src/cli/interactive_session.cpp` | Interactive CLI session handler | — |
| `src/cli/interactive_session.h` | Interactive session interface | — |
| `src/cli/terminal_image.h` | Terminal image rendering helper | — |

### Core library (`src/core/`)

| File | Purpose | LOC |
|------|---------|-----|
| `src/core/hasher.cpp` | CRC32/MD5/SHA1 hash computation | — |
| `src/core/hasher.h` | Hash interface | — |
| `src/core/scanner.cpp` | ROM file scanner | — |
| `src/core/scanner.h` | Scanner interface | — |
| `src/core/dat_parser.cpp` | ClrMamePro DAT file parser | — |
| `src/core/dat_parser.h` | DAT parser interface | — |
| `src/core/matching_engine.cpp` | Hash + fuzzy match logic | — |
| `src/core/matching_engine.h` | Matching engine interface | — |
| `src/core/organize_engine.cpp` | File rename/organize logic | — |
| `src/core/organize_engine.h` | Organize engine interface | — |
| `src/core/template_engine.cpp` | Naming template processor | — |
| `src/core/template_engine.h` | Template engine interface | — |
| `src/core/archive_extractor.cpp` | ZIP/7z/RAR extraction | — |
| `src/core/archive_extractor.h` | Archive extractor interface | — |
| `src/core/archive_creator.cpp` | Archive creation | — |
| `src/core/archive_creator.h` | Archive creator interface | — |
| `src/core/chd_converter.cpp` | CHD compression/conversion | — |
| `src/core/chd_converter.h` | CHD converter interface | — |
| `src/core/patch_engine.cpp` | IPS/BPS/UPS/XDelta3 patch application | — |
| `src/core/patch_engine.h` | Patch engine interface | — |
| `src/core/header_detector.cpp` | ROM header detection (NES, SNES, Lynx, etc.) | — |
| `src/core/header_detector.h` | Header detector interface | — |
| `src/core/database.cpp` | SQLite database access layer | — |
| `src/core/database.h` | Database interface | — |
| `src/core/verification_engine.cpp` | DAT-based ROM verification | — |
| `src/core/verification_engine.h` | Verification engine interface | — |
| `src/core/m3u_generator.cpp` | M3U playlist generator for multi-disc games | — |
| `src/core/m3u_generator.h` | M3U generator interface | — |
| `src/core/system_detector.cpp` | System/platform detection from file extension | — |
| `src/core/system_detector.h` | System detector interface | — |
| `src/core/system_resolver.cpp` | System ID resolution and normalisation | — |
| `src/core/system_resolver.h` | System resolver interface | — |
| `src/core/space_calculator.cpp` | Disk space estimation | — |
| `src/core/space_calculator.h` | Space calculator interface | — |
| `src/core/logging_categories.cpp` | Qt logging category definitions | — |
| `src/core/logging_categories.h` | Logging category declarations | — |
| `src/core/constants/constants.h` | Root constants include | — |
| `src/core/constants/api.h` | API endpoint constants | — |
| `src/core/constants/confidence.h` | Confidence score constants | — |
| `src/core/constants/database_schema.h` | SQLite schema constants | — |
| `src/core/constants/engines.h` | Engine parameter constants | — |
| `src/core/constants/errors.h` | Error code constants | — |
| `src/core/constants/hash_algorithms.h` | Hash algorithm identifier constants | — |
| `src/core/constants/match_methods.h` | Match method identifier constants | — |
| `src/core/constants/network.h` | Network timeout/retry constants | — |
| `src/core/constants/providers.h` | Metadata provider identifier constants | — |
| `src/core/constants/settings.h` | Application setting key constants | — |
| `src/core/constants/systems.h` | Retro system identifier constants | — |
| `src/core/constants/templates.h` | Naming template variable constants | — |
| `src/core/constants/ui_theme.h` | UI color palette, typography, layout, and confidence color helpers | 215 |

### Metadata library (`src/metadata/`)

| File | Purpose | LOC |
|------|---------|-----|
| `src/metadata/metadata_provider.cpp` | Base metadata provider implementation | — |
| `src/metadata/metadata_provider.h` | Provider interface | — |
| `src/metadata/provider_orchestrator.cpp` | Multi-provider orchestration with fallback | — |
| `src/metadata/provider_orchestrator.h` | Orchestrator interface | — |
| `src/metadata/metadata_cache.cpp` | 30-day SQLite metadata cache | — |
| `src/metadata/metadata_cache.h` | Cache interface | — |
| `src/metadata/hasheous_provider.cpp` | Hasheous (free hash matching) provider | — |
| `src/metadata/hasheous_provider.h` | Hasheous provider interface | — |
| `src/metadata/screenscraper_provider.cpp` | ScreenScraper provider | — |
| `src/metadata/screenscraper_provider.h` | ScreenScraper provider interface | — |
| `src/metadata/thegamesdb_provider.cpp` | TheGamesDB provider | — |
| `src/metadata/thegamesdb_provider.h` | TheGamesDB provider interface | — |
| `src/metadata/igdb_provider.cpp` | IGDB (Twitch OAuth) provider | — |
| `src/metadata/igdb_provider.h` | IGDB provider interface | — |
| `src/metadata/local_database_provider.cpp` | Local offline database provider | — |
| `src/metadata/local_database_provider.h` | Local database provider interface | — |
| `src/metadata/artwork_downloader.cpp` | Artwork fetch and cache | — |
| `src/metadata/artwork_downloader.h` | Artwork downloader interface | — |
| `src/metadata/rate_limiter.cpp` | Per-provider rate limiting | — |
| `src/metadata/rate_limiter.h` | Rate limiter interface | — |
| `src/metadata/clrmamepro_parser.cpp` | ClrMamePro DAT parser | — |
| `src/metadata/clrmamepro_parser.h` | Parser interface | — |
| `src/metadata/filename_normalizer.cpp` | ROM filename normalisation for matching | — |
| `src/metadata/filename_normalizer.h` | Normalizer interface | — |

### Services (`src/services/`)

| File | Purpose | LOC |
|------|---------|-----|
| `src/services/library_service.cpp` | Library scan and management orchestration | — |
| `src/services/library_service.h` | Library service interface | — |
| `src/services/hash_service.cpp` | Hash computation service | — |
| `src/services/hash_service.h` | Hash service interface | — |
| `src/services/match_service.cpp` | Matching workflow service | — |
| `src/services/match_service.h` | Match service interface | — |
| `src/services/conversion_service.cpp` | CHD/archive conversion service | — |
| `src/services/conversion_service.h` | Conversion service interface | — |
| `src/services/patch_service.cpp` | ROM patching workflow service | — |
| `src/services/patch_service.h` | Patch service interface | — |

### UI — Qt Quick/QML (`src/ui/`)

| File | Purpose | LOC |
|------|---------|-----|
| `src/ui/main.cpp` | Qt GUI entry point | — |
| `src/ui/theme_constants.cpp` | UI theme constant definitions | — |
| `src/ui/theme_constants.h` | UI theme constant declarations | — |
| `src/ui/models/file_list_model.cpp` | QAbstractListModel for file list | — |
| `src/ui/models/file_list_model.h` | File list model interface | — |
| `src/ui/models/match_list_model.cpp` | QAbstractListModel for match results | — |
| `src/ui/models/match_list_model.h` | Match list model interface | — |
| `src/ui/controllers/library_controller.cpp` | Library view QObject controller | — |
| `src/ui/controllers/library_controller.h` | Library controller interface | — |
| `src/ui/controllers/match_controller.cpp` | Match review QObject controller | — |
| `src/ui/controllers/match_controller.h` | Match controller interface | — |
| `src/ui/controllers/conversion_controller.cpp` | Conversion QObject controller | — |
| `src/ui/controllers/conversion_controller.h` | Conversion controller interface | — |
| `src/ui/controllers/settings_controller.cpp` | Settings QObject controller | — |
| `src/ui/controllers/settings_controller.h` | Settings controller interface | — |
| `src/ui/controllers/artwork_controller.cpp` | Artwork QObject controller | — |
| `src/ui/controllers/artwork_controller.h` | Artwork controller interface | — |
| `src/ui/controllers/dat_manager_controller.cpp` | DAT file management controller | — |
| `src/ui/controllers/dat_manager_controller.h` | DAT manager controller interface | — |
| `src/ui/controllers/export_controller.cpp` | Export (RetroArch/ES/CSV/JSON) controller | — |
| `src/ui/controllers/export_controller.h` | Export controller interface | — |
| `src/ui/controllers/metadata_editor_controller.cpp` | Metadata editing controller | — |
| `src/ui/controllers/metadata_editor_controller.h` | Metadata editor interface | — |
| `src/ui/controllers/patch_controller.cpp` | Patching QObject controller | — |
| `src/ui/controllers/patch_controller.h` | Patch controller interface | — |
| `src/ui/controllers/processing_controller.cpp` | Processing pipeline controller | — |
| `src/ui/controllers/processing_controller.h` | Processing controller interface | — |
| `src/ui/controllers/verification_controller.cpp` | ROM verification controller | — |
| `src/ui/controllers/verification_controller.h` | Verification controller interface | — |
| `src/ui/qml/MainWindow.qml` | Root QML window | — |
| `src/ui/qml/LibraryView.qml` | Library browse view | — |
| `src/ui/qml/MatchReviewView.qml` | Match review view | — |
| `src/ui/qml/ConversionView.qml` | CHD/archive conversion view | — |
| `src/ui/qml/SettingsView.qml` | Application settings view | — |
| `src/ui/qml/ArtworkView.qml` | Artwork management view | — |
| `src/ui/qml/ExportView.qml` | Export view | — |
| `src/ui/qml/GameDetailsView.qml` | Game detail view | — |
| `src/ui/qml/PatchView.qml` | ROM patching view | — |
| `src/ui/qml/VerificationView.qml` | ROM verification view | — |
| `src/ui/qml/components/` | Reusable themed QML components (ThemedButton, etc.) | — |

### TUI — Notcurses (`src/tui/`) — optional build target

| File | Purpose | LOC |
|------|---------|-----|
| `src/tui/main.cpp` | TUI entry point | — |
| `src/tui/app.cpp` | TUI application shell | — |
| `src/tui/pipeline.cpp` | TUI processing pipeline | — |
| `src/tui/library_screen.cpp` | Library screen | — |
| `src/tui/match_screen.cpp` | Match review screen | — |
| `src/tui/organize_screen.cpp` | Organize screen | — |
| `src/tui/compressor_screen.cpp` | Compression screen | — |
| `src/tui/patch_screen.cpp` | Patching screen | — |
| `src/tui/options_screen.cpp` | Options screen | — |
| `src/tui/launch_screen.cpp` | Launch/splash screen | — |
| `src/tui/manual_match_overlay.cpp` | Manual match overlay | — |
| `src/tui/main_menu_screen.cpp` | Main menu screen | — |
| `src/tui/widgets/` | TUI widget helpers (progress_bar, text_input, etc.) | — |

### Build & configuration

| File | Purpose | LOC |
|------|---------|-----|
| `CMakeLists.txt` | Root build definition | — |
| `tests/CMakeLists.txt` | Test suite build definition | — |
| `VERSION` | Semver version string | — |
| `README.md` | Project overview and feature list | — |
| `CHANGELOG.md` | Keep-a-Changelog version history | — |
| `scripts/package_appimage.sh` | AppImage packaging script | — |
| `assets/remus.desktop` | Linux desktop entry | — |
