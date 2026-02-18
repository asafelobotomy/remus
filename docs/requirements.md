# Requirements Document

## Target Systems

### Nintendo
- **NES** (Nintendo Entertainment System)
  - Extensions: `.nes`
  - Typical hash: CRC32, MD5
  
- **SNES** (Super Nintendo Entertainment System)
  - Extensions: `.sfc`, `.smc`
  - Typical hash: CRC32, MD5
  
- **Nintendo 64**
  - Extensions: `.n64`, `.z64`, `.v64`
  - Typical hash: CRC32, MD5
  
- **GameCube**
  - Extensions: `.iso`, `.gcm`, `.rvz`
  - Typical hash: MD5, SHA1
  
- **Wii**
  - Extensions: `.iso`, `.wbfs`, `.rvz`
  - Typical hash: MD5, SHA1

### Nintendo Handheld
- **Game Boy**
  - Extensions: `.gb`
  - Typical hash: CRC32, MD5
  
- **Game Boy Color**
  - Extensions: `.gbc`
  - Typical hash: CRC32, MD5
  
- **Game Boy Advance**
  - Extensions: `.gba`
  - Typical hash: CRC32, MD5
  
- **Nintendo DS**
  - Extensions: `.nds`
  - Typical hash: CRC32, MD5
  
- **Nintendo 3DS**
  - Extensions: `.3ds`, `.cia`
  - Typical hash: MD5, SHA1

### Sega
- **Master System**
  - Extensions: `.sms`
  - Typical hash: CRC32, MD5
  
- **Genesis / Mega Drive**
  - Extensions: `.md`, `.gen`, `.bin`
  - Typical hash: CRC32, MD5
  
- **Sega CD / Mega CD**
  - Extensions: `.cue`, `.bin`, `.iso`, `.chd`
  - Typical hash: MD5, SHA1
  
- **Saturn**
  - Extensions: `.cue`, `.bin`, `.iso`, `.chd`
  - Typical hash: MD5, SHA1
  
- **Dreamcast**
  - Extensions: `.cdi`, `.gdi`, `.chd`
  - Typical hash: MD5, SHA1

### Sony
- **PlayStation**
  - Extensions: `.cue`, `.bin`, `.iso`, `.pbp`, `.chd`
  - Typical hash: MD5, SHA1
  
- **PlayStation 2**
  - Extensions: `.iso`, `.chd`
  - Typical hash: MD5, SHA1
  
- **PSP**
  - Extensions: `.iso`, `.cso`
  - Typical hash: MD5, SHA1

### Arcade
- **MAME / Arcade**
  - Extensions: `.zip`
  - Typical hash: CRC32 (for individual files within ZIP)

### Other
- **Atari 2600**
  - Extensions: `.a26`
  - Typical hash: CRC32, MD5
  
- **Neo Geo**
  - Extensions: `.zip`
  - Typical hash: CRC32

## Supported File Types

### ROM Files
- Single-file ROM images: `.nes`, `.sfc`, `.smc`, `.gb`, `.gbc`, `.gba`, `.nds`, `.md`, `.gen`, `.sms`, `.n64`, `.z64`, `.v64`

### Disc Images
- **ISO**: `.iso` (raw disc image)
- **CUE/BIN**: `.cue` (cue sheet) + `.bin` (binary data)
- **CHD**: `.chd` (Compressed Hunks of Data - recommended for space savings)
- **M3U**: `.m3u` (multi-disc playlist file)
- **Other**: `.pbp`, `.cso`, `.wbfs`, `.rvz`, `.gcm`, `.cdi`, `.gdi`

### Special Files
- **M3U Playlists**: For multi-disc games, contains list of disc files
  - Example: `Final Fantasy VII (USA).m3u` references all 3 disc CHD files
  - Enables seamless disc swapping in RetroArch and other emulators

### Archive Files
- **ZIP**: `.zip` (common for arcade ROMs)
- M3U playlists for multi-disc games (enables disc swapping)
- **7z**: `.7z` (compressed archives)
- **RAR**: `.rar` (compressed archives)

### Multi-file Games
- Games with multiple discs (e.g., FF7 Disc 1, Disc 2, Disc 3)
- Games with companion files (e.g., .cue + .bin)

## Metadata Providers

### Hash-Based Providers (Recommended)

