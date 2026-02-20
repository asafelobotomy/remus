# Development Journal — Remus

Architectural decisions and context are recorded here in ADR style.

---

## 2026-02-19 — Project onboarded to copilot-instructions-template

**Context**: This project adopted the generic Lean/Kaizen Copilot instructions template v1.0.3 from [asafelobotomy/copilot-instructions-template](https://github.com/asafelobotomy/copilot-instructions-template). Remus was already a mature codebase (179 source files, 43,400 LOC, 76 CTest tests, v0.9.0) at the time of onboarding.

**Decision**: Use `.github/copilot-instructions.md` as the primary agent guidance document, with `.copilot/workspace/` for session-persistent identity state. Expert-level setup (all 19 questions answered) was chosen to fully configure the agent persona, autonomy, and VS Code integration.

**Preferences captured**: Balanced communication · Intermediate stack experience · Code quality mode · Tests mandatory · Ask only for risky changes · Ecosystem-first dependencies · Free to self-update instructions · Fix proactively · Narrative completion reports · Mentor persona · VS Code auto-apply · High autonomy (4) · Occasional mood lightener.

---

## 2026-02-19 — Full repo health review

**Context**: User requested a full review of the repository — build, tests, deprecations, LOC, dependencies.

**Findings**:
- **[Critical / W7]** `src/core/constants/ui_theme.h` was missing from disk entirely (never committed, referenced in `constants.h` line 27). This caused all targets including `constants.h` to fail to compile (100% of tests blocked).
- **[W1]** `src/ui/qml/LibraryView.qml.backup` — stray backup file in source tree; git history makes it redundant.
- **[W4]** `src/metadata/thegamesdb_provider.cpp:295` — `mapSystemToTheGamesDB()` is marked DEPRECATED; already delegates to `SystemResolver`. The method can be fully removed once all callers are confirmed migrated.
- **[W7]** `src/ui/controllers/processing_controller.cpp:623` — artwork download step is a no-op placeholder; `TODO` left in code.
- **[W6 / W4]** 30 source files exceed the 400-line hard limit (worst: `src/cli/main.cpp` at 1718 lines). Flagged; no extension should occur before decomposition.

**Decision**: Created `src/core/constants/ui_theme.h` (215 LOC) using the namespace and constant names observed in `src/ui/theme_constants.cpp` and `src/ui/models/match_list_model.cpp`. Dark/light palette, confidence colours, typography, layout, and animation constants. Helper functions `getConfidenceColor()` and `getConfidenceBackgroundColor()` added in the `Remus::Constants::UI` namespace.

**Result**: Build: 125/125 targets ✅ · Tests: 38/38 passing ✅ · Type errors: 0 ✅ · LOC (src): 43,683 total.

---

## 2026-02-20 — Full review remediation (all 6 issues resolved)

**Context**: Following the 2026-02-19 health review, all six flagged issues were addressed in a single session.

**Changes**:
1. **[W7] test_clrmamepro_parser.cpp** — Rewrote from a hardcoded debug script into a proper Qt Test class (`ClrMameProParserTest`) with 10 test methods using `QTemporaryFile` fixtures. Registered in `tests/CMakeLists.txt`. Test count increased from 38 → 39.
2. **[W1] mapSystemToTheGamesDB()** — Removed dead deprecated method from `thegamesdb_provider.h` and `thegamesdb_provider.cpp` (was marked DEPRECATED, no callers).
3. **[W7] Raw new/delete in main.cpp** — Replaced two raw `new MetadataProvider` / `delete` patterns in `--metadata` and `--search` CLI handlers with `std::unique_ptr<MetadataProvider>` + `std::make_unique`.
4. **[W1] stepArtwork() placeholder** — Implemented the download step in `processing_controller.cpp`: `stepMatch()` now captures `m_pendingArtworkUrl` and `m_pendingArtworkGameId`; `stepArtwork()` performs the actual download via `ArtworkDownloader`, emits `artworkDownloaded` signal. Added `artworkBasePath` property + `artworkBasePathChanged` signal.
5. **[W7] LibraryView TODO stubs** — Implemented all 6 stubs: remove, match-select, edit-metadata, rematch, screenshot-click, and cover-art lookup. Added `Database::removeFile()` and `LibraryController::removeFile()` Q_INVOKABLE (DB-only deletion, no file-system side effects).
6. **[W6/W4] LOC decomposition of src/cli/main.cpp** — Decomposed 1711-line monolith into 9 focused translation units, each ≤ 400 lines:
   - `cli_helpers.h/cpp` — selectBestHash, hashFileRecord, buildOrchestrator, getHashedFiles, persistMetadata, printFileInfo
   - `cli_logging.h` — internal logging macro redirect
   - `cli_commands.h` — CliContext struct + all handler declarations
   - `cli_commands_info.cpp` — stats, info, header-info, show-art, scan, list, hash-all
   - `cli_commands_metadata.cpp` — metadata, search
   - `cli_commands_match.cpp` — match, match-report
   - `cli_commands_verify.cpp` — checksum-verify, verify
   - `cli_commands_organize.cpp` — artwork, organize, m3u
   - `cli_commands_chd.cpp` — convert-chd, chd-extract, chd-verify, chd-info, extract-archive, space-report
   - `cli_commands_export.cpp` — export, patch operations
   - `main.cpp` reduced to **206 lines** (parser setup + dispatch table)

**Result**: Build: 0 errors ✅ · Tests: 39/39 passing ✅ · Type errors: 0 ✅ · LOC (src): 43,851 total · main.cpp: 1711 → 206 lines.

[session] Full health review complete — see findings in report above.

---

## 2026-02-20 — Coverage gap analysis + 8 new test suites

**Context**: User requested README housekeeping then test coverage review. Analysis identified 8 source files with zero test coverage. All 8 were created and fixed in one session.

**README changes**: Fixed badge URL (`user/remus` → `asafelobotomy/remus`), version (`0.9.0` → `0.10.1`), C++ standard (`C++17/20` → `C++17`), added TUI entry in Tech Stack, updated Quick Start to mention CLI/GUI/TUI.

**New test suites** (all registered in `tests/CMakeLists.txt`):
1. `test_database.cpp` — 14 tests for the 1190-line `Database` CRUD layer (previously zero coverage).
2. `test_verification_engine.cpp` — 9 tests for DAT import, hash-based ROM verification, and missing-game reporting.
3. `test_organize_engine.cpp` — 10 tests for file move/copy/dry-run/collision handling and undo.
4. `test_archive_creator.cpp` — 5 tests for ZIP compression via fake `runProcess` override.
5. `test_m3u_generator.cpp` — 9 tests for multi-disc detection and M3U playlist generation.
6. `test_cli_helpers.cpp` — 8 tests for `selectBestHash`, `getHashedFiles`, `persistMetadata`, `hashFileRecord`, `printFileInfo` (source compiled directly).
7. `test_processing_controller.cpp` — 9 tests for state machine, signal dispatch, stats, and full pipeline integration.
8. `test_export_controller.cpp` — 10 tests for JSON, CSV, RetroArch, EmulationStation, LaunchBox export; `getAvailableSystems`.

**Source bugs uncovered and fixed** (tests exposed real issues):
- **[W7] `organize_engine.cpp`** — `CollisionStrategy::Overwrite` + `FileOperation::Copy` silently failed because `QFile::copy()` refuses to overwrite an existing destination. Added `QFile::remove(newPath)` in the Overwrite collision branch before executing the copy.
- **[W7] `export_controller.cpp` `getAvailableSystems()`** — SQL used non-existent column `g.system`; games table uses `system_id` FK. Fixed to `JOIN systems s ON g.system_id = s.id` with `s.name AS system`.
- **[W7] `export_controller.cpp` `exportToCSV()`** — SQL used four non-existent columns: `g.system` → `s.name`, `g.year` → `g.release_date`, `g.genre` → `g.genres`, `f.filepath` → `f.current_path`, `m.match_type` → `m.match_method`. Fixed all column names and added the `systems` JOIN. (`exportToJSON` had the same wrong columns but silently ignored the query failure — left to a future cleanup pass.)

**Key findings during test writing**:
- `Database::insertFile()` does NOT persist `hash_calculated` — must call `updateFileHashes()` separately.
- `Database::getExistingFiles()` filters by `QFileInfo::exists()` — tests using phantom paths need real temp files.
- Qt AUTOMOC confuses raw string literals with embedded `"` — use concatenated escaped strings in test fixture data.
- MOC is confused by complex inline method bodies inside `private:` class sections — move helpers to file-scope `static` functions.

**Result**: Build: 0 errors ✅ · Tests: 47/47 passing ✅ · Type errors: 0 ✅ · LOC (src): 43,859 total.

---

## 2026-02-20 — Build stack optimization + hashing parallelization

**Context**: User requested implementation of the recommended stack/build improvements without a language/runtime rewrite.

**Changes implemented**:
1. **Build toggles in root CMake** (`CMakeLists.txt`):
   - `REMUS_ENABLE_CCACHE` (default ON): auto-detects `ccache` and sets `CMAKE_CXX_COMPILER_LAUNCHER`.
   - `REMUS_ENABLE_UNITY_BUILD` (default OFF): enables CMake unity/jumbo builds.
   - `REMUS_ENABLE_PCH` (default ON): enables precompiled headers for key targets.
   - `REMUS_ENABLE_CXX20` (default OFF): optional C++20 build mode while keeping C++17 default.
2. **Qt Concurrent integration**:
   - Added `Concurrent` to `find_package(Qt6 REQUIRED COMPONENTS ...)`.
   - Linked `Qt6::Concurrent` in `src/services/CMakeLists.txt`.
3. **PCH rollout**:
   - Added `target_precompile_headers(...)` behind `REMUS_ENABLE_PCH` for `remus-core`, `remus-metadata`, `remus-services`, `remus-ui`, and `remus-cli`.
4. **Hash pipeline performance** (`src/services/hash_service.cpp`):
   - Replaced sequential `hashAll()` loop with `QtConcurrent::blockingMapped` worker fan-out.
   - Bounded concurrency using global threadpool (`min(idealThreadCount, 8)`).
   - Preserved DB writes in a single sequential pass after hashing tasks complete.
   - Preserved cancellation checks, progress callbacks, and log callbacks.

**Docs updated**:
- `docs/setup/BUILD.md`: optional build acceleration flags and C++20 toggle.
- `README.md`: requirements now mention optional C++20 mode.

**Validation**:
- Build: successful ✅
- Focused test: `HashServiceTest` ✅
- Full suite: **47/47 passing** ✅
- Type errors: 0 ✅
- LOC (src): 43,918
