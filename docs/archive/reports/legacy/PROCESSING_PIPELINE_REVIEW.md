# Processing Pipeline & Sidebar Detail Review

## Current Pipeline Steps

The `ProcessingController` implements a 7-step pipeline:

1. **Extract** - Decompress archive files to a folder
2. **Hash** - Calculate CRC32, MD5, SHA1 for the ROM file
3. **Match** - Find game metadata using hash or name
4. **Metadata** - Gather additional info (currently placeholder)
5. **Artwork** - Download cover art (currently placeholder)
6. **Convert** - CHD conversion (optional, disc-based only)
7. **Complete** - Finalize and mark as processed

### Current Pipeline Code Location
- **Pipeline Definition**: `src/ui/controllers/processing_controller.h` (lines 20-27)
- **Step Execution**: `src/ui/controllers/processing_controller.cpp` (lines 405-625)
- **Individual Steps**:
  - `stepExtract()` - Lines 405-465
  - `stepHash()` - Lines 468-487
  - `stepMatch()` - Lines 489-556
  - `stepMetadata()` - Lines 558-565
  - `stepArtwork()` - Lines 567-582
  - `stepConvert()` - Lines 584-625

## Current Sidebar UI Structure

### For Unprocessed Items
**Component**: `src/ui/qml/components/UnprocessedSidebar.qml`

**Currently Shows**:
- File icon/question mark
- Filename
- File info (path, size, extension)
- Hash values (if already calculated)
- "Possible matches" array (currently empty/unused)
- Hash calculation button
- Action buttons (Remove, etc.)

**Missing**:
- Match confidence/method display
- Matched game title, publisher, year
- Genre/region/description
- Artwork preview
- Real-time match results during processing

### For Processed Items
**Component**: `src/ui/qml/components/ProcessedSidebar.qml`

**Currently Shows** ✅:
- Cover art
- Matched title
- Publisher, year
- Confidence score with color-coding
- Match method (hash/name/manual)
- Genre, region, rating
- Description (scrollable)
- Screenshots

## Data Flow for Processing

### When "Begin" Button is Pressed:

1. **Extract Step**:
   - Archives are decompressed to folder next to archive
   - Database updated with extracted file path
   - `.remusmd` marker created in extracted folder
   - Original archive moved to `Originals/` folder

2. **Hash Step**:
   - CRC32, MD5, SHA1 calculated
   - Database updated with hashes
   - **SIDEBAR NOT UPDATED** - No real-time feedback

3. **Match Step**:
   - Hash-based lookup (100% confidence)
   - Falls back to name-based search (70% confidence)
   - Game record created in database
   - Match record links file to game
   - **SIDEBAR NOT UPDATED** - No real-time feedback

4. **Metadata/Artwork/Convert Steps**:
   - Placeholders - don't affect sidebar
   - Artwork is handled separately by ArtworkController

5. **Complete Step**:
   - File marked as processed
   - `.remusmd` marker created
   - **ENTIRE SIDEBAR SWAPS** - Switches from UnprocessedSidebar to ProcessedSidebar

## Issues & Observations

### 1. **No Real-Time Feedback During Processing**
- User clicks "Begin" → item moves immediately to Processed
- No visual feedback of which step is running
- No display of hashes as they're calculated
- No display of match results as they're found
- User doesn't see the matching process happen

### 2. **Missing Match Signals**
- `ProcessingController` does NOT emit a signal when match is found
- UI must wait until file is marked as processed to get match data
- No way to update sidebar during processing steps

### 3. **Sidebar Reconstruction Problem**
- Match data is added to database in `stepMatch()`
- But sidebar data binding doesn't refresh until file is fully processed
- Need real-time signals to update during processing

### 4. **The "Possible Matches" Field**
- UnprocessedSidebar has `possibleMatches` property (currently unused)
- Could be populated during processing to show candidates
- Currently just an empty placeholder

## Recommended Improvements

### Immediate (High Impact):

1. **Add Match Result Signal**
   - Emit `matchFound(int fileId, GameMetadata metadata, int confidence)`
   - Emit `hashCalculated(int fileId, QString crc32, QString md5, QString sha1)`
   - Allow sidebar to update in real-time

2. **Update Sidebar During Processing**
   - Show hash values as they're calculated
   - Show matched game title, publisher, year as they're found
   - Show confidence score and match method
   - Fade in/animate when match appears

3. **Add Processing State Visualization**
   - Show progress: "Extracting... Hashing... Matching..."
   - Show completion checkmarks as steps finish
   - Show errors immediately if step fails

### Medium (UX Enhancement):

4. **Pre-Match Calculation**
   - On file selection, offer "Calculate Hash" button
   - Show hashes in sidebar after calculation
   - Allow instant name-based search without full processing

5. **Match Confidence Display During Processing**
   - Color-coded badges: Green (100%), Yellow (70%), Red (<60%)
   - Show multiple possible matches if confidence is borderline

### Longer Term:

6. **Batch Match Viewing**
   - Show all matched files in a review queue
   - Edit/confirm/reject matches before full processing
   - Apply matches in bulk

## Implementation Plan

The processing pipeline is already well-structured. We just need to:

1. Add signals to ProcessingController for intermediate results
2. Connect those signals to sidebar updates
3. Modify UnprocessedSidebar to show real-time match data
4. Keep sidebar visible during processing (don't swap immediately to ProcessedSidebar)
5. Animate/fade in new data as it arrives

**Current State**: Fully functional but user sees no intermediate progress  
**Desired State**: User sees hashes → matches → metadata appear in real-time as processing happens