1. **ScreenScraper** ⭐ PRIMARY
   - URL: https://www.screenscraper.fr/
   - Hash support: ✅ CRC32, MD5, SHA1
   - Strengths: Comprehensive retro game database, multiple artwork types, hash-based matching
   - Requirements: User account for API access (devid + devpassword + ssid + sspassword)
   - Rate limiting: 1 req/2s, 10k req/day (varies by account tier)
   - Data: Title, release date, publisher, developer, description, genres, players, region, boxart, screenshots, videos, wheels, logos
   - Best for: Verified No-Intro/Redump dumps (highest confidence)

2. **Hasheous** ⭐ EXCELLENT FALLBACK
   - URL: https://hasheous.org/
   - Hash support: ✅ MD5, SHA1
   - Strengths: FREE, no API key required, proxies IGDB metadata, community voting, very fast (533ms avg)
   - Requirements: None
   - Rate limiting: Standard rate limiting
   - Data: Title, description, cover art (proxied from IGDB), RetroAchievements IDs
   - Best for: Users who want friction-free hash matching without API credentials

3. **RetroAchievements**
   - URL: https://retroachievements.org/
   - Hash support: ✅ Custom per-system (see Game Identification docs)
   - Strengths: Achievement data + ROM verification, compatible with No-Intro hashes
   - Requirements: API key (RETROACHIEVEMENTS_API_KEY)
   - Rate limiting: Standard tier limits
   - Data: Title, achievement metadata, ROM verification status
   - Best for: Users who want achievements + hash verification

4. **PlayMatch**
   - Hash support: ✅ Hash-based matching
   - Strengths: Community-hosted, works with IGDB
   - Requirements: API endpoint configuration
   - Best for: Filling gaps in unmatched files

### Name-Based Providers (Fallback Only)

5. **TheGamesDB**
   - URL: https://thegamesdb.net/
   - Hash support: ❌ Name-based only
   - Strengths: Good coverage, free API
   - Requirements: Optional API key
   - Rate limiting: 1 req/s
   - Data: Title, release date, publisher, developer, description, genres, players, boxart, screenshots, banners
   - Best for: Name-based fallback when hash matching fails

6. **IGDB** (Internet Game Database)
   - URL: https://www.igdb.com/
   - Hash support: ❌ Name-based only
   - Strengths: Modern API, richest metadata, Twitch integration
   - Requirements: Twitch API credentials (client_id + client_secret)
   - Rate limiting: 4 req/s (250ms interval)
   - Data: Title, release date, publisher, developer, description, genres, cover art, screenshots, related games
   - Best for: Comprehensive metadata when hash matching unavailable

### Offline Databases

7. **OpenVGDB**
   - Hash support: ✅ ROM hash database
   - Type: Downloadable SQLite database
   - Strengths: Offline matching, no API calls
   - Best for: Local hash → game metadata mapping

### Hash Verification Databases (DAT Files)

These databases provide hash verification but not metadata:
- **No-Intro**: Cartridge-based systems
- **Redump**: Disc-based systems (CUE/BIN, ISO)
- **TOSEC**: Both disc and cartridge systems
- **Renascene**: PSP games
- **GameTDB**: GameCube, Wii, Wii U
- **gc-forever**: GameCube and Wii
- **PS3 IRD**: PlayStation 3 games

### Recommended Provider Strategy

**For M2 Implementation (Current)**:
1. ScreenScraper (hash-based, requires auth)
2. TheGamesDB (name-based fallback)
3. IGDB (name-based, rich metadata)

**Future Enhancement Options**:
1. Add Hasheous as primary hash fallback (no auth required!)
2. Add RetroAchievements for achievement data + hash verification
3. Add PlayMatch for community-driven matching
4. Consider OpenVGDB for offline mode

### Fallback Strategy
1. Try hash-based match on ScreenScraper (highest confidence)
2. If no match, try Hasheous (hash-based, no auth)
3. If no match, try name-based match on ScreenScraper
4. If still no match, fall back to TheGamesDB (name-based)
5. If still no match, try IGDB (name-based, richest metadata)
6. Allow manual search and override

## Naming Templates

Remus follows **No-Intro** (cartridge-based) and **Redump** (disc-based) naming conventions, the gold standard in ROM preservation.

### Template Variables
- `{title}`: Game title (with proper article placement)
- `{region}`: Full region name (USA, Europe, Japan, World, etc.)
- `{languages}`: Language codes if multi-language (En,Fr,De)
- `{version}`: Version/revision only if > initial release (Rev 1, v1.1)
- `{status}`: Development status (Beta, Proto, Sample)
- `{additional}`: Edition info (Limited Edition, Virtual Console)
- `{tags}`: Verification/mod tags [!], [h], [T+Eng]
- `{disc}`: Disc number for multi-disc games
- `{year}`: Release year (optional)
- `{publisher}`: Publisher name (optional)
- `{system}`: System name

