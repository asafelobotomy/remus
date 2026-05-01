# Workflow Workbench — Information Architecture

> Status: Design draft — 2026-05-01
> Target: Replace the 12-page navigation shell with a single primary workspace
> covering the full `scan → hash → match → enrich → package → place` pipeline.

---

## 1. Goal

Users should complete the entire production pipeline from one page without
navigating away. Secondary utilities (DAT import, patching, mods, deep settings)
remain available but leave the main view.

---

## 2. Wireframe

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ TOOLBAR                                                                     │
│ Library: [/path/to/library.db        ] [Open] [Close]   [▶ Run All]        │
└─────────────────────────────────────────────────────────────────────────────┘
┌──────────────┬──────────────────────────────────────────┬───────────────────┐
│ QUEUE        │ PIPELINE                                  │ INSPECTOR         │
│              │                                          │                   │
│ [All    42]  │ ┌─[1. INTAKE]──────────────────────────┐ │  [artwork]        │
│ [Intake   3] │ │ Source: [/roms     ] [Browse] [Scan]  │ │                   │
│ [Identity 8] │ │ ▓▓▓▓░░░ 42 / 100  [■ Stop]           │ │  Super Mario...   │
│ [Enrich   6] │ │ ▼ Recent: rom.zip, disc.iso ...       │ │  Nintendo SNES    │
│ [Package  5] │ └──────────────────────────────────────┘ │  USA · 1996       │
│ [Place   12] │                                          │                   │
│ [Done     8] │ ┌─[2. IDENTITY]────────────────────────┐ │  Match: 96 %      │
│              │ │ [Hash All] [Hash Selected]             │ │  screenscraper    │
│ ─────────── │ │ ▓▓▓░░░ 40 / 100                       │ │  [✓ Confirm]      │
│              │ │ [Match All] [Match Selected]           │ │  [✗ Reject]       │
│ rom_001.zip  │ │ ▓░░░░░  8 / 100  via screenscraper    │ │                   │
│  ● Intake    │ └──────────────────────────────────────┘ │  Title: [_______] │
│              │                                          │  Publisher: [____] │
│ rom_002.iso  │ ┌─[3. ENRICH]──────────────────────────┐ │  Year: [________] │
│  ● Identity  │ │ [Fetch Artwork All] [Fetch Selected]   │ │  Genre: [_______] │
│              │ │ ▓▓▓▓▓░ 60 / 100                       │ │  [Save Changes]   │
│ rom_003.chd  │ │ [3 metadata edits pending] [Save All] │ │                   │
│  ● Enrich    │ └──────────────────────────────────────┘ │  ───────────────  │
│              │                                          │                   │
│ rom_004.bin  │ ┌─[4. PACKAGE]─────────────────────────┐ │  Format: [CHD ▾]  │
│  ● Package   │ │ Format: [CHD ▾]  Output: [_________]  │ │  [Convert]        │
│              │ │ [Convert Selected]  [Bundle Selected]  │ │  → /path/out.chd  │
│              │ │ ▓▓░░░░ 28 %  · 1.4 GB → 620 MB       │ │                   │
│              │ └──────────────────────────────────────┘ │  [Bundle] [M3U]   │
│              │                                          │                   │
│              │ ┌─[5. PLACE]───────────────────────────┐ │  ───────────────  │
│              │ │ Template: [{system}/{title}.{ext}]     │ │                   │
│              │ │ Dest: [/path/organized] [Browse]       │ │  Place preview:   │
│              │ │ [Preview] [Apply ✓] [↺ Undo]          │ │  Nintendo - SNES/ │
│              │ │ 12 ready → /Nintendo - SNES/ ...      │ │  Super Mario...   │
│              │ └──────────────────────────────────────┘ │                   │
├──────────────┴──────────────────────────────────────────┴───────────────────┤
│ ACTIVITY  ▼  [provider:screenscraper] Mario matched 1.2s · 3 warnings       │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Zone Proportions (1480 px default width)

| Zone | Width | Notes |
| ------ | ------- | ------- |
| Queue | 200 px fixed | SplitView min; user-resizable |
| Pipeline | fills remaining | Scrollable ColumnLayout of stage cards |
| Inspector | 280 px fixed | SplitView; collapses when nothing selected |
| Activity | full width, 48 px collapsed | Expandable to ~180 px |

---

## 3. Navigation Model

The Workbench is the default landing view after opening a library. The top-level
navigation shrinks to four items:

| Nav item | Purpose |
| ---------- | --------- |
| **Workflow** (default) | The Workbench — the full pipeline |
| **Library** | Browse all files, bulk select, status overview |
| **Utilities** | DAT import / verify, patching, mods |
| **Settings** | Providers, tools, naming templates, app preferences |

The existing Sidebar is replaced or refactored to render only these four items.
All 12 current pages collapse into these four buckets.

---

## 4. Stage Definitions

