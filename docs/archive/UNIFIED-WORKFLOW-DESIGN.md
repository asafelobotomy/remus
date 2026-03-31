# Unified ROM Library Workflow Design

## Executive Summary

**Goal:** Consolidate the fragmented ROM management workflow (Library → Conversions → Match Review) into a single, seamless page where users can scan, extract, convert, and match ROMs without navigating between views.

**Feasibility:** ✅ **Highly achievable** with moderate refactoring. The controllers already exist — we need to compose them into a unified UI with status-aware actions.

---

## Current State Analysis

### Current User Journey (Fragmented)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  CURRENT WORKFLOW (Multiple Pages)                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ① Library Page          ② Conversions Page       ③ Match Review Page     │
│   ┌─────────────────┐     ┌──────────────────┐     ┌──────────────────┐    │
│   │ • Scan Directory│     │ • Browse to file │     │ • Start Matching │    │
│   │ • View ROM list │ ──►?│ • Convert to CHD │ ──►?│ • Confirm matches│    │
│   │ • Hash Files    │     │ • Extract archive│     │ • View confidence│    │
│   └─────────────────┘     └──────────────────┘     └──────────────────┘    │
│                                                                             │
│   User must:                                                                │
│   • Navigate manually between pages                                         │
│   • Re-browse for files on Conversions page (no connection to Library)     │
│   • Not clear which ROMs need extraction vs matching                        │
│   • No visual pipeline showing workflow progress                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Problems with Current Design

| Issue | Impact |
|-------|--------|
| **Disconnected pages** | User loses context switching between views |
| **Conversions page has no ROM list** | Must manually browse to each file |
| **No workflow status** | User doesn't know which step comes next |
| **No batch operations** | Can only process one file at a time |
| **Hidden dependencies** | Matching requires hashes, but this isn't obvious |

---

## Proposed Unified Design

