## Plan: TUI Retrospective — Full Remediation

All 15 issues from tui-retrospective.md, organized into 6 phases that build logically on each other. Earlier phases create foundations that later phases depend on.

---

### Phase 1: Foundations (widget layer + threading abstraction)

These two structural refactors reduce code volume by ~40% and unlock testability for Phase 5.

**Step 1.1 — `BackgroundTask` utility class**

Create src/tui/background_task.h — a template wrapping the duplicated thread lifecycle pattern found in three places:
- pipeline.h: `std::thread m_thread` + `std::atomic<bool> m_running`
- compressor_screen.h: `std::thread m_workerThread` + `std::atomic<bool> m_running` + `std::mutex m_progressMutex`
- patch_screen.h: identical pattern

The class provides: `start(fn)`, `cancel()`, `join()`, `running()`, destructor that auto-joins. Holds `std::thread`, `std::atomic<bool>`, `std::mutex` for progress. Accepts a `TuiApp*` for `post()` access.

Then refactor all three files to use `BackgroundTask` instead of raw thread management. The `goto next;` at compressor_screen.cpp gets cleaned up in this step by extracting `processFiles()`'s per-file work into a `processSingleFile(idx)` method, replacing the goto with structured control flow.

**Step 1.2 — Widget library**

Create src/tui/widgets/ with these reusable components:

- **`text_input.h/cpp`** — Encapsulates the text input pattern duplicated 5 times (in `MatchScreen`, `CompressorScreen` ×2, `PatchScreen` ×2, `LibraryScreen`). Handles: cursor rendering with `_`, backspace, printable char insertion, `...` truncation, focus highlight with bg color switch. Common input code at match_screen.cpp, compressor_screen.cpp, library_screen.cpp, etc.

- **`selectable_list.h/cpp`** — Encapsulates j/k/g/G/scroll navigation + selection highlight + scroll clamping. Pattern found 6+ times: match_screen.cpp, compressor_screen.cpp, library_screen.cpp, main_menu_screen.cpp. Supports optional checkboxes (Space to toggle, `a` toggle all) used by Compressor and Patch. Render callback per-row so screens control appearance.

- **`split_pane.h/cpp`** — Encapsulates the 55/45 horizontal split with `│` separator, used by MatchScreen, CompressorScreen, LibraryScreen. Manages two render regions and routes focus.

- **`progress_bar.h/cpp`** — Encapsulates the `[####    ] done/total` bar drawn identically in match_screen.cpp, compressor_screen.cpp, patch_screen.cpp. Accepts `stage`, `done`, `total`, `label`.

