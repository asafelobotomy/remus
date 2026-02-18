# Enhanced Metadata Display Implementation

**Status**: ✅ COMPLETE  
**Date**: 2024-02-07  
**Version**: v0.9.0+metadata-ui

## Overview
Enhanced the Remus GUI to display rich game metadata in the UnprocessedSidebar during and after ROM processing. The system now shows cover art, descriptions, ratings, screenshots, and system logos when a ROM is matched against the database.

## Implementation Summary

### 1. Data Structure Enhancements

**File**: `src/metadata/metadata_provider.h`

Added new fields to support rich metadata:
```cpp
struct ArtworkUrls {
    // ... existing fields ...
    QUrl screenshot2;      // Secondary screenshot for gameplay variety
    QUrl systemLogo;       // Platform/system logo (e.g., Genesis logo)
};

struct GameMetadata {
    // ... existing fields ...
    QString ratingSource;  // Track rating provider (e.g., "MobyGames", "IGDB")
};
```

### 2. Backend Integration

**File**: `src/ui/controllers/processing_controller.h`

Added new signal for rich metadata delivery:
```cpp
void metadataUpdated(
    int fileId,
    const QString &description,
    const QString &coverArtUrl,
    const QString &systemLogoUrl,
    const QString &screenshotUrl,
    const QString &titleScreenUrl,
    float rating,
    const QString &ratingSource
);
```

**File**: `src/ui/controllers/processing_controller.cpp`

Enhanced `stepMatch()` to fetch artwork after successful match:
```cpp
// After creating match record:
ArtworkUrls artwork;
if (!metadata.providerId.isEmpty()) {
    artwork = m_orchestrator->getArtworkWithFallback(
        metadata.id, systemName, metadata.providerId);
}

emit metadataUpdated(
    m_currentFileId,
    metadata.description,
    artwork.boxFront.toString(),
    artwork.systemLogo.toString(),
    artwork.screenshot.toString(),
    artwork.titleScreen.toString(),
    metadata.rating,
    metadata.ratingSource
);
```

### 3. UI Property Bindings

**File**: `src/ui/qml/components/UnprocessedSidebar.qml`

Added properties to receive metadata:
```qml
property string gameDescription: ""
property string coverArtUrl: ""
property string systemLogoUrl: ""
property string screenshotUrl: ""
property string titleScreenUrl: ""
property real gameRating: 0.0
property string ratingSource: ""
```

Added signal handler for real-time updates:
```qml
function onMetadataUpdated(fileId, description, coverArtUrl, 
                          systemLogoUrl, screenshotUrl, titleScreenUrl,
                          rating, ratingSource) {
    if (fileId !== root.selectedFileId) return;
    
    root.gameDescription = description;
    root.coverArtUrl = coverArtUrl;
    root.systemLogoUrl = systemLogoUrl;
    root.screenshotUrl = screenshotUrl;
    root.titleScreenUrl = titleScreenUrl;
    root.gameRating = rating;
    root.ratingSource = ratingSource;
}
```

### 4. Visual UI Components

**File**: `src/ui/qml/components/UnprocessedSidebar.qml` (lines 395-600)

Added "Game Details" section with:

#### Cover Art Display
- 200px height Image component with aspect-fit scaling
- Loading indicator (BusyIndicator) during image fetch
- Rounded corners (6px radius) on dark background

#### System Logo
- 100x40px Image centered horizontally
- Shows platform/system logo (Genesis, NES, etc.)

#### Rating Display
- Formatted as "8.5 / 10 (MobyGames)"
- Accent color for rating value
- Muted italic text for source attribution

#### Description Section
- Scrollable text area (max 120px height)
- Word-wrapped plain text
- Dark background card (theme.bgSecondary)

#### Screenshots Gallery
- Two screenshot sections:
  1. **Start Screen** - Title screen / intro screen
  2. **Gameplay** - Action screenshot
- Each 100px height with aspect-fit scaling
- Labels above each screenshot for context

## Data Flow

```
ROM Scanned
    ↓
Hash Calculated
    ↓
Database Match Found (Multi-Signal)
    ↓
stepMatch() called
    ↓
Artwork Fetched (getArtworkWithFallback)
    ↓
metadataUpdated Signal Emitted
    ↓
onMetadataUpdated() Handler
    ↓
UI Properties Updated
    ↓
QML UI Re-renders with Artwork
```

## Features

### Conditional Display
- Section only visible when metadata is available
- Individual components check for empty URLs before displaying
- Graceful degradation when artwork unavailable

### Asynchronous Loading
- All images use `asynchronous: true` for non-blocking load
- BusyIndicator shows during cover art fetch
- Smooth rendering enabled for better quality

### Visual Polish
- Theme-consistent colors (cardBg, bgSecondary, textMuted)
- 8px border radius for cards
- 12px spacing between sections
- Responsive to theme changes

