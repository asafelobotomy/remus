# TUI Implementation Retrospective

**Date:** 17 February 2026  
**Scope:** Full TUI build — framework, 5 screens, pipeline rewrite  
**Status:** All screens implemented, build clean, 33/33 tests pass

---

## What Was Built

| Component | File(s) | Lines | Purpose |
|---|---|---|---|
| Screen base class | `src/tui/screen.h` | ~40 | Virtual interface for all screens |
| TuiApp | `src/tui/app.h`, `app.cpp` | ~200 | notcurses context, screen stack, DB, event loop |
| LaunchScreen | `src/tui/launch_screen.h/.cpp` | ~120 | Animated splash, auto-transition |
| MainMenuScreen | `src/tui/main_menu_screen.h/.cpp` | ~200 | 5-item hub, all wired |
| MatchScreen | `src/tui/match_screen.h/.cpp` | ~750 | Split-pane, scan→hash→match pipeline, DB |
| CompressorScreen | `src/tui/compressor_screen.h/.cpp` | ~920 | CHD convert/extract, archive extraction |
| PatchScreen | `src/tui/patch_screen.h/.cpp` | ~750 | IPS/BPS/UPS/XDelta3/PPF application |
| LibraryScreen | `src/tui/library_screen.h/.cpp` | ~630 | DB browser, system grouping, filter, confirm/reject |
| OptionsScreen | `src/tui/options_screen.h/.cpp` | ~370 | QSettings for all credentials + settings |
| Pipeline | `src/tui/pipeline.h/.cpp` | ~200 | Threaded scan→hash→match with DB persistence |
| Entry point | `src/tui/main.cpp` | ~20 | QCoreApplication setup |
| Build config | `src/tui/CMakeLists.txt` | ~30 | All sources, links remus-core/metadata/Qt6/notcurses |

**Total:** 21 files, ~4,200 lines of new code.

---

## What I'd Do Differently

### 1. Shared threading abstraction

Three places (`Pipeline`, `CompressorScreen`, `PatchScreen`) each independently manage `std::thread` + `std::mutex` + `std::atomic<bool>` with nearly identical start/cancel/join patterns. A `BackgroundTask<Result>` utility (or at minimum a base class) would eliminate that duplication and guarantee consistent lifecycle management (join-on-destruct, exception propagation).

**Affected files:**
- `src/tui/pipeline.h` — `std::thread m_thread`
- `src/tui/compressor_screen.h` — `std::thread m_workerThread`
- `src/tui/patch_screen.h` — `std::thread m_workerThread`

### 2. Decouple rendering from logic

Each screen is a monolith — input handling, business logic, and raw notcurses draw calls all in one file. The match screen alone is ~750 lines. A thin "widget" layer (text input, selectable list, split pane, progress bar) would cut each screen to ~200 lines, make them testable in isolation, and improve maintainability.

**Proposed widget layer:**
- `TextInput` — cursor, backspace, truncation, focus highlight
- `SelectableList` — j/k navigation, scroll clamping, selection highlight, optional checkboxes
- `SplitPane` — horizontal split with configurable ratio and separator
- `ProgressBar` — fractional fill with label
- `StatusBar` — transient message with timeout

### 3. Filter without full DB reload

In `library_screen.cpp`, `loadFromDatabase()` is called on every keystroke in the filter box. This fetches the entire files + matches resultset via `db.getAllFiles()` / `db.getAllMatches()` and rebuilds the flat grouped list each time. For a library with thousands of ROMs this will cause visible stutter.

**Fix:** Load data once on `onEnter()`, store the raw records, and re-filter/re-group in-memory when `m_filter` changes. Only call `loadFromDatabase()` on explicit refresh (`r` key).

### 4. Escape key semantics

Esc is consumed at the app level (`app.cpp`) to pop the screen stack. Individual screens have no way to use Esc for "cancel editing" or "clear filter" — it always navigates back.

