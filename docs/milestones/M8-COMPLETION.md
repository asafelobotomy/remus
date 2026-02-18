# M8: Polish (Artwork, Metadata Editing, Export) - COMPLETE

**Version**: 0.8.0  
**Date**: 2025-02-05  
**Status**: ✅ Complete

## Overview

M8 delivers the "polish" milestone, transforming Remus from a functional ROM manager into a user-friendly application with artwork management, metadata editing, and export capabilities for major emulator frontends.

## Components Implemented

### 1. Artwork Management

**Files Created:**
- `src/ui/controllers/artwork_controller.h` (116 lines)
- `src/ui/controllers/artwork_controller.cpp` (396 lines)
- `src/ui/qml/ArtworkView.qml` (305 lines)

**Features:**
- Local artwork caching at `~/.local/share/Remus/artwork/`
- Organized by type: `boxart/`, `screenshots/`, `banners/`, `logos/`
- Batch download with cancellation support
- Progress tracking with signals
- Statistics: total games, with artwork, missing, storage used

**API (Q_INVOKABLE):**
```cpp
QString getArtworkPath(int gameId, const QString &type);
QUrl getArtworkUrl(int gameId, const QString &type);
bool hasLocalArtwork(int gameId, const QString &type);
void downloadArtwork(int gameId, const QStringList &types);
void downloadAllArtwork(const QString &systemFilter, bool overwrite);
void cancelDownloads();
QVariantMap getArtworkStats();
QVariantList getGamesMissingArtwork(const QString &type, int limit);
bool deleteArtwork(int gameId, const QString &type);
void clearArtworkCache();
```

### 2. Metadata Editing

**Files Created:**
- `src/ui/controllers/metadata_editor_controller.h` (113 lines)
- `src/ui/controllers/metadata_editor_controller.cpp` (476 lines)
- `src/ui/qml/GameDetailsView.qml` (378 lines)

**Features:**
- View and edit game metadata (title, region, year, publisher, developer, genre, players)
- Track pending changes with save/discard workflow
- View metadata provenance (which provider contributed what)
- Match confirmation (user confirm/reject automatic matches)
- Manual match creation for unmatched files
- Search by title or filter by system

**API (Q_INVOKABLE):**
```cpp
QVariantMap getGameDetails(int gameId);
QVariantList getMetadataSources(int gameId);
bool updateField(int gameId, const QString &field, const QVariant &value);
bool saveChanges();
void discardChanges();
void resetField(int gameId, const QString &field);
QVariantList getGameFiles(int gameId);
QVariantMap getMatchInfo(int fileId);
QVariantList searchGames(const QString &query, int limit);
QVariantList getGamesBySystem(const QString &system, int limit);
bool setMatchConfirmation(int matchId, bool confirm);
int createManualMatch(int fileId, const QString &title, const QString &system);
```

### 3. Export to Emulator Frontends

**Files Created:**
- `src/ui/controllers/export_controller.h` (118 lines)
- `src/ui/controllers/export_controller.cpp` (648 lines)
- `src/ui/qml/ExportView.qml` (365 lines)

**Export Formats:**

#### RetroArch Playlists (`.lpl`)
```json
{
  "version": "1.5",
  "items": [
    {
      "path": "/roms/nes/Super Mario Bros. (USA).nes",
      "label": "Super Mario Bros.",
      "crc32": "3337EC46|crc",
      "core_path": "DETECT",
      "core_name": "DETECT",
      "db_name": "Nintendo - Nintendo Entertainment System.lpl"
    }
  ]
}
```

#### EmulationStation Gamelists (`gamelist.xml`)
```xml
<gameList>
  <game>
    <path>./Super Mario Bros. (USA).nes</path>
    <name>Super Mario Bros.</name>
    <desc>A classic platformer...</desc>
    <releasedate>19850913T000000</releasedate>
    <developer>Nintendo</developer>
    <publisher>Nintendo</publisher>
    <genre>Platform</genre>
    <players>1-2</players>
  </game>
</gameList>
```

#### CSV Report
Full metadata export with columns:
`Title, System, Region, Year, Publisher, Developer, Genre, Filename, Path, CRC32, MD5, SHA1, Confidence, MatchType`

#### JSON Export
Complete backup format with:
- Game metadata
- Associated files with hashes
- Optional: raw metadata from providers

**API (Q_INVOKABLE):**
```cpp
int exportToRetroArch(const QString &outputDir, const QStringList &systems, bool includeUnmatched);
int exportToEmulationStation(const QString &romsDir, bool downloadArtwork);
bool exportToCSV(const QString &outputPath, const QStringList &systems);
bool exportToJSON(const QString &outputPath, bool includeMetadata);
QVariantList getAvailableSystems();
QVariantMap getExportPreview(const QStringList &systems);
void cancelExport();
QString getRetroArchThumbnailPath(const QString &playlistName, const QString &gameTitle, const QString &type);
```

### 4. UI Updates

**Files Modified:**
- `src/ui/qml/MainWindow.qml` - Added Artwork and Export navigation
- `src/ui/resources.qrc` - Added new QML files
- `src/ui/CMakeLists.txt` - Added new controller sources
- `src/ui/main.cpp` - Register new controllers with QML

**New Navigation Structure:**
1. Library
2. Match Review
3. Conversions
4. **Artwork** (new)
5. **Export** (new)
6. Settings

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      MainWindow.qml                         │
│  ┌─────────────────────────────────────────────────────────┤
│  │  Sidebar              │        StackView               │
│  │  ───────              │     ────────────               │
│  │  • Library            │     LibraryView                │
│  │  • Match Review       │     MatchReviewView            │
│  │  • Conversions        │     ConversionView             │
│  │  • Artwork        ←───┼────→ ArtworkView               │
│  │  • Export         ←───┼────→ ExportView                │
│  │  • Settings           │     SettingsView               │
│  └───────────────────────┴─────────────────────────────────│
└─────────────────────────────────────────────────────────────┘

Controllers (QML Context Properties):
─────────────────────────────────────
• artworkController    → ArtworkController
• metadataEditor       → MetadataEditorController  
• exportController     → ExportController
```

## Database Changes

Added public accessor to `Database` class:
```cpp
QSqlDatabase& database() { return m_db; }
```

This allows M8 controllers to perform complex queries not covered by existing wrapper methods.

## Testing

```bash
cd build && make -j$(nproc) && ctest --output-on-failure
# Result: 100% tests passed (1/1)
```

## Lines of Code Added

| Component | Lines |
|-----------|-------|
| artwork_controller.h | 116 |
| artwork_controller.cpp | 396 |
| metadata_editor_controller.h | 113 |
| metadata_editor_controller.cpp | 476 |
| export_controller.h | 118 |
| export_controller.cpp | 648 |
| ArtworkView.qml | 305 |
| GameDetailsView.qml | 378 |
| ExportView.qml | 365 |
| **Total New Code** | **~2,915 lines** |

## Compatibility

- **RetroArch**: Playlist format v1.5 with CRC32 matching
- **ES-DE / EmulationStation**: gamelist.xml with standard fields
- **EmuDeck**: Compatible via ES-DE export
- **RetroDeck**: Compatible via ES-DE export

## What's Next: M9

M9 will implement ROM verification and patching:
- DAT file parser (No-Intro/Redump XML format)
- Header stripping (NES, SNES, Lynx)
- Patch formats (IPS, BPS, UPS, XDelta3, PPF)
- romhacking.net integration for patch discovery
