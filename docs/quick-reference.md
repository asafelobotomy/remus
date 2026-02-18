# Quick Reference Guide

## Naming Convention Quick Reference

### No-Intro Format (Cartridge ROMs)
```
Title (Region) (Languages) (Version) (Status) (Additional) [Tags]
```

**Examples:**
```
✓ Super Mario World (USA).sfc
✓ Legend of Zelda, The (USA).nes
✓ Final Fantasy VI (USA) (Rev 1).sfc
✓ Chrono Trigger (USA, Europe).sfc
✓ Pokemon Red (USA, Europe) (Rev A).gb
✗ Super Mario World (U).sfc              // Wrong: Use full region name
✗ The Legend of Zelda (USA).nes          // Wrong: Article placement
✗ Final Fantasy VI (USA) (v1.0).sfc      // Wrong: Don't show initial version
```

### Redump Format (Disc Games)
```
Title (Region) (Version) (Additional) (Disc X)
```

**Examples:**
```
✓ Final Fantasy VII (USA) (Disc 1).chd
✓ Metal Gear Solid (USA) (Rev 1).chd
✓ Gran Turismo 2 (USA) (Greatest Hits).chd
✓ Tekken 3 (Europe) (Fr,De,Es,It).chd
✗ Final Fantasy VII (U) (Disc 1).chd     // Wrong: Use full region name
✗ SCUS-94163 - Final Fantasy VII.chd     // Wrong: No serials in filename
```

### Multi-Disc Enhanced Format
```
Title (Region) (Disc X of Y)
```

**Examples:**
```
✓ Final Fantasy VII (USA) (Disc 1 of 3).chd
✓ Final Fantasy VIII (USA) (Disc 2 of 4).chd
```

## Region Codes

### Required Format (Full Names)
```
✓ (USA)
✓ (Europe)
✓ (Japan)
✓ (World)
✓ (Asia)
✓ (Australia)
✓ (Korea)
✓ (USA, Europe)          // Multi-region
```

### DO NOT USE (Old GoodTools Format)
```
✗ (U)
✗ (E)
✗ (J)
✗ (W)
```

## Language Codes
```
En  = English
Fr  = French
De  = German
Es  = Spanish
It  = Italian
Pt  = Portuguese
Nl  = Dutch
Sv  = Swedish
No  = Norwegian
Da  = Danish
Fi  = Finnish
Zh  = Chinese
Ja  = Japanese
Ko  = Korean
```

**Example:** `(En,Fr,De)` for English, French, German

## Version & Status Tags

### Version (Only if > Initial Release)
```
(Rev 1), (Rev A), (Rev B)    // Revision (from cartridge stamp)
(v1.1), (v1.2)               // Version (from ROM header)
```

**DO NOT include initial versions:**
```
✗ (v1.0)    // Initial version, omit
```

### Development/Commercial Status
```
(Beta)          // Beta release
(Proto)         // Prototype
(Sample)        // Demo/Sample/Kiosk
(Demo)          // Common but should be (Sample)
```

### Additional Info
```
(Limited Edition)
(Greatest Hits)
(Player's Choice)
(Virtual Console)
(Rerelease)
(Rumble Version)
```

## Square Bracket Tags

```
[!]        // Verified good dump (use this!)
[b]        // Bad dump
[h]        // Hack/ROM hack
[t]        // Trained (cracked with trainer)
[f]        // Fixed
[T+Eng]    // Fan translation to English
[T+Fra]    // Fan translation to French
[o]        // Overdump
[BIOS]     // BIOS file
```

## File Extensions by System

### Cartridge ROMs
```
.nes        // NES
.sfc, .smc  // SNES
.md, .gen   // Genesis/Mega Drive
.sms        // Master System
.gb         // Game Boy
.gbc        // Game Boy Color
.gba        // Game Boy Advance
.nds        // Nintendo DS
.n64        // Nintendo 64
```

### Disc Images
```
.chd        // Compressed (RECOMMENDED)
.cue        // Cue sheet (primary file)
.bin        // Binary data (linked to .cue)
.iso        // ISO image
.gdi        // Dreamcast GDI
.m3u        // Multi-disc playlist
```

## M3U Playlist Format

**File:** `Final Fantasy VII (USA).m3u`

**Contents:**
```
Final Fantasy VII (USA) (Disc 1).chd
Final Fantasy VII (USA) (Disc 2).chd
Final Fantasy VII (USA) (Disc 3).chd
```

**OR with relative paths:**
```
./discs/Final Fantasy VII (USA) (Disc 1).chd
./discs/Final Fantasy VII (USA) (Disc 2).chd
./discs/Final Fantasy VII (USA) (Disc 3).chd
```

## CHD Conversion Commands

### Convert CUE/BIN to CHD
```bash
chdman createcd -i "game.cue" -o "game.chd"
```

### Convert ISO to CHD
```bash
chdman createcd -i "game.iso" -o "game.chd"
```