### Standard No-Intro/Redump Format

**Cartridge ROMs (No-Intro):**
```
Title (Region) (Languages) (Version) (Status) (Additional) [Tags]
```
Examples:
- `Super Mario World (USA).sfc`
- `Legend of Zelda, The (USA).nes`
- `Final Fantasy VI (USA) (Rev 1).sfc`
- `Chrono Trigger (USA) (Sample).sfc`
- `Rockman (Japan) [T+Eng].nes`

**Disc Images (Redump):**
```
Title (Region) (Version) (Additional) (Disc X)
```
Examples:
- `Final Fantasy VII (USA) (Disc 1).chd`
- `Metal Gear Solid (USA) (Rev 1).chd`
- `Gran Turismo 2 (USA) (Greatest Hits).chd`

**Multi-disc Enhanced:**
```
Title (Region) (Disc X of Y)
```
Example: `Final Fantasy VII (USA) (Disc 1 of 3).chd`

### Article Handling
Articles like "The" move to end with comma:
- ✓ `Legend of Zelda, The (USA).nes`
- ✗ `The Legend of Zelda (USA).nes`

### Region Names (Full Form)
- `(USA)` - NOT `(U)`
- `(Europe)` - NOT `(E)`
- `(Japan)` - NOT `(J)`
- `(World)` - Worldwide release
- `(USA, Europe)` - Multi-region

### Version Handling
**Only show if greater than initial release:**
- ✓ `Final Fantasy VI (USA) (Rev 1).sfc`
- ✗ `Final Fantasy VI (USA) (v1.0).sfc` (initial version, omit)

### Tags (Square Brackets)
- `[!]` - Verified good dump
- `[b]` - Bad dump
- `[h]` - Hack/modification
- `[t]` - Trained (cracked with trainer)
- `[f]` - Fixed
- `[T+Eng]` - Fan translation to English
- `[o]` - Overdump
- `[BIOS]` - BIOS file

### User Customization
Users can create custom templates with variables, but **default templates follow No-Intro/Redump standards** for maximum compatibility with:
- RetroArch playlist scanner
- EmulationStation DE scraper
- Metadata providers (ScreenScraper, TheGamesDB)
- Community ROM sets

## Organization Rules

### Folder Structure Options

**Option 1: System-based**
```
/Library/
  /NES/
    Super Mario Bros. (USA).nes
    The Legend of Zelda (USA).nes
  /SNES/
    Super Mario World (USA).sfc
    Chrono Trigger (USA).sfc
  /PlayStation/
    Final Fantasy VII (Disc 1) (USA).cue
    Final Fantasy VII (Disc 1) (USA).bin
```

**Option 2: Publisher-based**
```
/Library/
  /Nintendo/
    Super Mario World (USA) [SNES].sfc
    The Legend of Zelda (USA) [NES].nes
  /Square/
    Chrono Trigger (USA) [SNES].sfc
    Final Fantasy VII (Disc 1) (USA) [PlayStation].cue
```

**Option 3: Hybrid (System > Publisher)**
```
/Library/
  /SNES/
    /Nintendo/
      Super Mario World (USA).sfc
    /Square/
      Chrono Trigger (USA).sfc
```

**Option 4: Flat with metadata**
```
/Library/
  Chrono Trigger (USA) (1995) [SNES].sfc
  Final Fantasy VII (Disc 1) (USA) (1997) [PlayStation].cue
  Super Mario World (USA) (1990) [SNES].sfc
```

### Recommended Default
**System-based** (Option 1) with one folder per game for multi-file games:

```
/Library/
  /PlayStation/
    /Final Fantasy VII (USA)/
      Final Fantasy VII (Disc 1) (USA).cue
      Final Fantasy VII (Disc 1) (USA).bin
      Final Fantasy VII (Disc 2) (USA).cue
      Final Fantasy VII (Disc 2) (USA).bin
      Final Fantasy VII (Disc 3) (USA).cue
      Final Fantasy VII (Disc 3) (USA).bin
```

## Matching Rules

### Hash Matching
1. Calculate file hash (CRC32/MD5/SHA1 depending on system)
2. Query provider by hash
3. If exact match found, confidence = 100%

### Filename Matching
1. Parse filename to extract:
   - Title
   - Region codes
   - Tags (revision, hack, translation, etc.)
2. Query provider by title
3. If exact match found, confidence = 90%
4. If similar match found, confidence = 60-80% (based on string similarity)

