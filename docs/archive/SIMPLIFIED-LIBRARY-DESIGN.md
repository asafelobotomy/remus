# Simplified ROM Library Design

## Document Version
- **Version**: 1.0
- **Date**: February 2026
- **Status**: Proposed

---

## 1. Executive Summary

The current ROM Library interface is overly complex with too many workflow states, badges, and action buttons. This redesign simplifies the user experience to a two-section model (Unprocessed/Processed) with a single "Begin" action that handles the entire pipeline automatically.

### Goals
1. **Reduce cognitive load** - Two clear sections instead of multiple workflow states
2. **Batch-first workflow** - Select multiple ROMs, process all at once
3. **Contextual information** - Sidebar shows relevant details based on selection
4. **Minimal unprocessed display** - Just filename and file types until processed
5. **Rich processed display** - Full metadata, artwork, and details after processing

---

## 2. User Workflow

### Import Flow
```
1. Click "Scan Directory"
2. Select folder containing ROMs
3. ROMs imported (archived or unarchived)
4. New ROMs appear in "Unprocessed" section
5. Each ROM has a checkbox for selection
```

### Processing Flow
```
1. User checks ROMs they want to process
2. Click "Begin" button
3. App performs pipeline for each selected ROM:
   a) Extract archived files (if needed)
   b) Calculate hashes (CRC32, MD5, SHA1)
   c) Match against metadata databases
   d) Download metadata (title, publisher, year, etc.)
   e) Download artwork (cover, screenshots, box art)
   f) Convert to CHD (if disc-based and enabled)
   g) Repack to 7z archive (optional, configurable)
4. Processed ROMs move to "Processed" section
```

### Browse Flow
```
1. Click any ROM to select it
2. Sidebar shows contextual information:
   - Unprocessed: Archive contents, possible matches, hash tools
   - Processed: Full metadata, artwork, file details
```

---

## 3. Main View Layout

### Complete Layout with Sidebar

```
┌────────────────────────────────────────────────────────────────────────────┐
│ ROM Library                                    [Scan Directory]  [Begin]   │
├────────────────────────────────────────────────┬───────────────────────────┤
│                                                │                           │
│ ▼ Unprocessed (3)                [Select All]  │     SIDEBAR               │
│ ┌────────────────────────────────────────────┐ │     (contextual based     │
│ │ ☑ [?] Secret of Mana (USA).zip    .sfc    │ │      on selection)        │
│ │ ☐ [?] Silent Hill (USA).7z     .cue .bin  │ │                           │
│ │ ☐ [?] Sonic The Hedgehog.zip   .md        │ │                           │
│ └────────────────────────────────────────────┘ │                           │
│                                                │                           │
│ ▼ Processed (2)                                │                           │
│ ┌────────────────────────────────────────────┐ │                           │
│ │ [img] Chrono Trigger      SNES  1995  ✓   │ │                           │
│ │ [img] Final Fantasy VII   PS1   1997  ✓   │ │                           │
│ └────────────────────────────────────────────┘ │                           │
│                                                │                           │
│ ═══════════════ Progress ══════════════════   │                           │
│ Idle                                           │                           │
│                                                │                           │
└────────────────────────────────────────────────┴───────────────────────────┘
```

### Header Bar
| Element | Description |
|---------|-------------|
| Title | "ROM Library" |
| Scan Directory | Opens folder picker to import ROMs |
| Begin | Starts processing all checked ROMs |

### Unprocessed Section
| Element | Description |
|---------|-------------|
| Section Header | "▼ Unprocessed (N)" - collapsible |
| Select All | Checkbox to toggle all items |
| List Items | Checkbox + [?] icon + filename + file extensions |

### Processed Section
| Element | Description |
|---------|-------------|
| Section Header | "▼ Processed (N)" - collapsible |
| List Items | Thumbnail + title + system + year + match indicator |

### Progress Bar
| Element | Description |
|---------|-------------|
| Status | "Idle" / "Processing: [filename]..." |
| Progress | Visual progress bar during processing |
| Current Step | "Extracting..." / "Hashing..." / "Matching..." etc. |

---

## 4. Sidebar Designs

### 4.1 Unprocessed ROM Sidebar

When an unprocessed ROM is selected, the sidebar shows:

