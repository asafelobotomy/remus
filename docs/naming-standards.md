# ROM Naming Standards & Community Conventions

## Overview
This document details the ROM naming conventions used by the retro gaming community, focusing on No-Intro (cartridge-based) and Redump (disc-based) standards, which are considered the gold standard for ROM preservation and organization.

## No-Intro Convention

**Primary use:** Cartridge-based systems (NES, SNES, Genesis, GB, GBA, N64, etc.)

No-Intro is a preservation project focused on accurate ROM dumps with standardized naming. The No-Intro naming convention (last updated 2007-10-30) provides a structured approach to naming ROMs.

### Filename Format

```
Title (Region) (Languages) (Version) (Status) (Additional) [Special] [Tags]
```

### Field Breakdown

#### Title
- Game title in English (or romanized if non-English)
- Proper capitalization
- Special characters converted to Low ASCII
- Articles like "The" moved to end with comma: `Legend of Zelda, The`
- Subtitles separated by space-hyphen-space: `Final Fantasy VII - Crisis Core`

#### Region (Required)
Full region names in parentheses, comma-separated if multiple:
- `(USA)` - United States
- `(Europe)` - Europe
- `(Japan)` - Japan
- `(World)` - Worldwide release
- `(Asia)` - Asia
- `(Australia)` - Australia
- `(Korea)` - Korea
- `(USA, Europe)` - Multi-region

**NOT single-letter codes:** Avoid `(U)`, `(E)`, `(J)` - use full names.

#### Languages (Optional)
Only if multiple languages and differs from standard for region:
- `(En,Fr,De)` - English, French, German
- Language codes: En, Fr, De, Es, It, Pt, Nl, Sv, No, Da, Fi, Zh, Ja, Ko

#### Version (Optional)
Only shown if **greater than initial release**:
- `(Rev 1)`, `(Rev A)` - Revisions (from cartridge stamps)
- `(v1.1)`, `(v1.2)` - Versions (from ROM header)

**Initial versions like v1.0 are NOT shown.**

#### Development/Commercial Status (Optional)
- `(Beta)` - Beta release
- `(Proto)` - Prototype
- `(Sample)` - Sample/Demo/Kiosk demo
- `(Demo)` - Common but should be `(Sample)` per spec

#### Additional (Optional)
Only to differentiate between multiple releases:
- `(Limited Edition)`
- `(Rumble Version)`
- `(Virtual Console)`
- `(Rerelease)`
- Edition names in native language: `(Genteiban)`, `(Doukonban)`

#### Special Tags (Square Brackets)
System-specific information:
- `[!]` - Verified good dump
- `[b]` - Bad dump
- `[h]` - Hacked ROM
- `[t]` - Trained (cracked with trainer)
- `[f]` - Fixed
- `[T+Eng]` - Translation patch (English)
- `[o]` - Overdump
- `[BIOS]` - BIOS file

### Examples

```
Super Mario World (USA).sfc
Legend of Zelda, The - A Link to the Past (USA).sfc
Final Fantasy VI (USA) (Rev 1).sfc
Chrono Trigger (USA) (Sample).sfc
Super Mario World (USA, Europe) [!].sfc
Rockman (Japan) [T+Eng].nes
```

## Redump Convention

**Primary use:** Disc-based systems (PlayStation, Sega CD, Saturn, Dreamcast, PS2, GameCube, etc.)

Redump adopted the No-Intro naming convention in 2009 with disc-specific modifications.

### Disc-Specific Differences

#### Serial Numbers
- Historically included, now deprecated
- Modern Redump prefers No-Intro style without serials

#### Multi-Disc Games
Standard format:
```
Game Title (Region) (Disc 1)
Game Title (Region) (Disc 2)
Game Title (Region) (Disc 3)
```

Enhanced format (community preference):
```
Game Title (Region) (Disc 1 of 3)
Game Title (Region) (Disc 2 of 3)
Game Title (Region) (Disc 3 of 3)
```

#### Bonus/Extra Discs
```
Game Title (Bonus CD) (Region)
Game Title (Bonus DVD) (Region) (Limited Edition)
```

#### File Extensions
- `.cue` - Cue sheet (primary file)
- `.bin` - Binary data (secondary, linked to .cue)
- `.iso` - ISO image
- `.chd` - Compressed Hunks of Data (MAME format)

### Examples

```
Final Fantasy VII (USA) (Disc 1).cue
Final Fantasy VII (USA) (Disc 1).bin
Metal Gear Solid (USA) (Rev 1).cue
Tekken 3 (USA) (Greatest Hits).cue
Akumajou Dracula X - Gekka no Yasoukyoku (Bonus CD) (Japan).cue
```

## TOSEC Convention

**The Old School Emulation Center**

TOSEC focuses on complete software preservation including multiple releases, cracks, trainers, and variants. Less strict than No-Intro/Redump.

### Format
```
Title (Year)(Publisher)(System)(Country)(Language)(Copyright Status)(Development Status)(Media Type)(Media Label)[cr][f][h][m][p][t][tr][o][u][v][b][a][!][more info]
```

### Key Differences from No-Intro
- More verbose, includes publisher and year
- More granular crack/hack tracking
- Preserves everything (including bad dumps)
- Used primarily for computer systems (Amiga, C64, Atari ST)

## GoodTools Convention (Deprecated)

GoodTools was an early ROM organization tool using single-letter region codes. **Now deprecated in favor of No-Intro/Redump.**

Examples of old GoodTools naming:
```
Super Mario World (U) [!].smc
Zelda (U) (V1.1).nes
```

**Do not use** - convert to No-Intro format.