**Fix:** Give screens first-refusal on Esc via `handleInput()`. Only fall through to `popScreen()` if the screen returns `false` (didn't consume it). This enables:
- Options: Esc cancels edit mode before popping
- Library: Esc clears filter before popping
- Compressor/Patch: Esc cancels running operation before popping

---

## What More Could Be Done

### TUI-specific tests

Zero tests currently cover TUI code. The screen logic (filter, sort, status derivation, entry building) is testable without notcurses — it just needs render calls factored out. Priority test targets:

- `LibraryScreen::loadFromDatabase()` — grouping, filtering, header insertion
- `Pipeline` state machine — scan/hash/match phase transitions, DB persistence
- `OptionsScreen` — field parsing, load/save round-trip
- `CompressorScreen::detectFileType()` — extension-based format detection
- `PatchScreen::scanPatches()` — format detection, validation

### Archive creation

`ArchiveExtractor` is extraction-only. The Compressor screen detects ZIP/7z/RAR but can only extract them. To match the wireframe's "Compress?" intent fully, an `ArchiveCreator` class (backed by `libarchive` or `7z` CLI) would be needed. This would enable:
- Loose ROM files → ZIP/7z archive
- Re-compression (extract + re-archive with better settings)

### Error notification system

Errors currently write to a `statusMsg` string that appears at varying positions per screen. A transient toast/notification bar at a fixed position (bottom of terminal) with auto-dismiss timer would be more discoverable and consistent.

### Help screen / keybinding overlay

No `?` help overlay showing available keybindings for the current screen. Every screen has its own key conventions:

| Key | Match | Library | Compressor | Patch | Options |
|---|---|---|---|---|---|
| Tab | Cycle focus | Cycle focus | Cycle focus | Cycle focus | — |
| j/k | Navigate | Navigate | Navigate | Navigate | Navigate |
| Enter | Select | — | Start | Start | Edit/Save |
| Space | — | — | Toggle check | Toggle check | Toggle |
| f | — | Filter | — | — | — |
| r | — | Refresh | — | — | — |
| m | — | — | Mode toggle | — | — |
| c/x | — | Confirm/Reject | — | — | — |
| s | Start scan | — | Start | Start | Save |

### Mouse support

`notcurses_mice_enable()` is called and scroll events are handled, but click-to-select isn't implemented for list items or input fields.

### Screen state persistence

Every `onEnter()` reloads from scratch (DB, filesystem). Screens don't remember scroll position, selection, or filter when navigated back to via push/pop. The screen stack preserves instances, but `onEnter()` resets state.

---

## Open Questions

### 1. Filter scope in Library

The filter currently matches on **system name only**. Should it also match against filename, game title, or developer? A broader search would be more useful but requires indexing more fields during the filter pass.

### 2. Compressor — archive creation

The wireframe shows "Compress?" — is the goal limited to **CHD conversion** (disc images → CHD), or should the TUI also create **ZIP/7z archives** from loose ROM files? The latter needs a new `ArchiveCreator` class with `libarchive` or CLI tool dependency.

### 3. Screen persistence vs. fresh state

Should screens remember their scroll position, selection, and filter when navigating back? Currently `onEnter()` reloads everything. Options:
- **A)** Always reload (current) — guarantees fresh data, loses user position
- **B)** Preserve state on push/pop — keeps position, may show stale data
- **C)** Preserve state + show "data may be stale, press r to refresh" indicator

### 4. Esc behavior

Should Esc always pop back to the menu, or should it first cancel the current in-screen action (clear filter, exit edit mode, cancel operation) and only pop on a second press?

---

## Observations

### Related to TUI

1. **`getAllMatches()` returns `QMap<int, MatchResult>`** (sorted by file ID), but `getAllFiles()` returns `QVector<FileRecord>` in DB insertion order. Both are keyed on `id` so the join works, but nothing guarantees stable ordering if the DB is modified externally. A single joined query would be more robust.

2. **No visual distinction between confirmed, rejected, and pending-review matches.** `Database::confirmMatch()` and `rejectMatch()` are wired into Library screen, but the list shows the same display for all three states. Adding icons (✓ confirmed / ✗ rejected / ? pending) would make the review workflow clearer.

3. **Header detection in the pipeline** calls `Hasher::calculateHashes()` which internally runs `HeaderDetector`. The pipeline's hash phase iterates files that already have a `system_id` from the scanner, so this works correctly — but it's an implicit dependency worth a code comment.

4. **`CHDConverter` expects `chdman` on PATH**, and `PatchEngine` expects `flips`/`xdelta3`/`ppf`. Both screens check tool availability but neither tells the user *how* to install a missing tool. A one-line hint (e.g., `sudo apt install mame-tools` for chdman) would improve UX significantly.

5. **`goto next;` in `compressor_screen.cpp`** — functional but not idiomatic C++. Could be replaced with a `continue` after restructuring the loop, or by extracting per-file processing into a separate method.

### Unrelated to TUI (general codebase)

6. **GUI and TUI share no controller code.** The GUI has `LibraryController`, `MatchController`, `SettingsController` etc. with rich async logic. The TUI reimplements all of it inline. If the TUI grows, extracting a shared "service" layer (below controllers, above core engines) would prevent the two frontends from diverging.

   ```
   Current:    GUI Controllers ──→ Core Engines
               TUI Screens     ──→ Core Engines  (duplicated logic)

   Proposed:   GUI Controllers ──→ Services ──→ Core Engines
               TUI Screens     ──→ Services ──→ Core Engines
   ```

7. **`ArchiveExtractor` has no creation capability.** It wraps extraction tools (`unzip`, `7z`, `unrar`) but there is no `ArchiveCreator` counterpart. If archive creation is desired anywhere in the project (not just TUI), this gap exists at the core library level.

8. **Settings keys use slash-separated paths** (`"screenscraper/username"`) which QSettings interprets as nested groups. This works but means programmatic iteration over "all provider settings" requires knowing the group names upfront — there's no `Settings::ALL_PROVIDER_KEYS` list.

---

## Summary of Recommended Next Steps

| Priority | Item | Effort |
|---|---|---|
| High | Extract widget layer (TextInput, SelectableList, etc.) | 2-3 days |
| High | Fix library filter to use in-memory filtering | 1 hour |
| High | Fix Esc key first-refusal in screens | 1 hour |
| Medium | Add TUI unit tests (filter, grouping, pipeline) | 1 day |
| Medium | Add confirmed/rejected/pending icons in Library | 2 hours |
| Medium | Add tool installation hints in Compressor/Patch | 1 hour |
| Medium | Extract shared service layer (GUI + TUI) | 3-5 days |
| Low | Help overlay (`?` key) | 3 hours |
| Low | Mouse click-to-select | 4 hours |
| Low | Error toast/notification system | 3 hours |
| Low | Archive creation (`ArchiveCreator`) | 2-3 days |
| Low | Screen state persistence | 2 hours |