```
┌─────────────────────────────────┐
│ Secret of Mana (USA).zip        │
│ ┌─────┐                         │
│ │ [?] │  Unprocessed            │
│ └─────┘                         │
├─────────────────────────────────┤
│ Archive Contents                │
│ ─────────────────────────────── │
│ ├── Secret of Mana (USA).sfc    │
│ └── (12.4 MB total)             │
├─────────────────────────────────┤
│ Possible Matches                │
│ ─────────────────────────────── │
│ ┌─────────────────────────────┐ │
│ │ Secret of Mana        93%   │ │
│ │ SNES • 1993 • Square        │ │
│ └─────────────────────────────┘ │
│ ┌─────────────────────────────┐ │
│ │ Seiken Densetsu 2     71%   │ │
│ │ SNES • 1993 • Square        │ │
│ └─────────────────────────────┘ │
│ (matches based on filename)     │
├─────────────────────────────────┤
│ Manual Tools                    │
│ ─────────────────────────────── │
│ [Calculate Hash]                │
│                                 │
│ CRC32: (not calculated)         │
│ MD5:   (not calculated)         │
│ SHA1:  (not calculated)         │
├─────────────────────────────────┤
│ [Preview Contents]              │
│ [Remove from Library]           │
└─────────────────────────────────┘
```

#### Components:
1. **Header**: Filename + unprocessed status indicator
2. **Archive Contents**: List files inside archive with total size
3. **Possible Matches**: Quick name-based search results (preview only)
4. **Manual Tools**: Calculate hash button, hash display
5. **Actions**: Preview, Remove

### 4.2 Processed ROM Sidebar

When a processed ROM is selected, the sidebar shows full metadata:

```
┌─────────────────────────────────┐
│ ┌───────────┐                   │
│ │           │  Secret of Mana   │
│ │  [COVER]  │  ───────────────  │
│ │   ART     │  SNES • 1993      │
│ │           │  Square           │
│ └───────────┘                   │
├─────────────────────────────────┤
│ Description                     │
│ ─────────────────────────────── │
│ Long ago, people lived in       │
│ harmony with the natural        │
│ world and used the power of     │
│ Mana to help their nations      │
│ grow. But when evil forces...   │
│                    (see more)   │
├─────────────────────────────────┤
│ Details                         │
│ ─────────────────────────────── │
│ Publisher:  Square              │
│ Developer:  Square              │
│ Region:     USA                 │
│ Genre:      Action RPG          │
│ Players:    1-3                 │
├─────────────────────────────────┤
│ Rating        ┌───────────────┐ │
│               │  Moby: 8.4    │ │
│               │  #628 / 27.6K │ │
│               └───────────────┘ │
├─────────────────────────────────┤
│ Screenshots                     │
│ ─────────────────────────────── │
│ ┌─────┐ ┌─────┐ ┌─────┐        │
│ │     │ │     │ │     │        │
│ │     │ │     │ │     │        │
│ └─────┘ └─────┘ └─────┘        │
├─────────────────────────────────┤
│ File Info                       │
│ ─────────────────────────────── │
│ Archive:  SecretOfMana.7z       │
│ CRC32:    4A8F2B3C ✓            │
│ Size:     12.4 MB               │
│ Matched:  Hash (100%)           │
├─────────────────────────────────┤
│ [Edit Metadata]                 │
│ [Re-match]                      │
│ [Open Folder]                   │
└─────────────────────────────────┘
```

#### Components:
1. **Header**: Cover art + Title + System + Year + Publisher
2. **Description**: Game description with "see more" expansion
3. **Details**: Publisher, Developer, Region, Genre, Players
4. **Rating**: Moby score with ranking
5. **Screenshots**: Horizontal scrollable gallery
6. **File Info**: Archive path, hash, size, match confidence
7. **Actions**: Edit, Re-match, Open folder

---

## 5. Processing Pipeline

### Pipeline Stages

```
┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
│ Extract  │──▶│  Hash    │──▶│  Match   │──▶│ Metadata │
│ (if zip) │   │ CRC/MD5  │   │  (API)   │   │ (title+) │
└──────────┘   └──────────┘   └──────────┘   └──────────┘
                                                   │
                                                   ▼
┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
│  Done    │◀──│  Repack  │◀──│   CHD    │◀──│ Artwork  │
│          │   │ (opt 7z) │   │(if disc) │   │(download)│
└──────────┘   └──────────┘   └──────────┘   └──────────┘
```

### Stage Details