### Stage 1 — Intake

**Purpose:** Discover files and add them to the library.

**Actions:** Scan a directory recursively; stop mid-scan.

**Done when:** Every known file has a `FileRecord` row in the database.

**Controller:** `ScanController`

**Key surface:**

- Directory `TextField` + `FolderDialog`
- `[Scan]` / `[■ Stop]` button pair
- `ProgressBar` bound to `scannedFiles / totalFiles`
- Collapsible recent-log `TextArea` from `recentLogs`

---

### Stage 2 — Identity

**Purpose:** Compute hashes then resolve metadata matches.

**Actions:** Hash all/selected → match all/selected via provider orchestrator.

**Done when:** Every file has a non-null `md5`/`sha1` and a confirmed match.

**Controllers:** `HashController` + `MatchController`

**Key surface:**

- Hash row: `[Hash All]` / `[Hash Selected]` + `ProgressBar` (hashedFiles/totalFiles)
- Match row: `[Match All]` / `[Match Selected]` + `ProgressBar` + active provider label
- Inline warning badge when a file has a candidate match awaiting confirmation

---

### Stage 3 — Enrich

**Purpose:** Fill in metadata and fetch artwork.

**Actions:** Fetch artwork for all/selected; edit metadata fields inline.

**Done when:** Every matched file has artwork and no unsaved metadata edits.

**Controllers:** `ArtworkController` + `MetadataEditorController`

**Key surface:**

- Artwork row: `[Fetch All]` / `[Fetch Selected]` + progress
- Pending-edits badge: "N edits unsaved" + `[Save All]`
- Inline metadata editor in the Inspector (title, publisher, year, genre, rating)

---

### Stage 4 — Package

**Purpose:** Convert to the target disc format and/or bundle with metadata sidecar.

**Actions:** Convert format (CHD / RVZ / CSO / PBP); bundle ROM + metadata; export M3U.

**Done when:** Output files are in the chosen format and any requested bundles exist.

**Controllers:** `ConversionController` + `ExportController`

**Key surface:**

- Format selector `ComboBox` + optional output path override
- `[Convert Selected]` / `[Bundle Selected]` + `ProgressBar` (progress 0–100)
- Compression-ratio display from `conversionController.compressionRatio`
- `[Export M3U]` secondary action in Inspector

> **Note:** This stage surfaces `ExportController` for the first time in the
> primary UI. Currently `bundleSelected()` exists in code but has no visible page.

---

### Stage 5 — Place

**Purpose:** Rename and move files into a structured output tree.

**Actions:** Preview destination paths; apply; undo last operation.

**Done when:** All matched files are in the organized destination.

**Controller:** `OrganizeController`

**Key surface:**

- Naming template `TextField` (persisted via `settingsController`)
- Destination path `TextField` + `FolderDialog`
- `[Preview]` → populate organize preview list in Inspector
- `[Apply ✓]` / `[↺ Undo]` pair
- Mini-preview of the computed path for the selected file in the Inspector

---

## 5. Queue Sidebar

The queue answers "where are my files right now?" The filter tabs correspond to
pipeline stage:

| Filter | Meaning |
| -------- | --------- |
| All | Every file in the library |
| Intake | No FileRecord yet / scan pending |
| Identity | Scanned but not hashed, or hashed but unmatched, or match candidate unconfirmed |
| Enrich | Matched/confirmed but missing artwork or has unsaved metadata edits |
| Package | Enriched but not yet in the target format |
| Place | Packaged but not yet in the organized destination |
| Done | Fully through the pipeline |

These filters are derived from database state. They require the new
`WorkflowController` (§7).

---

## 6. Inspector Panel

The Inspector is context-sensitive on `appController.selectedFileId`. It shows:

- Artwork thumbnail (scaled, from `artworkController`)
- Title, system, region, year, genre, publisher — read from `metadataEditorController.currentGame`
- Inline editable fields for title, publisher, year, genre (bound to `metadataEditorController.setField()`)
- Match confidence + provider + `[✓ Confirm]` / `[✗ Reject]` (from `matchController`)
- Format selector + `[Convert]` (from `conversionController`)
- `[Bundle]` / `[Export M3U]` (from `exportController`)
- Place preview path (from `organizeController.previewEntries` filtered to selected file)
- `[Save Changes]` when `metadataEditorController.dirty`

---

## 7. Missing Layer — WorkflowController

**This is the primary new C++ addition required.**

Every pipeline stage is independently functional, but nothing currently:

- Calculates per-file stage state from the database
- Exposes stage-bucketed file counts for the queue sidebar
- Provides a single "run pipeline" action that chains all controllers in order
- Drives a "next suggested action" hint for the selected file

### Proposed `WorkflowController`