- **`toast.h/cpp`** — Transient notification bar (addresses issue #7). Renders at a fixed terminal row (bottom-2). Auto-dismisses after configurable timeout. Three severity levels: info (white), warning (orange), error (red). `TuiApp` owns a `Toast` instance; screens call `m_app.toast("message", level)`.

Add all widget sources to CMakeLists.txt.

Then refactor all 5 screens to use the widgets, eliminating the duplicated rendering and input code. Each screen should shrink significantly (MatchScreen ~750→~350, CompressorScreen ~920→~450, etc.).

---

### Phase 2: Behavior fixes (Esc, filter, confirm/reject, state persistence)

**Step 2.1 — Esc key first-refusal**

The framework at app.cpp already supports this — if `handleInput()` returns `true`, Esc won't pop. The fix is adding Esc handling to each screen:

- `OptionsScreen`: already handles Esc when `m_editing` is true ([options_screen.cpp L59](src/tui/options_screen.cpp#L59)). Add: when not editing and `m_dirty`, Esc shows "unsaved changes" toast; when filter has text and not editing, Esc clears it.
- `LibraryScreen`: Esc clears `m_filter` if non-empty (returns `true`), otherwise falls through to pop.
- `CompressorScreen` / `PatchScreen`: if `m_running`, Esc cancels the operation (returns `true`), otherwise falls through.
- `MatchScreen`: if pipeline running, Esc cancels, otherwise falls through.

**Step 2.2 — In-memory library filter**

In library_screen.cpp, split `loadFromDatabase()` into two methods:
- `loadFromDatabase()` — fetches `db.getAllFiles()` + `db.getAllMatches()` into new `m_rawFiles` / `m_rawMatches` members. Called only from `onEnter()` and `r` refresh.
- `applyFilter()` — filters `m_rawFiles` by `m_filter` and rebuilds the grouped `m_entries` list in-memory. Called from keystroke handlers at library_screen.cpp and library_screen.cpp instead of `loadFromDatabase()`.

The filter should also match against filename and game title (not just system name), addressing the open question from the retrospective.

**Step 2.3 — Confirm/reject visual state + auto-refresh**

In library_screen.h, add a `confirmationStatus` field to `FileEntry` (enum: `Pending`, `Confirmed`, `Rejected`). Populate it from the database in `loadFromDatabase()` — `Database::getMatchForFile()` returns match data that includes confirmation state.

In the list rendering, prefix matches with: `✓` (green) for confirmed, `✗` (red) for rejected, `?` (orange) for pending review.

In `confirmMatch()` / `rejectMatch()` at library_screen.cpp, call `applyFilter()` after the DB write so the display updates immediately instead of requiring manual refresh.

**Step 2.4 — Screen state persistence**

Change `onEnter()` behavior: only load data if the screen's data is empty (first visit). On subsequent returns via `popScreen()`, the existing state (scroll position, selection, filter text) is preserved. Add a `forceRefresh()` method screens can call when they know data is stale.

Affects: `LibraryScreen::onEnter()`, `MatchScreen::onEnter()`, `OptionsScreen::onEnter()`. `CompressorScreen` and `PatchScreen` already have stateless source paths so no change needed there.

---

### Phase 3: New features (help overlay, mouse, tool hints)

**Step 3.1 — Help overlay**

Create src/tui/widgets/help_overlay.h/cpp — a semi-transparent overlay showing keybindings. Each screen provides a `std::vector<std::pair<std::string, std::string>>` of `{key, description}` pairs via a new `virtual std::vector<KeyBinding> keybindings() const` on `Screen`.

In app.cpp, handle `?` globally (before dispatching to screen): if pressed, render the overlay on top of the current screen's output. Press `?` or Esc to dismiss. This is rendered as a centered box on the notcurses stdplane.

**Step 3.2 — Mouse click-to-select**

`notcurses_mice_enable()` is already called at app.cpp. The `ncinput` struct carries `y`/`x` coordinates for press events (`NCTYPE_PRESS`).

In the `SelectableList` widget (from Phase 1), add click handling: when a `NCTYPE_PRESS` event lands within the list bounds, compute the row index from `event.y - listStartY + scrollOffset` and set `m_selected`. This gives click-to-select across all screens that use the widget.

In `TextInput` widget, click sets focus to that input if event lands within its bounds.

**Step 3.3 — Tool installation hints**

In `CompressorScreen` and `PatchScreen`, where tool availability is displayed, add installation hint strings. Create a lookup in src/tui/tool_hints.h:

| Tool | Hint |
|---|---|
| `chdman` | `sudo apt install mame-tools` |
| `flips` | `https://github.com/Alcaro/Flips` |
| `xdelta3` | `sudo apt install xdelta3` |
| `ppf` | `https://github.com/icguy/ppf` |
| `unzip` | `sudo apt install unzip` |
| `7z` | `sudo apt install p7zip-full` |
| `unrar` | `sudo apt install unrar` |

Render the hint in dim grey next to each "Not found" tool line.

---

### Phase 4: Shared service layer

Extract business logic from both GUI controllers and TUI screens into 5 plain C++ service classes in a new src/services/ directory. Services are non-QObject, callback-based, and usable by both frontends.

**Step 4.1 — Create service interfaces**

- src/services/library_service.h/cpp — wraps `Scanner` + `SystemDetector` + `Database` file operations. Methods: `scan(path, progressCb)`, `getStats()`, `getSystems()`, `getFilePath(fileId)`. Replaces logic in `LibraryController::scanDirectory` and `TuiPipeline` scan stage.

- src/services/hash_service.h/cpp — wraps `Hasher` + per-system algorithm lookup + DB hash updates. Methods: `hashAll(db, progressCb)`, `hashFile(db, fileId)`. Replaces `LibraryController::hashFiles` and `TuiPipeline` hashing stage.

- src/services/match_service.h/cpp — wraps `MatchingEngine` + DB match operations. Methods: `matchAll(db, progressCb)`, `matchFile(db, fileId)`, `confirmMatch(db, fileId)`, `rejectMatch(db, fileId)`, `getAllMatches(db)`. Replaces `MatchController` business logic and `TuiPipeline` matching stage plus `LibraryScreen` confirm/reject.

- src/services/conversion_service.h/cpp — wraps `CHDConverter` + `ArchiveExtractor`. Methods: `convertToCHD(path, codec, progressCb)`, `extractCHD(path, progressCb)`, `extractArchive(path, progressCb)`, `batchConvert(paths, progressCb)`, `getToolStatus()`. Replaces `ConversionController` and `CompressorScreen::processFiles`.

- src/services/patch_service.h/cpp — wraps `PatchEngine`. Methods: `detectFormat(path)`, `apply(base, patch, output, progressCb)`, `batchApply(base, patches, progressCb)`, `getToolStatus()`, `setToolPaths(...)`. Replaces `PatchController` and `PatchScreen::applyPatches`.

**Step 4.2 — Create CMake library target**

Add `libremus-services.a` in src/services/CMakeLists.txt, linking against `remus-core`. Add `add_subdirectory(services)` to root CMakeLists.txt.

**Step 4.3 — Refactor TUI to use services**

- `TuiPipeline` becomes a thin orchestrator: `LibraryService::scan` → `HashService::hashAll` → `MatchService::matchAll`
- `CompressorScreen::processFiles` delegates to `ConversionService`
- `PatchScreen::applyPatches` delegates to `PatchService`
- `LibraryScreen` calls `MatchService::confirmMatch/rejectMatch`
- `OptionsScreen` remains direct QSettings (too thin to wrap)

**Step 4.4 — Refactor GUI controllers to use services**

- `LibraryController` → forwards to `LibraryService` + `HashService`
- `MatchController` → forwards to `MatchService`
- `ConversionController` → forwards to `ConversionService`
- `PatchController` → forwards to `PatchService`

GUI controllers retain their `QObject`/`Q_PROPERTY`/`Q_INVOKABLE` surface for QML but delegate all business logic to services. Signal emission stays in the controller adapters.

---

### Phase 5: New core capability (ArchiveCreator) + settings improvement

**Step 5.1 — `ArchiveCreator` class**

Create src/core/archive_creator.h/cpp — mirrors `ArchiveExtractor`'s pattern:
- Enum: `ArchiveFormat { ZIP, SevenZip }`
- Methods: `compress(inputPaths, outputArchive, format)`, `batchCompress(dirs, outputDir, format)`, `cancel()`, `getAvailableTools()`, `canCompress(format)`
- Tool detection: checks for `zip` and `7z` CLI tools on PATH
- Signals (QObject): `compressionStarted`, `compressionProgress`, `compressionCompleted`, `errorOccurred`
- `CompressionResult` struct: `{success, outputPath, error, originalSize, compressedSize}`

Wire into `ConversionService` with new methods: `compressToArchive(paths, output, format, progressCb)`, `batchCompressToArchive(...)`.

Update `CompressorScreen` to add a third mode (Compress → CHD / Compress → Archive / Extract), toggled with `m`.

**Step 5.2 — `ALL_PROVIDER_KEYS` constant**

In settings.h, add:

```
inline constexpr std::array ALL_PROVIDER_KEYS = { ... }
```

containing all 8 provider key constants. The `OptionsScreen` field initialization can then iterate this array instead of hardcoding each field.

---

### Phase 6: Testing

**Step 6.1 — Create TUI library target for testability**

In CMakeLists.txt, split into a static library `libremus-tui-core.a` (all sources except `main.cpp`) and the `remus-tui` executable (just `main.cpp` linking the library). This lets test executables link against `remus-tui-core`.

**Step 6.2 — Widget unit tests**

Create tests/test_tui_widgets.cpp:
- `TextInput`: verify cursor position, backspace, truncation, char insertion
- `SelectableList`: verify j/k navigation, bounds clamping, scroll offset, checkbox toggle
- `ProgressBar`: verify bar fill calculation at 0%, 50%, 100%

These widgets manage state independently of notcurses rendering, so tests validate state transitions only.

**Step 6.3 — Screen logic tests**

Create tests/test_library_screen.cpp:
- Test `loadFromDatabase()` grouping: mock DB data → verify system headers and file entries are interleaved correctly
- Test `applyFilter()`: given loaded data, verify filter on system/filename/title
- Test confirm/reject updates entry status

Create tests/test_tui_pipeline.cpp:
- Test stage transitions: Idle→Scanning→Hashing→Matching→Idle
- Test DB persistence: after pipeline run, verify files and matches are in DB

**Step 6.4 — Service layer tests**

Create tests for each service in Phase 4 — these validate business logic independent of both GUI and TUI:
- tests/test_library_service.cpp
- tests/test_match_service.cpp
- tests/test_conversion_service.cpp
- tests/test_patch_service.cpp

Add all test executables to CMakeLists.txt following the existing pattern: `add_executable` + `target_link_libraries(... Qt6::Test remus-core remus-tui-core)` + `add_test` + append to `run_tests` DEPENDS.

---

### Verification

- **Build**: `cd build && cmake -DREMUS_BUILD_TUI=ON .. && make -j$(nproc)` — clean compile, zero warnings
- **Existing tests**: `cd build && ctest --output-on-failure` — all 33 existing tests still pass
- **New tests**: new TUI + service tests pass via same `ctest` command
- **Manual smoke test**: launch `./build/remus-tui`, navigate all 5 screens, verify:
  - `?` shows keybinding overlay on every screen
  - Esc clears filter/cancels edit before popping
  - Library filter is instant (no DB hit per keystroke)
  - Confirm/reject shows ✓/✗ icons immediately
  - Toast notifications appear and auto-dismiss
  - Mouse click selects list items
  - Compressor has 3 modes (CHD compress / Archive compress / Extract)
  - Missing tools show installation hints
  - Screen state preserved on back-navigation

### Decisions

- **Phase ordering**: Foundations first (Phase 1) because widgets + `BackgroundTask` are prerequisites for clean Phase 2-3 changes and for testability in Phase 6
- **Service layer scope**: All 5 services, full GUI+TUI refactor — chosen over "TUI-only" per user preference
- **ArchiveCreator**: CLI-backed (`zip`/`7z`), not `libarchive` — consistent with existing `ArchiveExtractor` pattern and avoids a new build dependency
- **Filter broadened**: matches system name, filename, AND game title — addresses the open question
- **Esc behavior**: cancel-first semantics (clear filter / cancel edit / cancel operation), pop on second press or when nothing to cancel