## Character Restrictions

### Allowed Characters (Low ASCII only)
```
a-z A-Z 0-9 SPACE $ ! # % ' ( ) + , - . ; = @ [ ] ^ _ { } ~
```

### Forbidden Characters
```
\ / : * ? " < > | `
```

### Special Character Conversion
- `é` → `e`
- `ñ` → `n`
- `ü` → `u`
- `:` → `-` (in titles)
- `™`, `®` → removed

### Filename Rules
- No leading/trailing spaces or dots
- No double spaces
- Maximum compatibility across file systems

## Multi-Disc Handling for Emulators

### M3U Playlist Files

RetroArch, ES-DE, and most modern emulators support `.m3u` playlist files for multi-disc games.

#### Format
Plain text file with `.m3u` extension containing paths to disc images:

**File: `Final Fantasy VII (USA).m3u`**
```
Final Fantasy VII (USA) (Disc 1).chd
Final Fantasy VII (USA) (Disc 2).chd
Final Fantasy VII (USA) (Disc 3).chd
```

Or with relative paths:
```
./discs/Final Fantasy VII (USA) (Disc 1).chd
./discs/Final Fantasy VII (USA) (Disc 2).chd
./discs/Final Fantasy VII (USA) (Disc 3).chd
```

#### Benefits
- Single entry in game list
- In-game disc swapping via RetroArch menu
- Maintains save states across discs
- Better organization

#### Recommended Structure
```
/PlayStation/
  /Final Fantasy VII (USA)/
    Final Fantasy VII (USA).m3u
    Final Fantasy VII (USA) (Disc 1).chd
    Final Fantasy VII (USA) (Disc 2).chd
    Final Fantasy VII (USA) (Disc 3).chd
```

Or flattened with hidden discs:
```
/PlayStation/
  Final Fantasy VII (USA).m3u
  /.discs/
    Final Fantasy VII (USA) (Disc 1).chd
    Final Fantasy VII (USA) (Disc 2).chd
    Final Fantasy VII (USA) (Disc 3).chd
```

## Emulator-Specific Naming Requirements

### RetroArch
- **Playlist scanner** uses hash matching against internal database
- **Game names** derived from database, not filenames
- Supports No-Intro and Redump naming
- Manual scans work with any naming if files are recognized
- **Thumbnails** matched by database name

### EmulationStation DE (ES-DE)
- Scraper uses **filename parsing** + hash matching
- Works best with No-Intro/Redump naming
- Generates `gamelist.xml` with metadata
- Supports custom metadata overrides
- Artwork named by ROM filename (without extension)

### EmuDeck / RetroDeck
- Pre-configured for Steam Deck
- Uses **system-based folder structure**: `/roms/nes/`, `/roms/snes/`, etc.
- RetroArch + ES-DE underneath
- Works with No-Intro/Redump naming
- Steam ROM Manager for artwork

### Recommended Best Practices

1. **Use No-Intro for cartridges, Redump for discs**
2. **Include region in full form:** `(USA)` not `(U)`
3. **Omit version if initial release:** No `(v1.0)`
4. **Use m3u files for multi-disc games**
5. **Compress disc images to CHD** (see below)
6. **Organize by system folders**
7. **Use `[!]` tag to mark verified dumps**

### Folder Structure (Recommended)
```
/roms/
  /nes/
    Super Mario Bros. (USA).nes
    Legend of Zelda, The (USA).nes
  /snes/
    Super Mario World (USA).sfc
    Chrono Trigger (USA).sfc
  /psx/
    Final Fantasy VII (USA).m3u
    /.discs/
      Final Fantasy VII (USA) (Disc 1).chd
      Final Fantasy VII (USA) (Disc 2).chd
      Final Fantasy VII (USA) (Disc 3).chd
  /ps2/
    Metal Gear Solid 3 - Snake Eater (USA).chd
```

## CHD Compression

See [CHD Conversion Guide](chd-conversion.md) for detailed information on converting disc images to compressed CHD format.

## Region Priority

When you have multiple region versions of the same game, recommended priority:

1. **USA** (if playing in English, NTSC timing)
2. **Europe** (if playing in English, PAL timing)
3. **Japan** (original release, often has content removed in Western versions)
4. **World** (if available)

## Special Cases

### Hacks & Translations
```
Super Mario World (USA) [T+Eng].sfc           // Translation patch
Super Mario World (USA) [h Kaizo].sfc         // Kaizo hack
Zelda (Japan) [T+Eng v1.0 by TranslatorName].nes
```

### Homebrew
```
Original Game (World) (Unl) (Homebrew).gb     // Unlicensed homebrew
```

### Collections & Compilations
```
Super Mario All-Stars (USA).sfc
Sonic Mega Collection (USA).iso
```

### Updated Re-releases
```
Super Mario 64 (USA).n64                      // Original
Super Mario 64 (USA) (Shindou Edition).n64    // Re-release
```

## Hash Matching Priority

For accurate metadata matching:

1. **CRC32** - Cartridge ROMs (NES, SNES, Genesis, GB, GBA)
2. **MD5** - Disc images, some cartridges
3. **SHA1** - Disc images, verification
4. **Filename parsing** - Fallback if hash not in database
5. **Fuzzy matching** - Last resort

## References

- No-Intro Convention: https://wiki.no-intro.org/index.php?title=Naming_Convention
- Redump: http://redump.org/
- TOSEC: https://www.tosecdev.org/
- RetroArch Docs: https://docs.libretro.com/
- ES-DE User Guide: https://gitlab.com/es-de/emulationstation-de/-/blob/master/USERGUIDE.md