### Convert GDI to CHD (Dreamcast)
```bash
chdman createcd -i "game.gdi" -o "game.chd"
```

### Extract CHD back to CUE/BIN
```bash
chdman extractcd -i "game.chd" -o "game.cue"
```

### Verify CHD
```bash
chdman verify -i "game.chd"
```

### Batch Convert (Linux/Mac)
```bash
for file in *.cue; do
    chdman createcd -i "$file" -o "${file%.cue}.chd"
done
```

## Folder Structure Standards

### RetroArch
```
/roms/
  /nes/
  /snes/
  /psx/
```

### EmulationStation DE / EmuDeck
```
/roms/
  /nes/
  /snes/
  /psx/
  /ps2/
```

### Multi-Disc Game Structure (Option 1)
```
/roms/psx/
  Final Fantasy VII (USA).m3u
  Final Fantasy VII (USA) (Disc 1).chd
  Final Fantasy VII (USA) (Disc 2).chd
  Final Fantasy VII (USA) (Disc 3).chd
```

### Multi-Disc Game Structure (Option 2)
```
/roms/psx/
  Final Fantasy VII (USA).m3u
  /.discs/
    Final Fantasy VII (USA) (Disc 1).chd
    Final Fantasy VII (USA) (Disc 2).chd
    Final Fantasy VII (USA) (Disc 3).chd
```

### Multi-Disc Game Structure (Option 3)
```
/roms/psx/
  /Final Fantasy VII (USA)/
    Final Fantasy VII (USA).m3u
    Final Fantasy VII (USA) (Disc 1).chd
    Final Fantasy VII (USA) (Disc 2).chd
    Final Fantasy VII (USA) (Disc 3).chd
```

## Hash Algorithms by System

### Cartridge-based (Use CRC32)
- NES, SNES, Genesis, Game Boy, GBA, N64

### Disc-based (Use MD5 or SHA1)
- PlayStation, PS2, Sega CD, Saturn, Dreamcast, GameCube

## Common Mistakes to Avoid

### ❌ Wrong Region Format
```
✗ Super Mario World (U).sfc
✓ Super Mario World (USA).sfc
```

### ❌ Wrong Article Placement
```
✗ The Legend of Zelda (USA).nes
✓ Legend of Zelda, The (USA).nes
```

### ❌ Showing Initial Version
```
✗ Final Fantasy VI (USA) (v1.0).sfc
✓ Final Fantasy VI (USA).sfc
```

### ❌ Including Serial Numbers
```
✗ SCUS-94163 - Final Fantasy VII (USA).chd
✓ Final Fantasy VII (USA).chd
```

### ❌ Special Characters in Filenames
```
✗ Final Fantasy VII: Advent Children (USA).chd
✓ Final Fantasy VII - Advent Children (USA).chd
```
(Replace `:` with `-`)

### ❌ Mixed Case in Folders
```
✗ /roms/PlayStation/
✓ /roms/psx/
```
(Use lowercase, standard abbreviations)

## Metadata Providers

### Hash-Based (Best Accuracy)

#### ScreenScraper ⭐ (Implemented)
- **Hash Support:** ✅ CRC32, MD5, SHA1
- **Best for:** Verified No-Intro/Redump dumps
- **Requires:** Account (free tier available)
- **Rate Limits:** 1 req/2s, 10k req/day
- **API:** Hash + name search
- **Coverage:** 125+ systems

#### Hasheous 🆕 (Recommended for M3)
- **Hash Support:** ✅ MD5, SHA1
- **Best for:** No-auth hash matching
- **Requires:** None
- **Rate Limits:** Standard (533ms avg)
- **API:** Hash matching (proxies IGDB)
- **Coverage:** 135+ systems
- **Cost:** FREE

#### RetroAchievements 🆕
- **Hash Support:** ✅ Custom per-system
- **Best for:** Achievements + verification
- **Requires:** API key
- **Coverage:** 70+ systems

### Name-Based (Fallback)

#### TheGamesDB (Implemented)
- **Hash Support:** ❌ Name only
- **Best for:** Wide coverage, free tier
- **Requires:** Optional API key
- **Rate Limits:** 1 req/s
- **API:** Name search

#### IGDB (Implemented)
- **Hash Support:** ❌ Name only
- **Best for:** Modern + retro, richest metadata
- **Requires:** Twitch credentials
- **Rate Limits:** 4 req/s
- **API:** Advanced search, 200+ systems

## Emulator Compatibility

| Frontend | Naming | M3U | CHD | Metadata |
|----------|--------|-----|-----|----------|
| RetroArch | No-Intro/Redump | ✓ | ✓ | .lpl |
| ES-DE | No-Intro/Redump | ✓ | ✓ | gamelist.xml |
| EmuDeck | No-Intro/Redump | ✓ | ✓ (preferred) | gamelist.xml |
| RetroDeck | No-Intro/Redump | ✓ | ✓ (preferred) | gamelist.xml |
| LaunchBox | Flexible | ✓ | ✓ | LaunchBox XML |