| Stage | Description | Conditions |
|-------|-------------|------------|
| Extract | Extract files from zip/7z/rar | If file is archived |
| Hash | Calculate CRC32, MD5, SHA1 | Always |
| Match | Query metadata providers | Always |
| Metadata | Download title, publisher, year, description | If match found |
| Artwork | Download cover, screenshots, box art | If match found |
| CHD | Convert to CHD format | If disc-based AND enabled in settings |
| Repack | Compress to 7z archive | If enabled in settings |

### Error Handling

| Error | Behavior |
|-------|----------|
| Extraction fails | Mark as failed, keep in Unprocessed |
| No match found | Keep in Processed with "Unmatched" status |
| Artwork download fails | Continue without artwork, log warning |
| CHD conversion fails | Keep original format, log error |

---

## 6. Data Model Changes

### Database Schema Updates

```sql
-- Add processed flag to files table
ALTER TABLE files ADD COLUMN is_processed INTEGER DEFAULT 0;

-- Add processing status
ALTER TABLE files ADD COLUMN processing_status TEXT DEFAULT 'pending';
-- Values: 'pending', 'processing', 'completed', 'failed'

-- Add failure reason if applicable
ALTER TABLE files ADD COLUMN processing_error TEXT;
```

### FileListModel Changes

```cpp
// New roles
enum FileRole {
    // ... existing roles ...
    IsProcessedRole,
    ProcessingStatusRole,
    IsSelectedRole,        // For checkbox state
    ArchiveContentsRole,   // JSON list of files in archive
};

// New methods
Q_INVOKABLE void setSelected(int fileId, bool selected);
Q_INVOKABLE void selectAll(bool processed);
Q_INVOKABLE QVariantList getSelectedUnprocessed();
```

---

## 7. Settings Integration

### New Settings Options

| Setting | Default | Description |
|---------|---------|-------------|
| Auto-convert to CHD | OFF | Convert disc images automatically |
| Repack to 7z | OFF | Recompress processed ROMs |
| Artwork directory | ~/.remus/artwork | Where to store downloaded artwork |
| Keep original files | ON | Keep originals after processing |

---

## 8. Implementation Phases

### Phase 1: Database & Model Updates (Backend)
**Estimated: 1-2 hours**

Tasks:
- [ ] Add `is_processed` column to files table
- [ ] Add `processing_status` column to files table
- [ ] Update Database class with migration
- [ ] Add `IsProcessedRole`, `IsSelectedRole` to FileListModel
- [ ] Add selection state tracking (QSet<int> m_selectedIds)
- [ ] Add `setSelected()`, `selectAll()`, `getSelectedUnprocessed()` methods
- [ ] Update `groupFiles()` to separate processed/unprocessed

Files to modify:
- `src/core/database.cpp` - Schema migration
- `src/ui/models/file_list_model.h` - New roles and methods
- `src/ui/models/file_list_model.cpp` - Implementation

### Phase 2: New Simplified LibraryView (UI)
**Estimated: 2-3 hours**

Tasks:
- [ ] Create new `SimplifiedLibraryView.qml` (or replace existing)
- [ ] Two-section layout (Unprocessed/Processed)
- [ ] Collapsible section headers with counts
- [ ] Checkbox list items for Unprocessed
- [ ] Rich list items for Processed (with thumbnail)
- [ ] Header bar with "Scan Directory" and "Begin" buttons
- [ ] Progress bar at bottom

Files to create/modify:
- `src/ui/qml/LibraryView.qml` - Complete rewrite
- `src/ui/qml/components/UnprocessedListItem.qml` - New component
- `src/ui/qml/components/ProcessedListItem.qml` - New component
- `src/ui/qml/components/SectionHeader.qml` - New component

### Phase 3: Contextual Sidebar (UI)
**Estimated: 2-3 hours**

Tasks:
- [ ] Create `UnprocessedSidebar.qml` component
  - Archive contents display
  - Possible matches preview (quick search)
  - Manual hash calculator
- [ ] Create `ProcessedSidebar.qml` component
  - Cover art display
  - Metadata display (description, details, rating)
  - Screenshots gallery
  - File info display
- [ ] Context switching based on selection

Files to create:
- `src/ui/qml/components/UnprocessedSidebar.qml`
- `src/ui/qml/components/ProcessedSidebar.qml`
- `src/ui/qml/components/MatchPreviewCard.qml`
- `src/ui/qml/components/ScreenshotGallery.qml`

