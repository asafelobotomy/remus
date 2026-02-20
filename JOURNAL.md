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