## Character Restrictions

### Allowed (Low ASCII)
```
a-z A-Z 0-9 SPACE $ ! # % ' ( ) + , - . ; = @ [ ] ^ _ { } ~
```

### Forbidden
```
\ / : * ? " < > | `
```

### Filename Rules
- No leading/trailing spaces or dots
- No double spaces
- Convert special characters to Low ASCII
- Replace `:` with `-` in titles

## Quick Validation Checklist

✓ Region in full form `(USA)` not `(U)`  
✓ Article at end: `Zelda, The` not `The Zelda`  
✓ Version only if > v1.0  
✓ Low ASCII characters only  
✓ Multi-disc games have M3U  
✓ Disc images compressed to CHD  
✓ System-based folder structure  
✓ `[!]` tag on verified dumps  

## File Size Reference

| System | Original | CHD | Savings |
|--------|----------|-----|---------|
| PSX | 700 MB | 350 MB | ~50% |
| PS2 | 4.7 GB | 3 GB | ~36% |
| Dreamcast | 1.2 GB | 700 MB | ~42% |
| Sega CD | 600 MB | 350 MB | ~42% |
| Saturn | 600 MB | 400 MB | ~33% |
## Verification Commands (M8)

### Import DAT Files
```bash
# Import No-Intro DAT for NES
remus verify --import-dat ~/Downloads/No-Intro_NES_2024.dat

# Import Redump DAT for PlayStation
remus verify --import-dat ~/Downloads/Redump_PSX_2024.dat
```

### Verify ROMs
```bash
# Verify entire library
remus verify --all

# Verify specific system
remus verify --system NES

# Verify single file
remus verify --file "/path/to/Super Mario Bros. (USA).nes"

# Show missing ROMs (in DAT but not in library)
remus verify --missing --system SNES

# Export verification report
remus verify --report ~/reports/verification-2026-02-05.csv
```

### Verification Status Values
```
✅ verified      - Exact hash match with DAT (authentic dump)
❌ failed        - Hash mismatch (corrupted or modified file)
❓ unknown       - Not in DAT (hack, translation, bad dump, unlicensed)
⏭️  not_checked  - Verification not run yet
```

## Patching Commands (M8)

### Apply Patches Manually
```bash
# Apply BPS patch (recommended, includes checksums)
remus patch --apply \
  --base "/path/to/Super Mario Bros. (USA).nes" \
  --patch ~/Downloads/smb_deluxe.bps \
  --output "/path/to/Super Mario Bros. (USA) [Deluxe].nes"

# Apply IPS patch (legacy format)
remus patch --apply \
  --base "/path/to/Final Fantasy VI (USA).sfc" \
  --patch ~/Downloads/ff6_brave_new_world.ips \
  --output "/path/to/Final Fantasy VI (USA) [BNW v2.1].sfc"

# Apply XDelta3 patch (disc images)
remus patch --apply \
  --base "/path/to/Final Fantasy VII (USA) (Disc 1).bin" \
  --patch ~/Downloads/ff7_retranslation.xdelta \
  --output "/path/to/Final Fantasy VII (USA) (Disc 1) [Retranslation].bin"
```

### Patch Discovery
```bash
# Discover available patches for games in library
remus patch --discover

# Discover patches for specific game
remus patch --discover --game "Final Fantasy VI"

# Download and apply patch by romhacking.net ID
remus patch --apply-from-web \
  --romhacking-id 1234 \
  --base "/path/to/Final Fantasy VI (USA).sfc"
```

### Patch Management
```bash
# List all applied patches
remus patch --list-applied

# Show patch history for a file
remus patch --history --file "/path/to/Super Mario Bros. (USA) [Deluxe].nes"

# Unapply patch (restore original, delete patched)
remus patch --unapply --file "/path/to/Super Mario Bros. (USA) [Deluxe].nes"
```

### Supported Patch Formats
```
IPS      - Max 16 MB, no checksums (legacy)
BPS      - Unlimited, checksums included (recommended)
UPS      - Unlimited, checksums included
PPF      - PlayStation disc patches
XDelta3  - Large files, delta compression
APS      - GBA/N64 specific formats
```

### Flips (BPS/IPS Tool) Commands
```bash
# Apply BPS patch
flips --apply patch.bps "base.rom" "output.rom"

# Create BPS patch
flips --create "original.rom" "modified.rom" "output.bps"

# Verify BPS patch integrity
flips --apply patch.bps "base.rom" "output.rom" --verbose
```

### XDelta3 Commands
```bash
# Apply XDelta3 patch
xdelta3 -d -s "base.iso" "patch.xdelta" "output.iso"

# Create XDelta3 patch
xdelta3 -e -s "original.iso" "modified.iso" "output.xdelta"
```