### New User Journey (Single Page)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  UNIFIED LIBRARY PAGE                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─ Header Actions ─────────────────────────────────────────────────────┐  │
│  │ [Scan Directory]  [Process All]  [Auto-Workflow ▼]     Filter: [All]  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│  ┌─ Pipeline Status Bar ────────────────────────────────────────────────┐  │
│  │ ● Scanned: 45  ● Archives: 12  ● Need CHD: 8  ● Matched: 30  ◐ 67%  │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│  ┌─ ROM List with Inline Actions ───────────────────────────────────────┐  │
│  │ ┌──────────────────────────────────────────────────────────────────┐ │  │
│  │ │ ◉ Silent Hill (USA)          .CUE/.BIN   650 MB   PlayStation    │ │  │
│  │ │    Status: ✓ Extracted  ○ Needs CHD  ○ Not Matched               │ │  │
│  │ │    [Convert to CHD]  [Match]                                      │ │  │
│  │ ├──────────────────────────────────────────────────────────────────┤ │  │
│  │ │ ◉ Sonic The Hedgehog        .MD          512 KB   Genesis        │ │  │
│  │ │    Status: ✓ Extracted  — N/A CHD  ✓ Matched (98%)               │ │  │
│  │ │    [View Match Details]                                           │ │  │
│  │ ├──────────────────────────────────────────────────────────────────┤ │  │
│  │ │ ◉ Final Fantasy VII         .7z (archive) 1.2 GB  PlayStation    │ │  │
│  │ │    Status: ◌ Needs Extraction  ○ —  ○ —                          │ │  │
│  │ │    [Extract Archive]  [Delete]                                    │ │  │
│  │ └──────────────────────────────────────────────────────────────────┘ │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
│  ┌─ Detail Panel (when item selected) ──────────────────────────────────┐  │
│  │  Game: Silent Hill (USA)                                             │  │
│  │  Files: 2 (Silent Hill (USA).cue, Silent Hill (USA).bin)            │  │
│  │  Path: /home/user/roms/psx/Silent Hill (USA).7z                     │  │
│  │  System: Sony PlayStation                                            │  │
│  │                                                                       │  │
│  │  ═══ Workflow Steps ═══                                              │  │
│  │  ✓ 1. Scanned & Detected                                            │  │
│  │  ✓ 2. Archive Extracted                                             │  │
│  │  ○ 3. Convert to CHD (optional)        [Convert]                    │  │
│  │  ○ 4. Calculate Hash                   [Hash Now]                   │  │
│  │  ○ 5. Match with Database              [Start Match]                │  │
│  │                                                                       │  │
│  │  ═══ Match Results (when available) ═══                              │  │
│  │  Matched: Silent Hill (Konami, 1999)                                 │  │
│  │  Confidence: 95%  Method: Hash Match                                 │  │
│  │  [Confirm Match] [Reject] [Search for Different Match]              │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Visual Mockup Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│ ROM Library                                                      [Scan] [Process All]  │
├─────────────────────────────────────────────────────────────────────────────────────────┤
│ Pipeline: [█████░░░░░] 45% │ Scanned: 12 │ Extracted: 8 │ Hashed: 6 │ Matched: 5       │
├───────────────────────────────────────┬─────────────────────────────────────────────────┤
│          ROM LIST                     │            DETAIL / ACTIONS PANEL              │
│ ┌───────────────────────────────────┐ │ ┌─────────────────────────────────────────────┐│
│ │⬛ Silent Hill (USA)               │◄├►│ Silent Hill (USA)                           ││
│ │  .CUE/.BIN │ PSX │ 650MB │ 2files │ │ │                                             ││
│ │  [🔄 Needs CHD] [❓ Not Matched]   │ │ │ ┌─── Workflow Progress ────────────────┐   ││
│ ├───────────────────────────────────┤ │ │ │ ✅ Scanned         ✅ Extracted       │   ││
│ │⬜ Sonic The Hedgehog              │ │ │ │ ⏳ Convert CHD     ⏳ Hash             │   ││
│ │  .MD │ Genesis │ 512KB            │ │ │ │ ⏳ Match                               │   ││
│ │  [✅ Matched: 98%]                 │ │ │ └─────────────────────────────────────┘   ││
│ ├───────────────────────────────────┤ │ │                                             ││
│ │⬜ FF7.7z (ARCHIVE)                │ │ │ Quick Actions:                              ││
│ │  📦 Archive │ PSX │ 1.2GB         │ │ │ ┌────────────┐ ┌────────────┐              ││
│ │  [⚠️ Needs Extraction]             │ │ │ │ Convert    │ │ Hash Files │              ││
│ ├───────────────────────────────────┤ │ │ │ to CHD     │ │            │              ││
│ │⬜ Castlevania SOTN                │ │ │ └────────────┘ └────────────┘              ││
│ │  .CHD │ PSX │ 450MB               │ │ │ ┌────────────┐ ┌────────────┐              ││
│ │  [✅ CHD] [✅ Matched: 100%]       │ │ │ │ Start      │ │ View Match │              ││
│ └───────────────────────────────────┘ │ │ │ Matching   │ │ Details    │              ││
│                                       │ │ └────────────┘ └────────────┘              ││
│ [Select All] [Extract All Archives]  │ │                                             ││
│ [Convert All to CHD] [Match All]     │ │ Match Preview (if matched):                 ││
│                                       │ │ ┌─────────────────────────────────────────┐││
│                                       │ │ │ 🎮 Silent Hill                          │││
│                                       │ │ │ Publisher: Konami │ Year: 1999          │││
│                                       │ │ │ Region: USA │ Confidence: 95%           │││
│                                       │ │ │                                         │││
│                                       │ │ │ [✓ Confirm] [✗ Reject] [🔍 Re-search]   │││
│                                       │ │ └─────────────────────────────────────────┘││
│                                       │ └─────────────────────────────────────────────┘│
└───────────────────────────────────────┴─────────────────────────────────────────────────┘
```

---

## Data Model for Unified View

### Enhanced FileGroupEntry (already partially implemented)

```cpp
struct FileGroupEntry {
    // Existing fields
    int primaryFileId;
    QString displayName;
    QStringList extensions;
    qint64 totalSize;
    int systemId;
    int fileCount;
    
    // NEW: Workflow status fields
    enum WorkflowState {
        NeedsExtraction,    // Is still inside an archive
        Extracted,          // ROM files available on disk
        NeedsCHDConversion, // Disc image that could be compressed
        CHDConverted,       // Already in CHD format
        NeedsHashing,       // Hash not yet calculated
        Hashed,             // Hash available
        NeedsMatching,      // No match found yet
        Matched,            // Match confirmed
        MatchRejected       // User rejected the match
    };
    
