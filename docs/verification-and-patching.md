# ROM Verification and Patching

## Overview

Remus includes two powerful features for ROM integrity and enhancement:

1. **Verification**: Validate ROMs against DAT catalogs, with optional bundled compendium data available for supplemental matching
2. **Patching**: Apply translation patches, ROM hacks, and improvements from sources like romhacking.net

---

## 1. ROM Verification

### What is ROM Verification?

ROM verification validates your files against reference databases (DAT files) maintained by preservation communities:

#### Primary Verification Databases
- **No-Intro**: Cartridge-based systems (NES, SNES, Genesis, GBA, etc.)
  - URL: https://datomatic.no-intro.org/
  - Formats: XML DAT files
  - Hashes: CRC32, MD5, SHA1
  - Coverage: 70+ cartridge systems
  
- **Redump**: Disc-based systems (PlayStation, Saturn, Dreamcast, etc.)
  - URL: http://redump.org/
  - Formats: XML DAT files
  - Hashes: MD5, SHA1 (per-track for multi-track discs)
  - Coverage: CD-ROM, DVD, Blu-ray systems

#### Additional Hash Databases
- **TOSEC** (The Old School Emulation Center): Both disc and cartridge systems
  - URL: https://www.tosecdev.org/
  - Broader scope including demos, educational software
  
- **Renascene**: PSP games and UMD images
  - Specialized for PSP verification
  
- **GameTDB**: GameCube, Wii, Wii U
  - URL: https://www.gametdb.com/
  - Hash database for Nintendo disc systems
  
- **gc-forever**: GameCube and Wii preservation
  - Community-driven GameCube/Wii hash database
  
- **PS3 IRD Database**: PlayStation 3 game verification
  - IRD (ISO Rebuild Data) files for PS3 disc verification

These databases contain cryptographic checksums (CRC32, MD5, SHA1) of verified clean dumps.

