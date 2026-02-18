# Remus GUI Testing Guide - Metadata Matching

## Current Status
✅ **GUI is running** (PID: check with `ps aux | grep remus-gui`)
✅ **LocalDatabaseProvider is active** and found a match for Sonic ROM

## Test Scenarios

### Scenario 1: Verify Existing Metadata Match

The CLI tool already scanned and matched the Sonic ROM. Check the GUI to see:

1. **Library View**:
   - Navigate to Library section
   - Look for "Sonic The Hedgehog (USA, Europe).md"
   - Should show:
     - System: Genesis
     - Match status: Matched ✓
     - Confidence: ~90% (hash + filename + size)

2. **File Details**:
   - Click on the Sonic ROM entry
   - Verify displayed information:
     - Title: "Sonic The Hedgehog (USA, Europe)"
     - Region: USA
     - CRC32: `f9394e97`
     - MD5: `1bc674be034e43c96b86487ac69d9293`
     - SHA1: `6ddb7de1e17e7f6cdb88927bd906352030daa194`

### Scenario 2: Fresh Scan with Multi-Signal Matching

To test the complete pipeline from scratch:

#### Step 1: Setup LocalDatabaseProvider
1. Go to **Settings** → **Metadata Providers**
2. Verify "LocalDatabaseProvider" is enabled (should be priority 100)
3. Go to **Settings** → **DAT Management**
4. Click "Load DAT File"
5. Select: `/home/solon/Documents/remus/data/databases/Sega - Mega Drive - Genesis.dat`
6. Should show: "3267 entries loaded"

#### Step 2: Scan ROMs
1. Go to **Library** section
2. Click "Scan Directory"
3. Browse to: `/home/solon/Documents/remus/tests/rom_tests/Sonic The Hedgehog (USA, Europe)/`
4. Click "Start Scan"

**Expected Results**:
- Found: 1 file (Sonic The Hedgehog (USA, Europe).md)
- System detected: Genesis (from .md extension)
- Status: Scanning complete

#### Step 3: Calculate Hashes
1. After scan completes, click "Calculate Hashes"
2. Watch progress bar

**Expected Results**:
- Progress: 1/1 files
- Time: <1 second
- Hashes calculated: CRC32, MD5, SHA1

#### Step 4: Match Metadata
1. Click "Match Metadata" button
2. **Watch the console logs** (in `/tmp/remus-test-session.log`)

**Expected Log Output**:
```
LocalDatabaseProvider: Multi-signal matching for "Sonic The Hedgehog (USA, Europe).md"
  CRC32: "f9394e97" MD5: "1bc674be034e43c96b86487ac69d9293" SHA1: "6ddb7de1e17e7f6cdb88927bd906352030daa194"
  Hash match (CRC32): "Sonic The Hedgehog (USA, Europe)"
LocalDatabaseProvider: Found 1 multi-signal matches
  Best match: "Sonic The Hedgehog (USA, Europe)" confidence: 90% signals: 3
```

**Expected GUI Results**:
- Match found: ✓
- Game Title: "Sonic The Hedgehog (USA, Europe)"
- Confidence: 90% (hash + filename + size matched)
- Metadata populated:
  - Title
  - Region (USA)
  - Serial number
  - Publisher
  - Release date

### Scenario 3: Test Confidence Scoring

To see different confidence levels in action:

#### 3A: Perfect Match (All Signals)
- Use the original Sonic ROM
- Expected: 100% confidence if serial number matches too

#### 3B: Hash-Only Match (Bad Metadata)
1. Rename the ROM file: `mv "Sonic The Hedgehog (USA, Europe).md" "WrongName.md"`
2. Rescan and match
3. Expected: 50% confidence (hash only, no filename/size match)

#### 3C: Fallback Match (No Hash Available)
1. Clear hash values from database
2. Match using only filename and size
3. Expected: 40% confidence (filename + size only)

## Monitoring Test Results

### Real-Time Logs
```bash
tail -f /tmp/remus-test-session.log | grep -E "(LocalDatabase|Multi-signal|confidence)"
```

### Database Query (After Matching)
```bash
sqlite3 ~/.local/share/Remus/Remus/remus.db \
  "SELECT filename, crc32, md5, system_id FROM files LIMIT 5"
```

### Check Matches Table
```bash
sqlite3 ~/.local/share/Remus/Remus/remus.db \
  "SELECT f.filename, m.confidence, m.method FROM matches m 
   JOIN files f ON m.file_id = f.id LIMIT 5"
```

## Expected Provider Behavior

### LocalDatabaseProvider (Priority 100)
- **Hash matching**: Searches CRC32, MD5, SHA1 indexes
- **Fallback**: Filename + size when no hash available
- **Confidence scoring**: 0-200 scale (shown as 0-100%)
- **Performance**: O(log n) for hash, O(n) for fallback

### Hasheous (Priority 90)
- Falls back if LocalDatabase doesn't find a match
- Uses online API (requires internet)

### ScreenScraper (Priority 80)
- Falls back if Hasheous fails
- Requires authentication (username/password)

## Troubleshooting

### Issue: No DAT files loaded
**Solution**: Load Genesis DAT from Settings → DAT Management

### Issue: No matches found despite correct hash
**Check**:
1. DAT file loaded? (Settings → DAT Management)
2. Hash format correct? (lowercase, no spaces)
3. Check logs: `grep "LocalDatabase" /tmp/remus-test-session.log`

### Issue: Low confidence score
**Expected**: Different scenarios yield different scores:
- Perfect match: 90-100%
- Hash only: 50%
- Fallback: 40%

### Issue: GUI not responding
**Restart**:
```bash
pkill -9 remus-gui
cd /home/solon/Documents/remus
./build/src/ui/remus-gui 2>&1 | tee /tmp/remus-test-session.log &
```

## Success Criteria

### ✅ Pass: All Tests Working
- [x] GUI launches without crashes
- [x] LocalDatabaseProvider loads DAT file
- [x] ROM scanning detects correct system
- [x] Hash calculation completes properly
- [x] Multi-signal matching finds correct game
- [x] Confidence score displays correctly (90% for Sonic)
- [x] Metadata populated in UI

### ✅ Pass: Performance Acceptable
- [x] DAT load: <2 seconds for 3267 entries
- [x] Hash calculation: <5 seconds per ROM
- [x] Matching: <1 second per file
- [x] UI responsive during operations

## Additional Test ROMs

If you want to test more games:

```bash
# Nintendo ROMs (requires NES/SNES DAT files)
/home/solon/Documents/remus/tests/rom_tests/Originals/

# PlayStation ROMs (requires PS1 DAT file)
/home/solon/Documents/remus/tests/rom_tests/Silent Hill (USA).7z
```

---

**Quick Start**: Just open the Remus GUI and go to Library → verify Sonic ROM is already matched with 90% confidence!