    WorkflowState extractionState;
    WorkflowState conversionState;  
    WorkflowState hashState;
    WorkflowState matchState;
    
    // Match details (if matched)
    int matchConfidence;
    QString matchedTitle;
    QString matchedPublisher;
    int matchedYear;
    QString matchMethod;  // "hash", "name", "fuzzy"
    
    // Source info
    bool isInsideArchive;
    QString archivePath;  // If still in archive
};
```

---

## Implementation Plan

### Phase 1: Enhance FileListModel (Backend)

| Task | Effort | Description |
|------|--------|-------------|
| Add workflow state fields | 2h | Extend `FileGroupEntry` with extraction/hash/match states |
| Query match status from DB | 2h | JOIN with `matches` table to get match info per file |
| Add archive detection | 1h | Flag entries that are still inside archives |
| Add CHD candidacy | 1h | Flag disc-based systems for CHD conversion option |

### Phase 2: Create Unified LibraryView (UI)

| Task | Effort | Description |
|------|--------|-------------|
| Add pipeline status bar | 2h | Show counts: scanned/extracted/hashed/matched |
| Enhance list delegate | 3h | Show status badges and inline actions per row |
| Add detail panel | 4h | Right-side panel with full workflow controls |
| Add batch action buttons | 2h | "Extract All", "Convert All", "Match All" |

### Phase 3: Wire Up Controllers (Integration)

| Task | Effort | Description |
|------|--------|-------------|
| Expose ConversionController | 1h | Make available to unified view |
| Expose MatchController | 1h | Make available to unified view |
| Add batch operations | 3h | Process selected/all files for each action |
| Add auto-workflow mode | 4h | Optional: run full pipeline automatically |

### Phase 4: Simplify Navigation

| Task | Effort | Description |
|------|--------|-------------|
| Remove/hide Conversions nav | 30m | Or convert to "Advanced Tools" |
| Update Match Review | 2h | Make it a detail view within Library, or keep as secondary |
| Update sidebar | 1h | Reflect new consolidated structure |

---

## Sidebar Navigation (Proposed)

### Current (8 items)
```
├─ Library
├─ Match Review       ← Merging into Library
├─ Conversions        ← Merging into Library  
├─ Artwork
├─ Verification
├─ Patching
├─ Export
└─ Settings
```

### Proposed (6 items)
```
├─ Library            ← Now contains full workflow
├─ Artwork            ← Keep (artwork management)
├─ Verification       ← Keep (advanced feature)
├─ Patching           ← Keep (advanced feature)
├─ Export             ← Keep (EmulationStation export)
└─ Settings           ← Keep
```

---

## Workflow State Machine

```
                    ┌──────────────────┐
                    │   SCAN DIRECTORY │
                    └────────┬─────────┘
                             │
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
        ┌──────────┐  ┌──────────┐  ┌──────────────┐
        │ Archive  │  │ Disc Img │  │ Cartridge ROM│
        │ (ZIP/7z) │  │ (CUE/ISO)│  │ (NES/SNES)   │
        └────┬─────┘  └────┬─────┘  └──────┬───────┘
             │             │               │
             ▼             │               │
        ┌──────────┐       │               │
        │ EXTRACT  │       │               │
        └────┬─────┘       │               │
             │             │               │
             └─────────────┼───────────────┤
                           │               │
              ┌────────────┴────┐          │
              ▼                 │          │
        ┌───────────┐           │          │
        │ CONVERT   │ (optional)│          │
        │ TO CHD    │           │          │
        └─────┬─────┘           │          │
              │                 │          │
              └─────────────────┼──────────┤
                                │          │
                    ┌───────────┴──────────┴───────────┐
                    ▼                                  │
              ┌──────────┐                             │
              │  HASH    │                             │
              │ (MD5/CRC)│                             │
              └────┬─────┘                             │
                   │                                   │
                   ▼                                   │
              ┌──────────┐                             │
              │  MATCH   │                             │
              │ (metadata)                             │
              └────┬─────┘                             │
                   │                                   │
                   ▼                                   │
              ┌──────────┐                             │
              │ COMPLETE │ ◄───────────────────────────┘
              │ (Ready)  │   (cartridge ROMs skip CHD)
              └──────────┘