**Reference**: See [Emulation General Wiki - File Hashes](https://emulation.gametechwiki.com/index.php/File_hashes) for comprehensive hash database list and verification tools.

### Why Verify ROMs?

- **Authenticity**: Confirm you have an unmodified official release
- **Quality**: Detect corrupted or bad dumps
- **Completeness**: Identify which verified releases you own vs. missing
- **Organization**: Distinguish hacks/translations from official releases
- **Safety**: Avoid malware or tampered files

### DAT File Format

No-Intro and Redump use XML DAT files following this structure:

```xml
<?xml version="1.0"?>
<!DOCTYPE datafile PUBLIC "-//Logiqx//DTD ROM Management Datafile//EN" "http://www.logiqx.com/Dats/datafile.dtd">
<datafile>
  <header>
    <name>Nintendo - Nintendo Entertainment System (Headerless)</name>
    <description>No-Intro | 2024-01-15</description>
    <version>20240115</version>
  </header>
  <game name="Super Mario Bros. (USA)">
    <description>Super Mario Bros. (USA)</description>
    <rom name="Super Mario Bros. (USA).nes" size="40960" crc="3337ec46" md5="811b027eaf99c2def7b933c5208636de" sha1="ea343f4e445a9050d4b4fbac2c77d0693b1d0922"/>
  </game>
  <game name="The Legend of Zelda (USA)">
    <description>The Legend of Zelda (USA)</description>
    <rom name="The Legend of Zelda (USA).nes" size="131088" crc="337bd2bb" md5="32b76cc4e7f3f13e4f8d2ae1d27b3ffa" sha1="cc27f7b793e16b7c6c3ea89e3cf87e7bbfe62f5d"/>
  </game>
</datafile>
```

### Verification Process in Remus

#### Step 1: Supply A Verification Catalog

```text
1. Provide a DAT or XML catalog file from No-Intro, Redump, or another reference source
2. Run the CLI verification flow with `--verify <dat-file>`
3. If a bundled compendium database is present, Remus also attaches it for supplemental verification and patch matching
```

#### Step 2: Run Verification

```text
1. Library → Verify Collection
2. Select systems to verify
3. Remus compares file hashes against imported verification catalog entries
4. Results categorized as:
  - ✅ Verified: Exact match with reference catalog data
  - ❌ Failed: Hash mismatch (corrupt/modified)
  - ❓ Unknown: Not in the supplied catalog data
```

#### Step 3: Review Results

```text
- View missing games (in the supplied catalog but not in library)
- View failed verification (potential corruption)
- View unknown ROMs (candidates for patching detection)
- Export verification report (CSV/JSON)
```

### Header Handling

Some systems (NES, Lynx) have copier headers that must be excluded from hash calculation:

| System | Header Size | Notes |
|--------|-------------|-------|
| NES (iNES) | 16 bytes | Must be stripped before hashing |
| Lynx | 64 bytes | Must be stripped |
| SNES (SMC) | 512 bytes | Rare, old copier format |

Remus automatically detects and strips headers when verifying these systems.

### Multi-Track Disc Verification

For multi-track CD-ROM games (common on PlayStation and Saturn):
- Each track must be hashed separately
- Compare per-track hashes against Redump DAT entries
- Use tools like IsoBuster (Windows) or cdirip (Linux) to extract tracks
- `.cue` files define track layout

**Example**: PlayStation game with audio tracks
```
Track 01 (Data): SHA1 hash of .bin file
Track 02 (Audio): SHA1 hash of audio .bin
Track 03 (Audio): SHA1 hash of audio .bin
```

Remus will verify each track individually when using Redump DAT files.

### Implementation Details

**Database Schema**: See `verification_dats` and `verification_results` tables in [data-model.md](data-model.md)

**Hash Priority**:
1. SHA1 (strongest, preferred for disc systems)
2. MD5 (fallback for disc systems)
3. CRC32 (sufficient for cartridge systems, faster)

**Performance**: Verification is parallelized with progress tracking for large libraries.

---

## 2. ROM Patching

### What is ROM Patching?

ROM patching applies binary difference files to transform a base ROM:
- **Translations**: Japanese games → English
- **ROM Hacks**: Gameplay mods, level edits, graphic changes
- **Fixes**: Bug patches, enhancement patches

### Patch Formats

| Format | Max Size | Checksums | Use Case | Recommended Tool |
|--------|----------|-----------|----------|------------------|
| **IPS** | 16 MB | No | Legacy, NES/SNES | Flips |
| **BPS** | Unlimited | Yes (source, target, patch) | Modern standard | Flips, beat |
| **UPS** | Unlimited | Yes | Alternative to BPS | beat |
| **PPF** | Unlimited | Yes | PlayStation images | PPF-O-Matic |
| **XDelta3** | Unlimited | Yes | Large files (disc) | xdelta3 |
| **APS** | Varies | Yes | GBA (APS-GBA), N64 (APS-N64) | UniPatcher |

**Recommendation**: Prefer BPS for new patches (built-in verification prevents wrong ROM errors).

### Patching Tools Integration

Remus will bundle/integrate these open-source patching tools:

#### Flips (Floating IPS)
- **License**: GPL
- **Platforms**: Linux, Windows, macOS
- **Formats**: IPS, BPS
- **CLI**: `flips --apply patch.bps base.rom output.rom`
- **Source**: https://github.com/Alcaro/Flips

#### xdelta3
- **License**: GPL
- **Platforms**: Linux, Windows, macOS
- **Formats**: XDelta3
- **CLI**: `xdelta3 -d -s base.iso patch.xdelta output.iso`
- **Use**: Large disc-based games

#### Additional Tools (Future Consider)
- **beat**: BPS reference implementation (no longer maintained)
- **PPF-O-Matic**: PlayStation PPF patches
- **UniPatcher libs**: Android multi-format support (could port)

### romhacking.net Integration

#### Current State (2026)
- **No Public API**: romhacking.net does not offer an official API
- **Web Scraping Required**: Must parse HTML or use RSS feeds
- **Legal Considerations**: Respect robots.txt and rate limiting
- **Patch Distribution**: Patches are freely downloadable (not ROM files)

#### Integration Strategy

**Phase 1: Manual Patch Application (M8)**
```
1. User downloads patch from romhacking.net manually
2. Remus → Tools → Apply Patch
3. Select base ROM + patch file
4. Remus detects format, applies with appropriate tool
5. Store patched ROM with metadata linking to source
```

**Phase 2: Semi-Automatic (Future)**
```
1. User provides romhacking.net URL for patch
2. Remus scrapes patch details (title, system, format)
3. Downloads patch file directly
4. Identifies matching ROM in library by name/hash
5. Prompts user to review and apply
6. Stores patch info in database
```

**Phase 3: Fully Automatic (Future - Advanced)**
```
1. During library scan, detect potential hack candidates:
   - Unknown ROMs (not in No-Intro/Redump)
   - Filename patterns matching hack names
2. Search romhacking.net for matching ROM name
3. If patch found, suggest "unapplying" to restore original:
   - "Detected: Super Metroid [Redesign].smc"
   - "Found patch: Super Metroid Redesign by DMantra"
   - "Original ROM: Super Mario Bros. (USA) [verified]"
   - "Action: [Keep Hack] [Restore Original]"
```

#### Web Scraping Considerations

**robots.txt Compliance**: Check https://www.romhacking.net/robots.txt before implementing
**Rate Limiting**: Max 1 request per 2 seconds, implement exponential backoff
**Caching**: Store patch metadata locally (refreshed monthly)
**Error Handling**: Graceful fallback if site structure changes

#### Patch Metadata to Scrape

```json
{
  "patch_id": "1234",
  "title": "Final Fantasy VI - Brave New World",
  "author": "BTB and Synchysi",
  "system": "SNES",
  "type": "Improvement",
  "version": "2.1.0",
  "release_date": "2023-08-15",
  "description": "A complete overhaul of FF6...",
  "patch_format": "IPS",
  "base_rom_crc32": "e986575b",
  "download_url": "https://www.romhacking.net/hacks/1234/download",
  "download_count": 50234,
  "rating": 4.8
}
```

### Automatic Patching Workflow (Future)

#### Discovery Mode
```
1. User: "Library → Discover Patches"
2. Remus analyzes library:
   - Identifies verified ROMs (from No-Intro/Redump)
   - Queries romhacking.net cache for patches
   - Displays available patches per game
3. User selects patches to apply
4. Remus downloads patches
5. For each patch:
   - Verify base ROM checksum (BPS format includes this)
   - Apply patch
   - Name output: "Game Title [Hack Name].ext"
   - Store in library with metadata
```

#### Example UI Flow
```
╔═══════════════════════════════════════════════════════════╗
║ Patch Suggestions for: Super Mario Bros. (USA)           ║
╠═══════════════════════════════════════════════════════════╣
║ ✅ Super Mario Bros. Deluxe (Improvement)                ║
║    Author: YourBudTevin | v1.0 | BPS | ⭐ 4.7           ║
║    Desc: Improved graphics and controls                  ║
║    [Apply Patch] [View Details] [Ignore]                 ║
╠═══════════════════════════════════════════════════════════╣
║ ✅ All Night Nippon Super Mario Bros. (Translation)     ║
║    Author: Stardust Crusaders | v1.1 | IPS | ⭐ 4.9     ║
║    Desc: English translation of Japanese radio tie-in    ║
║    [Apply Patch] [View Details] [Ignore]                 ║
╚═══════════════════════════════════════════════════════════╝
```

### Patch Organization

Remus organizes patched ROMs alongside originals:

```
/NES/
  Super Mario Bros. (USA).nes                    [verified original]
  Super Mario Bros. (USA) [Deluxe v1.0].nes     [patched]
  Super Mario Bros. (USA) [All Night v1.1].nes  [patched]
```

**Metadata Tracking**:
- Link patched ROM to source ROM
- Store patch file hash
- Store patch source URL
- Track patch version
- Enable "unpatch" / "repatch" operations

### Safety Features

1. **Backup Original**: Always preserve unpatched ROM
2. **Verification**: BPS/UPS formats verify correct base ROM before patching
3. **Dry-Run Preview**: Show what will be created before applying
4. **Undo**: Store original hash to detect if restoration needed
5. **Collision Handling**: Prompt if output file already exists

### Soft Patching (Future Enhancement)

Some emulators support "soft patching" - applying patches at runtime without creating new files:

| Emulator | Soft Patch Support | Format |
|----------|-------------------|--------|
| RetroArch | Yes | IPS, BPS, UPS |
| BSNES | Yes | BPS |
| Mesen | Yes | IPS |
| FCEUX | Yes | IPS |

**Benefit**: Save disk space, keep library clean
**Implementation**: Store patch files alongside ROMs, emulator applies on load

---

## 3. Integration with Metadata Layer

### Distinguishing Hacks from Official Releases

When scanning library:
1. Calculate hash of ROM
2. Check against No-Intro/Redump DAT (verification)
3. If **not found** in DAT:
   - Check filename for hack indicators: `[Hack]`, `[T-Eng]`, `[Redux]`
   - Check romhacking.net cache for matching patched ROM hash
   - Tag as: `type: "hack"` or `type: "translation"`
4. If **found** in DAT:
   - Tag as: `type: "verified"` or `type: "official"`

### Metadata Provider Priority

For verified ROMs:
1. ScreenScraper (uses No-Intro/Redump naming)
2. TheGamesDB
3. IGDB

For hacks/translations:
1. romhacking.net metadata (if available)
2. Fallback to base game metadata (e.g., "Super Mario Bros." metadata for "Super Mario Bros. [Kaizo]")
3. Manual entry

---

## 4. Database Schema Extensions

### New Tables

#### `verification_dats`
Stores imported DAT files

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | DAT file record ID |
| system_id | INTEGER FK | System this DAT applies to |
| name | TEXT | DAT name (e.g., "No-Intro NES") |
| description | TEXT | DAT description |
| version | TEXT | DAT version/date |
| source | TEXT | "no-intro" or "redump" |
| file_path | TEXT | Path to original .dat/.xml file |
| imported_at | DATETIME | Import timestamp |
| entry_count | INTEGER | Number of ROM entries |

#### `verification_results`
Stores per-file verification outcomes

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Result ID |
| file_id | INTEGER FK | File being verified |
| dat_id | INTEGER FK | DAT used for verification |
| status | TEXT | "verified", "failed", "unknown" |
| expected_crc32 | TEXT | CRC32 from DAT (if exists) |
| expected_md5 | TEXT | MD5 from DAT (if exists) |
| expected_sha1 | TEXT | SHA1 from DAT (if exists) |
| actual_crc32 | TEXT | Calculated CRC32 |
| actual_md5 | TEXT | Calculated MD5 |
| actual_sha1 | TEXT | Calculated SHA1 |
| expected_size | INTEGER | File size from DAT |
| actual_size | INTEGER | Actual file size |
| verified_at | DATETIME | Verification timestamp |
| notes | TEXT | Error details if failed |

#### `patches`
Stores patch file information

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Patch record ID |
| romhacking_id | INTEGER | ID from romhacking.net |
| title | TEXT | Patch name |
| author | TEXT | Patch creator |
| system_id | INTEGER FK | Target system |
| type | TEXT | "translation", "hack", "improvement", "fix" |
| description | TEXT | Patch description |
| version | TEXT | Patch version |
| release_date | DATE | Release date |
| patch_format | TEXT | "IPS", "BPS", "UPS", "xdelta3", etc. |
| file_path | TEXT | Local path to patch file |
| file_hash | TEXT | SHA256 of patch file |
| source_url | TEXT | Download URL |
| base_rom_crc32 | TEXT | Required CRC32 for base ROM |
| base_rom_md5 | TEXT | Required MD5 (if available) |
| base_rom_sha1 | TEXT | Required SHA1 (if available) |
| rating | REAL | User rating (0.0-5.0) |
| download_count | INTEGER | Download count from site |
| created_at | DATETIME | Record creation timestamp |
| updated_at | DATETIME | Last metadata refresh |

#### `patch_applications`
Tracks applied patches (links patched ROM to source ROM + patch)

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PRIMARY KEY | Application record ID |
| patch_id | INTEGER FK | Patch that was applied |
| base_file_id | INTEGER FK | Original ROM file |
| patched_file_id | INTEGER FK | Resulting patched ROM file |
| applied_at | DATETIME | When patch was applied |
| applied_by_user | BOOLEAN | Manual vs. automatic |
| verified | BOOLEAN | Patch checksum verified before apply |
| notes | TEXT | User notes or errors |

### Modified Tables

#### `files` (add column)
```sql
ALTER TABLE files ADD COLUMN verification_status TEXT DEFAULT 'unknown';
-- Values: 'verified', 'failed', 'unknown', 'not_checked'

ALTER TABLE files ADD COLUMN file_type TEXT DEFAULT 'official';
-- Values: 'official', 'hack', 'translation', 'homebrew', 'prototype', 'unknown'

ALTER TABLE files ADD COLUMN is_patched BOOLEAN DEFAULT 0;
-- Indicates if this file was created via patching
```

---

## 5. CLI Examples

### Verification Commands
```bash
# Verify scanned files against a DAT or XML catalog
remus --verify ~/Downloads/No-Intro_NES_2024.dat --verify-report

# Supplemental compendium data is attached automatically when
# data/compendium/remus_compendium.db is available.
```

### Patching Commands
```bash
# Apply patch manually
remus patch --apply \
  --base "/path/to/Super Mario Bros. (USA).nes" \
  --patch ~/Downloads/smb_deluxe.bps \
  --output "/path/to/Super Mario Bros. (USA) [Deluxe].nes"

# Scan for patches on romhacking.net
remus patch --discover --game "Final Fantasy VI"

# Download and apply patch by ID
remus patch --apply-from-web \
  --romhacking-id 1234 \
  --base "/path/to/Final Fantasy VI (USA).sfc"

# List applied patches
remus patch --list-applied

# Unapply patch (restore original, delete patched)
remus patch --unapply --file "/path/to/Super Mario Bros. (USA) [Deluxe].nes"
```

---

## 6. UI Wireframes (M9 UI)

### Verification Tab
```
┌─────────────────────────────────────────────────────────────┐
│ 📊 Verification Status                                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [Import DAT Files]  [Verify All]  [Export Report]         │
│                                                             │
│  Systems Overview:                                          │
│  ┌─────────────┬─────────┬─────────┬─────────┬──────────┐  │
│  │ System      │ Total   │Verified │ Failed  │ Unknown  │  │
│  ├─────────────┼─────────┼─────────┼─────────┼──────────┤  │
│  │ NES         │   450   │  442 ✅ │   3 ❌  │   5 ❓   │  │
│  │ SNES        │   320   │  318 ✅ │   0 ❌  │   2 ❓   │  │
│  │ PlayStation │   180   │  175 ✅ │   2 ❌  │   3 ❓   │  │
│  └─────────────┴─────────┴─────────┴─────────┴──────────┘  │
│                                                             │
│  Recent Verifications:                                      │
│  🟢 Super Mario Bros. (USA) - Verified (CRC: 3337ec46)     │
│  🔴 Zelda II (USA) - Failed (corrupt or modified)          │
│  🟡 Custom Hack v2.3 - Unknown (not in database)           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Patching Tab
```
┌─────────────────────────────────────────────────────────────┐
│ 🔧 ROM Patching                                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [Apply Patch Manually]  [Discover Patches]  [History]     │
│                                                             │
│  Available Patches for Your Library: (12 found)            │
│  ┌───────────────────────────────────────────────────────┐ │
│  │ 🎮 Final Fantasy VI - Brave New World v2.1            │ │
│  │    Type: Improvement | Format: IPS | ⭐ 4.9 | 50k DL  │ │
│  │    Base: Final Fantasy III (USA).sfc [in library ✅]  │ │
│  │    [Apply] [Details] [Ignore]                         │ │
│  ├───────────────────────────────────────────────────────┤ │
│  │ 🇯🇵 Fire Emblem - English Translation v1.0            │ │
│  │    Type: Translation | Format: BPS | ⭐ 5.0 | 120k DL │ │
│  │    Base: Fire Emblem (Japan).gba [in library ✅]      │ │
│  │    [Apply] [Details] [Ignore]                         │ │
│  └───────────────────────────────────────────────────────┘ │
│                                                             │
│  Recently Applied: (3)                                      │
│  • Super Mario Bros. [Deluxe v1.0] - Applied 2026-02-03    │
│  • Metroid [Redux v1.5] - Applied 2026-01-28               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. Implementation Roadmap

### M8.1: Verification Foundation (Week 14)
- [ ] Database schema: add verification tables
- [ ] DAT file parser (XML format)
- [ ] Header detection and stripping (NES, Lynx)
- [ ] Verification engine (hash comparison)
- [ ] CLI commands: `--verify <dat-file>`, `--verify-report`
- [ ] Verification status UI in library view

### M8.2: Manual Patching (Week 15)
- [ ] Integrate Flips binary (Linux)
- [ ] Integrate xdelta3 binary
- [ ] Patch format detection
- [ ] Database schema: patches + patch_applications tables
- [ ] CLI command: `patch --apply`
- [ ] UI: Manual patch application dialog

### M9: romhacking.net Integration (Week 16-17)
- [ ] Web scraper for romhacking.net
- [ ] Patch metadata cache
- [ ] Semi-automatic patch discovery
- [ ] CLI command: `patch --discover`
- [ ] UI: Patch suggestions tab
- [ ] Download and apply workflow

### M10: Advanced Patching (Future)
- [ ] Automatic hack detection during scan
- [ ] Soft patching support (emulator integration)
- [ ] Batch patch application
- [ ] Patch creation tools (for power users)
- [ ] Community patch ratings/reviews

---

## 8. Technical Considerations

### Security
- **Checksums**: Always verify patch checksums before applying
- **Sandboxing**: Run patching tools in restricted environment
- **Malware Scanning**: Optional ClamAV integration for downloaded patches
- **HTTPS Only**: Enforce encrypted downloads from romhacking.net

### Performance
- **Parallel Verification**: Multi-threaded hash calculation (4-8 workers)
- **Incremental Verification**: Only verify new/modified files
- **DAT Caching**: Index DAT files in SQLite for fast lookups
- **Patch Metadata Cache**: Refresh once per week, not per query

### Error Handling
- **Corrupt Patches**: Detect truncated downloads, verify integrity
- **Wrong Base ROM**: BPS format includes checksums to prevent mismatch
- **Missing Tools**: Graceful fallback if Flips/xdelta3 not found
- **Network Failures**: Retry logic with exponential backoff

### Legal Compliance
- **No ROM Distribution**: Never host or redistribute ROM files
- **Patch Distribution OK**: Patches are legal (contain only differences)
- **Copyright Notices**: Display romhacking.net attribution
- **robots.txt**: Respect crawling restrictions

---

## 9. User Stories

### Story 1: Verify Collection Authenticity
> "As a collector, I want to verify my 500 NES ROMs against No-Intro to identify bad dumps."

**Flow**:
1. User imports No-Intro NES DAT file
2. Clicks "Verify All" for NES system
3. Remus calculates hashes (with header stripping)
4. Shows: 485 verified ✅, 3 failed ❌, 12 unknown ❓
5. User clicks failed ROMs to see expected vs. actual hashes
6. User re-downloads corrupted ROMs

### Story 2: Apply English Translation
> "As a player, I want to patch my Japanese Fire Emblem ROM with the English fan translation."

**Flow**:
1. User has: "Fire Emblem (Japan).gba" in library
2. Downloads "fire_emblem_eng_v1.0.bps" from romhacking.net
3. Tools → Apply Patch
4. Selects base ROM + patch file
5. Sets output name: "Fire Emblem (Japan) [T-Eng v1.0].gba"
6. Remus verifies BPS checksum matches base ROM
7. Applies patch, adds to library
8. User launches in emulator

### Story 3: Discover Available Hacks
> "As an enthusiast, I want Remus to suggest popular ROM hacks for games I own."

**Flow**:
1. User: Library → Discover Patches
2. Remus queries romhacking.net cache
3. Finds 8 patches for owned games
4. Shows suggestions sorted by rating
5. User selects "Super Mario World - Kaizo Mario"
6. Reviews details: "Extreme difficulty hack, v3.0, BPS, 4.8⭐"
7. Clicks "Apply" → Remus downloads, verifies, applies
8. New ROM added: "Super Mario World (USA) [Kaizo v3.0].sfc"

---

## 10. Future Enhancements

- **Incremental Patches**: Apply series of patches (v1.0 → v1.1 → v1.2)
- **Patch Reversal**: Unapply patches to restore original
- **Custom Patch Creation**: Built-in IPS/BPS creator for user mods
- **Patch Collections**: Bundle multiple patches (e.g., "SNES Translation Pack")
- **Community Integration**: Rate and review patches within Remus
- **Auto-Update Patches**: Notify when patch updates available
- **Softpatching Playlists**: Generate RetroArch playlists with soft-patch references

---

## References

- **No-Intro**: https://no-intro.org/ (DAT files for cartridge systems)
- **Redump**: http://redump.org/ (DAT files for disc systems)
- **romhacking.net**: https://www.romhacking.net/ (patch database)
- **Flips (Floating IPS)**: https://github.com/Alcaro/Flips (IPS/BPS patcher)
- **xdelta3**: https://github.com/jmacd/xdelta (delta compression)
- **BPS Specification**: https://zerosoft.zophar.net/ips.php
- **DAT Format**: http://www.logiqx.com/DatFAQs/

---

*This feature will launch in M8-M9 (Weeks 14-17). Verification provides trust and integrity; patching unlocks translations and enhancements. Together they make Remus indispensable for ROM enthusiasts.*