### Phase 4: Batch Processing Pipeline (Backend + UI)
**Estimated: 3-4 hours**

Tasks:
- [ ] Create `ProcessingController` class
  - Process queue management
  - Sequential pipeline execution
  - Progress signals
- [ ] Implement full pipeline:
  1. Extract (use existing ArchiveExtractor)
  2. Hash (use existing Hasher)
  3. Match (use existing MatchController)
  4. Metadata (use existing ProviderOrchestrator)
  5. Artwork (use existing ArtworkDownloader)
  6. CHD conversion (optional, use existing CHDConverter)
  7. Repack (implement 7z compression)
- [ ] Progress UI integration
- [ ] Mark files as processed on completion

Files to create/modify:
- `src/ui/controllers/processing_controller.h` - New
- `src/ui/controllers/processing_controller.cpp` - New
- `src/ui/main.cpp` - Register controller

### Phase 5: Polish & Integration
**Estimated: 1-2 hours**

Tasks:
- [ ] Wire up all signals between components
- [ ] Add keyboard shortcuts (Space to toggle, Enter to process)
- [ ] Add drag-drop support for importing
- [ ] Update sidebar navigation (simplify if needed)
- [ ] Settings page for new options
- [ ] Error handling and user feedback
- [ ] Testing with various ROM types

---

## 9. Component Dependency Graph

```
LibraryView.qml
├── SectionHeader.qml (Unprocessed)
│   └── [Select All] checkbox
├── ListView (Unprocessed)
│   └── UnprocessedListItem.qml
│       └── CheckBox + filename + extensions
├── SectionHeader.qml (Processed)
├── ListView (Processed)
│   └── ProcessedListItem.qml
│       └── Thumbnail + title + system + year
├── ProgressBar
└── Sidebar
    ├── UnprocessedSidebar.qml (if unprocessed selected)
    │   ├── ArchiveContents
    │   ├── MatchPreviewCard.qml (multiple)
    │   └── HashCalculator
    └── ProcessedSidebar.qml (if processed selected)
        ├── CoverArt
        ├── MetadataDisplay
        ├── ScreenshotGallery.qml
        └── FileInfo
```

---

## 10. Success Criteria

| Criteria | Measurement |
|----------|-------------|
| Simplified UI | Only 2 states visible (Unprocessed/Processed) |
| Single action | One "Begin" button processes all selected |
| Batch selection | Checkboxes work correctly |
| Contextual sidebar | Shows appropriate content based on selection |
| Full pipeline | All 7 stages execute correctly |
| Progress visibility | User can see current processing status |
| Error resilience | Failures don't crash, errors are shown |

---

## 11. Design Decisions

| Question | Decision | Rationale |
|----------|----------|-----------|
| Repack format | **Match original** | If source was .zip, repack as .zip; if .7z, repack as .7z |
| Artwork location | **Alongside ROM files** | Store in same folder as extracted ROM for easy access |
| Unmatched handling | **Keep as Unprocessed** | Manual matching system to be implemented later |
| Batch size | **Sequential (1 at a time)** | Predictable progress, safe memory usage, resumable |
| Original files | **Move to "Original ROMs"** | Create subfolder in same directory, preserve originals |

### Artwork Storage Structure
```
~/ROMs/SNES/
├── Secret of Mana (USA)/
│   ├── Secret of Mana (USA).sfc
│   ├── cover.jpg
│   ├── screenshot_1.jpg
│   ├── screenshot_2.jpg
│   └── box_front.jpg
├── Chrono Trigger (USA)/
│   ├── Chrono Trigger (USA).sfc
│   ├── cover.jpg
│   └── ...
└── Original ROMs/
    ├── Secret of Mana (USA).zip
    └── Chrono Trigger (USA).zip
```

### Processing Strategy
```
For each selected ROM (sequential):
  1. Extract → single ROM at a time
  2. Hash → single file operation
  3. Match → single API request
  4. Metadata → single API request
  5. Artwork → parallel downloads (up to 5 concurrent)
  6. CHD → single conversion (CPU intensive)
  7. Repack → match original format
  8. Move original → to "Original ROMs" folder
  9. Mark as processed
```

---

## 12. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Feb 2026 | Initial design document |
| 1.1 | Feb 2026 | Resolved open questions: match original format, artwork alongside ROMs, sequential processing, originals to subfolder |