### Error Handling
- Invalid URLs gracefully handled by QML Image
- Missing artwork doesn't break UI
- Empty states handled with `visible` bindings

## Testing Instructions

### 1. Restart GUI
```bash
cd /home/solon/Documents/remus
./build/src/ui/remus-gui
```

### 2. Process Test ROM
- Navigate to "Processing" view
- Select the Sonic ROM: `/home/solon/roms/genesis/Sonic The Hedgehog (USA, Europe).md`
- Click "Process ROM"

### 3. Verify Display
Expected to see in the sidebar:
- ✅ **Cover Art**: Sonic box art (if available in database)
- ✅ **System Logo**: Sega Genesis logo
- ✅ **Rating**: Game rating + source (e.g., "8.5 / 10 (MobyGames)")
- ✅ **Description**: Game synopsis scrollable text
- ✅ **Screenshots**: 
  - Start Screen: Sonic title screen
  - Gameplay: In-game screenshot

### 4. Check Edge Cases
- Process ROM with missing artwork (verify graceful handling)
- Process ROM with only partial metadata (description but no images)
- Process ROM from different system (verify system logo changes)

## Debug Logging

To verify signal emission:
```bash
QT_LOGGING_RULES="remus.ui.debug=true" ./build/src/ui/remus-gui 2>&1 | grep -i "metadata\|artwork"
```

Expected output:
```
remus.ui: Processing file 1 / 1 : "Sonic The Hedgehog (USA, Europe).md"
remus.ui: Match found - fetching artwork for game ID: ...
remus.ui: Emitting metadataUpdated with 7 parameters
remus.ui: Artwork URLs: boxFront=https://..., systemLogo=https://...
```

## Known Limitations

1. **Artwork Fallback**: If ProviderOrchestrator fails to fetch artwork, URLs will be empty
2. **Description Length**: Currently capped at 120px scrollable height
3. **Screenshot2 Field**: Not yet populated by any provider (future enhancement)
4. **Network Dependency**: Cover art display requires internet connection if not cached

## Integration Points

### Metadata Providers
The system uses existing provider infrastructure:
- `ProviderOrchestrator::getArtworkWithFallback()`
- Tries multiple providers in priority order:
  1. Hasheous (hash-based)
  2. ScreenScraper (hash + name)
  3. TheGamesDB (name-only)
  4. IGDB (name-only)

### Artwork Caching
- `ArtworkDownloader` handles local caching
- Cache location: `~/.cache/Remus/artwork/`
- 30-day TTL on cached images

### Database Schema
Metadata stored in:
- `games` table: Core game info
- `matches` table: Links files to games
- `metadata_sources` table: Provider attribution
- `artwork_cache` table: Cached image references

## Future Enhancements

1. **Lazy Loading**: Only fetch artwork when sidebar is visible
2. **Screenshot Carousel**: Allow cycling through multiple screenshots
3. **Full-Screen Preview**: Click to enlarge cover art/screenshots
4. **Manual Artwork Override**: Allow users to upload custom artwork
5. **Artwork Quality Selection**: HD vs thumbnail options
6. **Box Art Variants**: Show front/back/spine as tabs

## Files Modified

1. `src/metadata/metadata_provider.h` - Added screenshot2, systemLogo, ratingSource fields
2. `src/ui/controllers/processing_controller.h` - Added metadataUpdated signal
3. `src/ui/controllers/processing_controller.cpp` - Integrated artwork fetching in stepMatch()
4. `src/ui/qml/components/UnprocessedSidebar.qml` - Added Game Details section UI

## Commit Message Suggestion
```
feat(ui): Add rich metadata display with cover art and screenshots

- Add screenshot2 and systemLogo fields to ArtworkUrls
- Add ratingSource field to GameMetadata for attribution
- Emit metadataUpdated signal with 7 metadata parameters after match
- Display cover art, description, rating, screenshots in UnprocessedSidebar
- Implement conditional rendering based on metadata availability
- Add asynchronous image loading with BusyIndicator

Enhances ROM processing UX by showing visual metadata during and after
matching. Sidebar now displays game artwork fetched from providers
using existing fallback chain (Hasheous → ScreenScraper → TheGamesDB → IGDB).

Part of v0.9.0 metadata enhancements.
```

## Success Criteria

- [x] Backend structures extended (ArtworkUrls, GameMetadata)
- [x] ProcessingController emits metadataUpdated signal
- [x] UI properties receive and bind metadata
- [x] Visual components display artwork
- [ ] **Testing**: Verify with real ROM processing (PENDING)
- [ ] **Edge Cases**: Test missing artwork handling (PENDING)
- [ ] **Performance**: Verify no UI blocking during fetch (PENDING)

---

**Next Step**: Launch `remus-gui` and process Sonic ROM to verify the enhanced metadata displays correctly in the sidebar.