### Fuzzy Matching
1. Use Levenshtein distance or similar algorithm
2. Account for common variations:
   - "The" prefix
   - Subtitle separators (: vs -)
   - Special characters
3. Confidence = similarity score (0-100%)

### Manual Override
1. User can search and select correct match
2. Store override in database
3. Confidence = 100% (user confirmed)

## Safety Requirements

### Dry-run Mode
- All operations must support preview mode
- Show before/after state
- Require user confirmation before applying

### Undo Support
- Store original paths and metadata
- Allow rollback of rename/move operations
- Keep undo history (configurable limit)

### Collision Handling
- Detect filename conflicts before moving
- Offer resolution strategies:
  - Skip
  - Rename with suffix (e.g., " (1)", " (2)")
  - Overwrite (with confirmation)

### Backup Option
- Optional: copy files instead of move
- Keep original files in place

## Metadata Storage

### Cached Metadata
- Store fetched metadata in local database
- Include cache timestamp
- Allow refresh/re-scrape

### Artwork Storage
- Download and cache artwork locally
- Support multiple artwork types:
  - Box art / cover
  - Screenshot
  - Banner
  - Logo
  - Video (optional)
- Configurable storage location

## User Preferences

### Required Settings
- Library paths (can be multiple)
- Metadata provider credentials
- Default naming template
- Default organization structure

### Optional Settings
- Preferred providers order
- Confidence threshold for auto-match
- Artwork types to download
- Cache location and size limits
- Undo history depth
- CHD conversion preferences
- M3U auto-generation for multi-disc games
- Naming convention preference (No-Intro/Redump vs custom)

## File Conversion & Compression

### CHD Compression Support

Remus supports converting disc images to CHD (Compressed Hunks of Data) format for space savings and better organization.

#### Supported Conversions
- **BIN/CUE → CHD** (PlayStation, Sega CD, Saturn, etc.)
- **ISO → CHD** (PlayStation 2, Dreamcast, GameCube)
- **GDI → CHD** (Dreamcast)
- **CHD → BIN/CUE** (extraction/reversal)

#### Benefits
- 30-60% space savings (lossless compression)
- Single file instead of BIN+CUE pairs
- Faster loading in many emulators
- Wide compatibility (RetroArch, MAME, standalone emulators)

#### Conversion Workflow
1. User selects games to convert
2. Preview space savings
3. Batch conversion with progress tracking
4. Automatic verification of CHD integrity
5. Optional: Delete originals after successful conversion
6. Database automatically updated with new file paths

#### M3U Auto-Generation
For multi-disc games converted to CHD:
1. Detect all discs of same game
2. Convert each disc to CHD
3. Auto-generate M3U playlist file
4. Structure: One .m3u file + multiple .chd files
5. Result: Single entry in game list with in-game disc swapping

Example output:
```
Final Fantasy VII (USA).m3u
Final Fantasy VII (USA) (Disc 1).chd
Final Fantasy VII (USA) (Disc 2).chd
Final Fantasy VII (USA) (Disc 3).chd
```

#### Safety Features
- Verify CHD integrity before deleting originals
- Undo support (re-extract from CHD if needed)
- Batch operation with error handling
- Test in emulator option before cleanup

### Archive Extraction

Support for extracting games from archives:
- **ZIP** → Extract ROM files
- **7z** → Extract ROM files
- **RAR** → Extract ROM files

Auto-detect and extract archives during scanning.
## Verification and Patching

### ROM Verification

Remus validates ROM authenticity against community-maintained databases:

#### Supported DAT Formats
- **No-Intro**: XML DAT files for cartridge-based systems
- **Redump**: XML DAT files for disc-based systems
- Format: Logiqx XML standard with CRC32/MD5/SHA1 checksums

#### Verification Features
1. **Import DAT Files**: Load No-Intro/Redump DAT files per system
2. **Batch Verification**: Check entire library or specific systems
3. **Hash Comparison**: Compare calculated hashes against DAT entries
4. **Header Handling**: Auto-strip copier headers (NES, Lynx) before hashing
5. **Results Categorization**:
   - ✅ **Verified**: Exact match with DAT (authentic dump)
   - ❌ **Failed**: Hash mismatch (corrupted or modified)
   - ❓ **Unknown**: Not in DAT (hacks, translations, bad dumps)

#### Verification Outputs
- Status badges in library view
- Detailed verification reports (CSV/JSON export)
- Missing games list (in DAT but not in library)
- Failed verification list (potential corruption)

#### Header Stripping Support