```cpp
class WorkflowController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int intakeCount   READ intakeCount   NOTIFY stageCountsChanged)
    Q_PROPERTY(int identityCount READ identityCount NOTIFY stageCountsChanged)
    Q_PROPERTY(int enrichCount   READ enrichCount   NOTIFY stageCountsChanged)
    Q_PROPERTY(int packageCount  READ packageCount  NOTIFY stageCountsChanged)
    Q_PROPERTY(int placeCount    READ placeCount    NOTIFY stageCountsChanged)
    Q_PROPERTY(int doneCount     READ doneCount     NOTIFY stageCountsChanged)
    Q_PROPERTY(bool running      READ isRunning     NOTIFY runningChanged)
    Q_PROPERTY(QString hint      READ hint          NOTIFY hintChanged)

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void runAll();
    Q_INVOKABLE void runFrom(int stage);   // stage enum value
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantList filesAtStage(int stage);

    enum Stage {
        Intake    = 0,
        Identity  = 1,
        Enrich    = 2,
        Package   = 3,
        Place     = 4,
        Done      = 5
    };
    Q_ENUM(Stage)
};
```

`refresh()` queries the database to recount files per stage. It is cheap (one
SQL query per stage using existing columns: `path`, `md5`, `match_id`,
`match_confirmed`, `artwork_path`, `current_path` vs `original_path`).

`runAll()` calls the pipeline controllers in order: scan (if sourceDir is set) →
`hashController.startHashAll()` → `matchController.matchAll()` →
`artworkController.fetchAll()` → `organizeController.applyOrganize()`.

`hint` returns a human-readable prompt for the selected file, e.g.
`"Ready to hash — click Identity ▶"` or `"Match needs confirmation"`.

### Stage derivation logic (per file)

| Condition | Stage |
| ----------- | ------- |
| `file.md5 IS NULL` | Identity |
| `file.match_id IS NULL` | Identity |
| `match.confirmed = 0` | Identity (candidate pending) |
| `file.artwork_path IS NULL` | Enrich |
| `file.current_path = file.original_path AND format ≠ target` | Package |
| `file.current_path` not under destination root | Place |
| otherwise | Done |

> Files that have not been ingested at all cannot appear in the list — they are
> the result of a scan. The "Intake" count therefore tracks how many files the
> last scan reported minus how many have a `FileRecord`. In practice this means
> the Intake filter only shows files that failed to fully register during a scan.
> A simpler approximation is: Intake = 0 after a successful scan completes.

---

## 8. Secondary Pages (Utilities)

These move out of the primary nav into a **Utilities** tab or drawer:

| Utility | Current page | Notes |
| --------- | ------------- | ------- |
| DAT import + FTS search | DatView | Rarely needed after initial setup |
| Verification | VerifyView | Run-once audit tool |
| Patch apply / create | PatchView | On-demand |
| Mod catalog + install | ModView | On-demand |

---

## 9. File Changes Required

### New files

| File | Purpose |
| ------ | --------- |
| `src/gui/controllers/workflow_controller.h` | Declaration |
| `src/gui/controllers/workflow_controller.cpp` | Stage counting, runAll chain |
| `src/gui/qml/views/WorkflowView.qml` | Workbench page |
| `src/gui/qml/components/StageCard.qml` | Reusable collapsible stage section |
| `src/gui/qml/components/InspectorPanel.qml` | Right-side detail area |
| `src/gui/qml/components/QueueSidebar.qml` | Stage-bucketed file queue |

### Modified files

| File | Change |
| ------ | -------- |
| `src/gui/qml/Main.qml` | Replace 12-page StackLayout with 4-item nav; add WorkflowView as index 0 |
| `src/gui/qml/Sidebar.qml` | Reduce to 4 items: Workflow / Library / Utilities / Settings |
| `src/gui/main.cpp` | Instantiate and export `workflowController` |
| `src/gui/CMakeLists.txt` | Add new source files and QML components |
| `src/gui/controllers/app_controller.h` | Add `WorkflowView` to View enum, shrink enum |
| `tests/CMakeLists.txt` | Add `test_workflow_controller` |

### Preserved as-is (moved to Utilities page)

`DatView.qml`, `VerifyView.qml`, `PatchView.qml`, `ModView.qml`

---

## 10. Implementation Sequence

Suggested order to minimize risk and keep the app running throughout:

1. Build `WorkflowController` (C++ only, no QML changes yet) — add stage
   counting and wire into main.cpp. Add unit test.
2. Build `StageCard.qml` component — standalone, no business logic.
3. Build `QueueSidebar.qml` — replaces current Sidebar filter; driven by
   `workflowController` counts.
4. Build `InspectorPanel.qml` — assemble from existing controller bindings.
5. Build `WorkflowView.qml` — combine StageCard × 5 + QueueSidebar +
   InspectorPanel.
6. Update `Main.qml` and `Sidebar.qml` — insert WorkflowView as index 0, reduce
   nav to 4 items, move utilities to Utilities tab.
7. Integrate `runAll()` chain in WorkflowController; add smoke test.