```

---

## Key UI Components Needed

### 1. PipelineStatusBar.qml (New Component)
```qml
// Shows: ● Scanned: 45  ● Archives: 12  ● Need CHD: 8  ● Matched: 30
Rectangle {
    Row {
        StatusPill { label: "Scanned"; count: libraryController.scannedCount; color: "#8ec07c" }
        StatusPill { label: "Archives"; count: libraryController.archiveCount; color: "#fe8019" }
        StatusPill { label: "Need CHD"; count: libraryController.needsChdCount; color: "#83a598" }
        StatusPill { label: "Matched"; count: libraryController.matchedCount; color: "#b8bb26" }
    }
}
```

### 2. Enhanced List Delegate
```qml
ItemDelegate {
    Row {
        // File info (existing)
        Column {
            Label { text: model.displayName }
            Label { text: model.extensions + " | " + model.systemName }
        }
        
        // Status badges (NEW)
        Row {
            StatusBadge { 
                visible: model.isInsideArchive
                text: "📦 Archive"
                action: function() { conversionController.extractArchive(...) }
            }
            StatusBadge {
                visible: model.needsCHD
                text: "💿 Convert CHD"
                action: function() { conversionController.convertToCHD(...) }
            }
            StatusBadge {
                visible: model.needsMatch
                text: "🔍 Match"
                action: function() { matchController.matchFile(...) }
            }
            ConfidenceCircle {
                visible: model.matchConfidence > 0
                value: model.matchConfidence
            }
        }
    }
}
```

### 3. DetailPanel.qml (New Component)
```qml
Rectangle {
    Column {
        // File details
        Label { text: "Game: " + selectedEntry.displayName }
        Label { text: "Files: " + selectedEntry.fileCount }
        Label { text: "System: " + selectedEntry.systemName }
        
        // Workflow checklist
        WorkflowStep { step: 1; label: "Scanned"; done: true }
        WorkflowStep { step: 2; label: "Extracted"; done: selectedEntry.extracted }
        WorkflowStep { step: 3; label: "Converted to CHD"; done: selectedEntry.isCHD; optional: true }
        WorkflowStep { step: 4; label: "Hashed"; done: selectedEntry.hasHash }
        WorkflowStep { step: 5; label: "Matched"; done: selectedEntry.hasMatch }
        
        // Action buttons
        Button { text: "Convert to CHD"; visible: selectedEntry.needsCHD }
        Button { text: "Hash Now"; visible: !selectedEntry.hasHash }
        Button { text: "Start Matching"; visible: !selectedEntry.hasMatch }
        
        // Match result (if available)
        MatchPreview { visible: selectedEntry.hasMatch }
    }
}
```

---

## Estimated Total Effort

| Phase | Tasks | Effort |
|-------|-------|--------|
| Phase 1: Backend | FileListModel enhancements | 6 hours |
| Phase 2: UI | Unified LibraryView | 11 hours |
| Phase 3: Integration | Controller wiring | 9 hours |
| Phase 4: Navigation | Sidebar cleanup | 3.5 hours |
| **Total** | | **~30 hours** |

---

## Summary

### Is it possible?
**Yes, absolutely.** The core functionality already exists in separate controllers. This is primarily a UI composition and data flow exercise.

### Key benefits:
1. **Single page workflow** — no context switching
2. **Visual pipeline status** — user always knows where they are
3. **Inline actions** — operate on files directly from the list
4. **Batch operations** — process all files at once
5. **Clear dependencies** — visual workflow shows what's needed

### What stays the same:
- Core scanning logic (Scanner, SystemDetector)
- CHD conversion logic (CHDConverter)
- Archive extraction logic (ArchiveExtractor)
- Matching logic (MatchController, ProviderOrchestrator)
- Match confirmation logic

### What changes:
- FileListModel gains workflow state tracking
- LibraryView becomes the unified hub
- Detail panel replaces separate Match Review page
- Conversions page becomes optional "Advanced Tools"

---

## Next Steps

1. **Approve this design** — confirm direction
2. **Phase 1** — Enhance FileListModel with workflow states
3. **Phase 2** — Rebuild LibraryView with detail panel and inline actions
4. **Phase 3** — Wire up all controllers to the unified view
5. **Phase 4** — Clean up navigation and remove/hide redundant pages