| System | Header Size | Action |
|--------|-------------|--------|
| NES (iNES) | 16 bytes | Strip before hashing |
| Lynx | 64 bytes | Strip before hashing |
| SNES (SMC) | 512 bytes | Strip if present |

### ROM Patching

Apply community-created patches for translations, hacks, and improvements.

#### Supported Patch Formats

| Format | Max File Size | Checksum Verification | Primary Use |
|--------|---------------|----------------------|-------------|
| **IPS** | 16 MB | No | Legacy NES/SNES patches |
| **BPS** | Unlimited | Yes (source, target, patch) | Modern standard (recommended) |
| **UPS** | Unlimited | Yes | Alternative to BPS |
| **PPF** | Unlimited | Yes | PlayStation disc images |
| **XDelta3** | Unlimited | Yes | Large disc-based games |
| **APS** | Varies | Yes | GBA/N64 specific |

**Note**: BPS format is preferred as it verifies the base ROM matches the patch requirements, preventing errors.

#### Patching Tools Integration

Remus bundles these open-source patching engines:

1. **Flips (Floating IPS)**
   - Formats: IPS, BPS
   - License: GPL
   - Platform: Linux native
   - Command: `flips --apply patch.bps base.rom output.rom`

2. **xdelta3**
   - Formats: XDelta3
   - License: GPL
   - Platform: Cross-platform
   - Command: `xdelta3 -d -s base.iso patch.xdelta output.iso`

#### Patching Workflows

**Manual Patch Application (M8)**:
1. User downloads patch from romhacking.net or other source
2. Tools → Apply Patch
3. Select base ROM + patch file
4. Remus detects format automatically
5. Applies patch with appropriate tool
6. Names output: `Game Title [Patch Name vX.X].ext`
7. Links patched ROM to base ROM in database

**Semi-Automatic Patching (M9)**:
1. User provides romhacking.net URL
2. Remus scrapes patch metadata
3. Downloads patch file
4. Identifies matching ROM in library by hash/name
5. User reviews and confirms
6. Applies patch automatically

**Future: Patch Discovery**:
1. Remus queries romhacking.net for patches
2. Matches available patches to library games
3. Suggests popular patches (translations, improvements)
4. One-click apply with automatic download

#### romhacking.net Integration

- **No official API**: Web scraping with rate limiting (1 req/2s)
- **Patch Metadata**: Title, author, version, system, format, ratings
- **Legal**: Patches are freely distributable (contain only binary differences)
- **robots.txt Compliance**: Respect crawling rules
- **Caching**: Store patch metadata locally, refresh weekly

#### Patch Metadata Tracking

Database stores:
- Patch file information (title, author, version, format)
- Base ROM requirements (CRC32/MD5/SHA1)
- Application history (base ROM → patched ROM relationship)
- Source URL (romhacking.net ID and download link)
- Ratings and download counts

#### Organization After Patching

Patched ROMs organized alongside originals:
```
/SNES/
  Super Mario World (USA).sfc                      [verified original]
  Super Mario World (USA) [Kaizo Mario v3.0].sfc  [patched - hack]
  Super Mario World (USA) [Return to Dino v2.1].sfc [patched - hack]
```

Metadata distinguishes:
- `file_type: "official"` - Verified No-Intro/Redump dump
- `file_type: "translation"` - Fan translation patch applied
- `file_type: "hack"` - ROM hack patch applied
- `file_type: "homebrew"` - Homebrew/indie game
- `file_type: "prototype"` - Pre-release/prototype dump

#### Safety Features
1. **Backup Originals**: Never modify source ROM
2. **BPS Verification**: Automatic base ROM checksum verification
3. **Dry-Run Preview**: Show what will be created before applying
4. **Undo Support**: Restore original, delete patched file
5. **Collision Detection**: Prompt if patched file already exists

#### Patch Discovery UI

```
┌─────────────────────────────────────────────────────────┐
│ 🔧 Available Patches for Your Library (15 found)       │
├─────────────────────────────────────────────────────────┤
│ ✅ Final Fantasy VI - Brave New World v2.1             │
│    Type: Improvement | IPS | ⭐ 4.9 | 50k downloads    │
│    Base: Final Fantasy III (USA).sfc [in library]      │
│    [Apply] [Details] [Ignore]                          │
├─────────────────────────────────────────────────────────┤
│ ✅ Fire Emblem - English Translation v1.0              │
│    Type: Translation | BPS | ⭐ 5.0 | 120k downloads   │
│    Base: Fire Emblem (Japan).gba [in library]          │
│    [Apply] [Details] [Ignore]                          │
└─────────────────────────────────────────────────────────┘
```