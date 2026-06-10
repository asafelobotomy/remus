# TinyMediaManager UI Alignment — Research & Remus Recommendations

> Status: Research complete — 2026-06-08  
> Scope: GUI information architecture only (no backend matching changes)  
> References: [TMM Toolbar](https://www.tinymediamanager.org/docs/toolbar), [Quickstart](https://www.tinymediamanager.org/docs/quickstart), [Movie Panel](https://www.tinymediamanager.org/docs/movies/moviepanel), [Movie Renamer](https://www.tinymediamanager.org/docs/movies/renamer)

---

## 1. Executive summary

TinyMediaManager (TMM) treats the **library as the home screen** and exposes the workflow as **four toolbar actions** plus **modal dialogs** for decisions that need context (search/scrape, edit, rename preview). Remus already consolidated many pages into the Workflow Workbench, but still exposes the pipeline as **six accordion stages** in a centre column — which fragments attention and duplicates the Library view.

The highest-value alignment is not visual mimicry but **interaction model parity**:

| TMM pattern | Remus today | Target |
|-------------|-------------|--------|
| Toolbar: Update sources | Scan buried in Stage 1 accordion | **Update library** in top bar |
| Toolbar: Search & Scrape → modal | Match in Stage 2; confirm inline | **Match & enrich dialog** with preview + field toggles |
| Toolbar: Edit | MetadataEditor exists but not prominent | **Edit** opens inspector editor / bulk edit |
| Toolbar: Rename & Cleanup (one action) | Stages 5 + 6 separate; Apply batches both | **Rename & organize** as single action + settings |
| List + status icon columns | Queue badges in sidebar only | **Status columns** on main table |
| Tabbed detail panel | Single scroll InspectorPanel | **Details / Files / Artwork / Match** tabs |

---

## 2. How TMM’s UI achieves its goals

### 2.1 Toolbar as workflow vocabulary

TMM’s [toolbar documentation](https://www.tinymediamanager.org/docs/toolbar) defines four module actions (left) and common actions (right):

1. **Update sources** — Re-scan configured data-source folders; find new/changed/moved files. Does *not* scrape or rename.
2. **Search & Scrape** — For *selected* items; opens a dedicated dialog.
3. **Edit** — Inline/bulk metadata editing for selected items.
4. **Rename & Cleanup** — Applies renamer patterns *and* housekeeping (remove stale NFOs, align sidecar filenames) in one pass.

Each icon is a **primary click**; the label beneath exposes **secondary variants** (e.g. scrape all vs selected, dry-run rename).

**Why it works:** The user always knows *where* they are (the movie list) and *what* they can do next (toolbar). They never hunt through numbered pipeline sections.

### 2.2 Setup once, operate many times

From the [Quickstart](https://www.tinymediamanager.org/docs/quickstart):

- **Data sources** and **scraper defaults** live in Settings.
- First run: add folders → Update sources → import into DB.
- Ongoing: select rows → Search & Scrape → optionally Rename & Cleanup.

Scrape field defaults (title, plot, poster, fanart, …) are configured in Settings but **overridable per run** in the search dialog via tag chips. Automatic scrape can use a **confidence threshold** (1.0 = exact title match) so bulk runs skip the dialog when safe.

### 2.3 Search & Scrape dialog (decision surface)

Documented flow + user screenshots show a single modal that combines:

| Zone | Purpose |
|------|---------|
| **Header** | Active file path — user never loses context |
| **Controls** | Scraper, language, editable search query, Search button |
| **Results table** | Title, year, ID, **match score %** |
| **Preview pane** | Poster + full metadata (plot, cast, ratings) for selected result |
| **Footer tags** | Toggle which fields to import (Title, Plot, Poster, …) |
| **Options** | “Do not overwrite existing data” ([v4.2+](https://www.tinymediamanager.org/blog/scrape-dont-overwrite/)) |
| **Actions** | Cancel / OK (commit scrape) |

**Why it works:** Search, evaluation, and commit happen in one place. The user sees *alternatives* and *preview* before writing anything.

### 2.4 Master–detail main window

The [Movie Panel](https://www.tinymediamanager.org/docs/movies/moviepanel) splits:

- **List (centre-left):** Sortable table with many optional columns, including binary status icons for NFO, artwork, trailer, subtitles.
- **Information (right):** Sub-tabs — Details, Cast, Media files, Artwork, Trailer — so dense data stays reachable without scrolling one giant panel.

List supports **search box** (title) and **Filter popup** (complex filters), not stage-based queue buckets.

### 2.5 Rename & Cleanup as one concept

The [Movie Renamer](https://www.tinymediamanager.org/docs/movies/renamer) doc shows rename is **settings-driven** (JMTE token patterns for folder + filename). The toolbar action applies those patterns and performs cleanup (e.g. remove foreign NFOs, rename sidecars). There is no separate “organize to destination” stage in the main workflow — the data source *is* the destination tree.

CLI mirrors this: `tinyMediaManager movie -u -n -r` = update, scrape new, rename.

---

## 3. Remus GUI — current state

### 3.1 Shell layout

```
Main.qml
├── header: library path + [Open Library]          ← no workflow actions
├── Sidebar: Workflow | Library | Utilities | Settings
└── StackLayout
    ├── WorkflowView   ← primary work surface
    ├── LibraryView    ← duplicate flat list, no inspector
    ├── UtilitiesView  ← DAT / Verify / Patch / Mods tabs
    └── SettingsView
```

Files: `src/gui/qml/Main.qml`, `Sidebar.qml`, `views/WorkflowView.qml`, `views/LibraryView.qml`.

### 3.2 Workflow Workbench (implemented design)

`WorkflowView.qml` implements `docs/plans/workflow-workbench.md`:

```
┌──────────┬─────────────────────────────┬─────────────┐
│ Queue    │ 6× StageCard accordion      │ Inspector   │
│ filters  │ + Run All + Apply           │ (scroll)    │
└──────────┴─────────────────────────────┴─────────────┘
```

| Stage | Controller(s) | User-facing actions |
|-------|---------------|---------------------|
| 1 · Scan | `ScanController` | Directory, Browse, Scan |
| 2 · Hash & Match | `HashController`, `MatchController` | Hash & Match All/Selected; Confirm/Reject |
| 3 · Artwork & Metadata | `ArtworkController` | Enrich All/Selected |
| 4 · Convert | `ConversionController` | Format combo, Convert All/Selected |
| 5 · Bundle & Rename | `ExportController` | Bundle + naming template combo |
| 6 · Organize | `OrganizeController` | Dest dir, Organize, Undo |
| — | `WorkflowController` | Run All Stages, Apply (batch confirm+bundle+organize) |

**Smart behaviours already present:**

- Queue stage filters (`QueueSidebar.qml`) with pipeline badges (Hash&Match, Art&Meta, Bun&Ren, Org, Conv).
- Auto-open stage follows selected file’s pipeline state.
- `InspectorPanel.qml` shows rich metadata, paths, hashes — but only after selection; no tabs.
- Apply dialog confirms batch confirm + bundle + organize.

### 3.3 Gaps vs TMM (UX, not backend)

| Gap | Detail |
|-----|--------|
| **No action toolbar** | Library open/close is the only header control; workflow actions live inside accordion. |
| **Segmented pipeline UI** | Six collapsible stages compete with queue + inspector — 4 horizontal zones on a 1480px window. |
| **No match/search dialog** | `MatchController` auto-picks best match; confirm/reject is inline in Stage 2. No multi-result table, no preview-before-commit, no per-field scrape toggles. |
| **Library view redundant** | `LibraryView` lists files without status columns or detail panel; Workflow queue is the real list. |
| **Rename ≠ Cleanup split** | Bundle & Rename (stage 5) and Organize (stage 6) are separate; TMM unifies filesystem outcomes. |
| **Settings vs inline config** | Naming template in stage 5; organize path in stage 6 — TMM keeps renamer in Settings. |
| **Convert is ROM-specific** | TMM has no analogue; should stay but not dominate the main toolbar (secondary / context menu). |
| **Utilities overlap** | Verify/Patch/DAT fit TMM’s “Tools” menu, not the main pipeline column. |

---

## 4. Recommended alignment (phased)

### Phase A — Toolbar + single home view (medium effort, high impact)

**Goal:** One primary screen; workflow verbs in the header.

Proposed header (after library path):

| Action | Maps to | Default scope |
|--------|---------|---------------|
| **Update library** | `ScanController.startScan(lastDir)` or folder picker if empty | All new files under last scan root |
| **Match & enrich** | Hash → match → artwork (existing controllers) | Selected rows, or selection-empty → prompt |
| **Edit** | `MetadataEditor` / `metadata_editor_controller` | Selected |
| **Rename & organize** | `ExportController` bundle + `OrganizeController` | Selected; patterns from Settings |
| *(right)* Settings, Tools ▼ | Utilities stack, logs | — |

**Merge Workflow + Library:**

- Replace `StackLayout` indices 0 and 1 with one **LibraryView** (TMM movie list).
- Table columns: Title, System, Year, Match %, icons for Matched / Artwork / Converted / Bundled / Organized (reuse queue badge logic).
- Keep `QueueSidebar` filters as **list filters** (toolbar dropdown or left filter chips), not a separate column — or drop the column and use TMM-style Filter popup.

**Remove** centre accordion from default view; keep **Run all** under Tools ▼ → Advanced batch.

Files touched: `Main.qml`, new `ActionToolbar.qml`, refactor `WorkflowView.qml` → `LibraryWorkbench.qml`, deprecate duplicate `LibraryView.qml`.

### Phase B — Match & Enrich dialog (higher effort, highest UX win)

New QML: `dialogs/MatchEnrichDialog.qml` + `MatchSearchModel` (C++ or expose orchestrator candidates to QML).

Dialog structure (TMM parity):

```
┌─ Match & Enrich ─────────────────────────────────────────────┐
│ ROM: /path/to/game.bin                                       │
│ Provider: [Compendium ▼]  System: SNES  Query: [Super Mario] │
│                                              [Search]        │
├──────────────────────┬───────────────────────────────────────┤
│ Results              │ Preview                               │
│ Title    Year  Score │ [box art]                             │
│ Super M… 1990  100%  │ Title, publisher, plot, …           │
├──────────────────────┴───────────────────────────────────────┤
│ Import: [Title×] [Plot×] [Box art×] [Screenshots×] …         │
│ ☐ Do not overwrite existing fields                           │
│                                    [Cancel]  [Apply match]   │
└──────────────────────────────────────────────────────────────┘
```

**Backend prerequisites** (may be incremental):

1. `MatchController` / orchestrator exposes **candidate list** per file (today: single best + confirm).
2. Preview artwork URL before commit (partially exists via `artworkController.previewUrl` after match).
3. Field-level merge policy (maps to enrich + match row updates).

Wire toolbar **Match & enrich** to open dialog for selection; bulk mode uses Settings threshold (like TMM automatic scrape).

### Phase C — Tabbed inspector (low–medium effort)

Refactor `InspectorPanel.qml`:

| Tab | Content |
|-----|---------|
| **Details** | Current metadata block (title, genre, plot, ratings, external IDs) |
| **Files** | ROM path, converted path, bundle path, organized path, hashes, size |
| **Artwork** | Box art, screenshots, download controls |
| **Match** | Method, confidence, provider, confirm/reject, “Search again…” → opens dialog |

Matches TMM Details / Media files / Artwork / (Trailer → N/A for ROMs).

### Phase D — Rename & organize unified (medium effort)

Conceptual merge of stages 5 + 6:

1. Move **naming template** and **organize root** to Settings (like TMM renamer patterns).
2. Single **Rename & organize** action:
   - Preview renames (dry-run list dialog — TMM renamer preview).
   - Apply: bundle/rename sidecars + move to `{organizeRoot}/{system}/{title}/…`.
3. Retain **Undo** for organize moves (`OrganizeController.undoLast`).

Remus differs from TMM: organize often targets a **library root outside the scan folder**. Keep dest path in Settings, not per-session text field in stage 6.

**Convert** stays a context action (right-click / Files tab) — no TMM equivalent.

### Phase E — Utilities → Tools menu

Map `UtilitiesView` tabs to a **Tools ▼** menu:

- Import DAT / Verify ROM / Apply patch / Mod catalog

Frees sidebar to: **Library | Settings** only (TMM: module icons + settings gear).

---

## 5. Mapping table — TMM ↔ Remus

| TMM concept | Remus controller / CLI | Proposed UI home |
|-------------|------------------------|------------------|
| Data source folders | Scan directory + library DB | Settings → Library paths |
| Update sources | `--scan` / `ScanController` | Toolbar **Update library** |
| Search & Scrape | `--match`, `--enrich` / Hash+Match+Artwork | Toolbar → **Match & enrich dialog** |
| Edit metadata | Manual match row / metadata editor | Toolbar **Edit** + inspector |
| Rename & Cleanup | `--bundle`, `--organize` / Export+Organize | Toolbar **Rename & organize** |
| Status columns (NFO, poster, …) | DB flags + queue badges | Table icon columns |
| Scrape field defaults | Provider + enrich config | Settings; overridden in dialog |
| Automatic scrape threshold | Match confidence / hash hit | Settings → Match automation |
| CLI batch | `movie -u -n -r` | `remus-cli scan && match && bundle && organize` (unchanged) |

---

## 6. What not to copy

| TMM behaviour | Remus reason to differ |
|---------------|------------------------|
| Rename only inside data source | Remus organizes into emulator-specific trees (`Remus Library/{system}/…`) |
| No convert stage | ROM formats (CHD, CSO, WBFS) need explicit conversion |
| TV vs Movie modules | Remus: system-aware ROM pipeline + verification/patch utilities |
| NFO-centric storage | Remus: SQLite library DB + optional export |

---

## 7. Wireframe — target layout

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│ [/path/library.db] [Open]  │ Update │ Match&Enrich │ Edit │ Rename&Organize │
│                            │                    Tools ▼  Settings          │
└─────────────────────────────────────────────────────────────────────────────┘
┌────────┬──────────────────────────────────────────────┬────────────────────┐
│ Filter │  ROM table (sortable, status icon columns)    │ [Details|Files|   │
│ chips  │  Search: [____________]  Filter ▼             │  Artwork|Match]   │
│        │                                               │                   │
│ All    │  Title          System  Year  Match  🎨 📦 📁  │  (tab content)    │
│ Needs  │  Super Mario…   SNES   1990  98%   ✓  ✓  —    │                   │
│ Done   │  ...                                            │                   │
└────────┴──────────────────────────────────────────────┴────────────────────┘
│ Activity ▼  matched Super Mario World via compendium · 1.2s                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

Modal **Match & enrich** opens on toolbar click (Phase B). **Run all stages** moves to Tools → Advanced batch.

---

## 8. Implementation order (suggested)

| Priority | Item | Effort | User impact |
|----------|------|--------|-------------|
| P1 | Action toolbar + merge Workflow/Library list | M | Immediate clarity |
| P2 | Status columns on main table | S | At-a-glance completeness |
| P3 | Tabbed inspector | M | Less scroll; TMM-like detail |
| P4 | Match & enrich dialog (+ candidate API) | L | Core parity with TMM scrape UX |
| P5 | Rename & organize unified + Settings migration | M | Matches TMM rename/cleanup |
| P6 | Sidebar → Library + Settings; Tools menu | S | Navigation simplification |

Existing plan `docs/plans/workflow-workbench.md` remains valid for **controller wiring** and queue semantics; this document supersedes its **centre-column accordion** as the long-term default layout.

---

## 9. Related docs

- `docs/plans/workflow-workbench.md` — original workbench IA (2026-05-01)
- `docs/archive/UNIFIED-WORKFLOW-DESIGN.md` — pre-workbench unified page proposal
- `docs/reports/ROM-MATCHING-AUDIT.md` — backend matching order (dialog needs candidate API)
- `docs/naming-standards.md` — rename token vocabulary for Settings

---

*Research sources: TMM official docs (toolbar, quickstart, movie panel, renamer, scrape overwrite blog); Remus QML under `src/gui/qml/`; user-provided TMM v5.2 screenshots.*
