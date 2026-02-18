# Research Summary: ROM Naming & Community Standards

## Date: February 5, 2026

This document summarizes comprehensive research into current ROM naming conventions, emulator compatibility requirements, and file format standards used in the retro gaming community.

## Key Findings

### 1. Naming Convention Standards

**No-Intro** (cartridge-based systems) and **Redump** (disc-based systems) are the **gold standard** for ROM preservation and naming.

#### No-Intro Convention (2007-10-30)
- Used by preservation community for cartridge ROMs
- Strict format: `Title (Region) (Languages) (Version) (Status) (Additional) [Tags]`
- Full region names: `(USA)`, `(Europe)`, `(Japan)` - NOT single letters
- Version only shown if > initial release
- Article handling: "The Legend of Zelda" → `Legend of Zelda, The`
- Low ASCII characters only (cross-platform compatibility)

#### Redump Convention
- Adopted No-Intro naming in 2009
- Specific to disc-based systems
- Multi-disc format: `Title (Region) (Disc 1)` or `(Disc 1 of 3)`
- Deprecated serial numbers in filenames
- Focus on preservation accuracy

#### Deprecated Standards
- **GoodTools** - Old single-letter region codes `(U)`, `(E)`, `(J)` - avoid
- **TOSEC** - More verbose, used for computer systems (Amiga, C64), less strict

### 2. Multi-Disc Game Handling

**M3U Playlists** are the universal standard for multi-disc games.

#### Format
Plain text file with `.m3u` extension listing disc paths:
```
Final Fantasy VII (USA) (Disc 1).chd
Final Fantasy VII (USA) (Disc 2).chd
Final Fantasy VII (USA) (Disc 3).chd
```

#### Benefits
- Single entry in game list
- In-game disc swapping via emulator menu
- Maintains save states across discs
- Supported by: RetroArch, ES-DE, EmuDeck, Batocera, Recalbox, LaunchBox

#### Recommendation
Auto-generate M3U files for any detected multi-disc game during organization.

### 3. CHD Compression Format

**CHD (Compressed Hunks of Data)** is the standard for disc image compression.

#### Key Points
- Developed by MAME team
- Lossless compression (bit-perfect)
- 30-60% space savings
- Single file replaces BIN+CUE pairs
- Wide emulator support

#### Compression Results
- PlayStation: 700 MB → 300-400 MB (40-50% savings)
- PlayStation 2: 4.7 GB → 2.5-3.5 GB (30-40% savings)
- Dreamcast: 1.2 GB → 600-800 MB (35-50% savings)

#### Tool: chdman
- Official MAME command-line tool
- Cross-platform (Windows, Linux, macOS)
- Basic usage: `chdman createcd -i "game.cue" -o "game.chd"`
- Supports BIN/CUE, ISO, GDI formats

#### CHD Versions
- v5 - Current standard (recommended)
- v4 - Older, some emulators still use
- v3 - Legacy, avoid

### 4. Emulator Frontend Compatibility

Research confirmed compatibility requirements for major frontends:

#### RetroArch
- Uses hash-based playlist scanner
- Game names from internal database, not filename
- Supports No-Intro/Redump naming
- Full M3U and CHD support
- Playlist format: `.lpl` (JSON)

#### EmulationStation DE (ES-DE)
- Filename parsing + hash matching
- Works best with No-Intro/Redump naming
- Metadata stored in `gamelist.xml` per system
- Fixed folder structure by system
- Full M3U and CHD support

#### EmuDeck (Steam Deck)
- Combines RetroArch + ES-DE + standalone emulators
- Folder: `/Emulation/roms/[system]/`
- Steam ROM Manager for Steam library integration
- **CHD format preferred** for disc games
- Supports 160+ systems

#### RetroDeck
- Similar to EmuDeck but as Flatpak
- Folder: `/retrodeck/roms/[system]/`
- Same compatibility as EmuDeck

### 5. File Format Support

#### Disc Images (Priority Order)
1. **CHD** - Compressed, single file, widely supported
2. **CUE/BIN** - Uncompressed, accurate
3. **ISO** - Simple but lacks multi-track support
4. **Others** - GDI (Dreamcast), PBP (PSP), RVZ (GameCube/Wii)

#### Archives
- ZIP - Common for arcade ROMs (MAME)
- 7z - Good compression for ROM sets
- RAR - Less common, proprietary

### 6. Folder Organization Standards

Community consensus on folder structure:

```
/roms/
  /nes/
  /snes/
  /psx/
  /ps2/
  /dreamcast/
  etc.
```

System folder names vary by frontend but generally lowercase or exact system names.

### 7. Region Priority Recommendations

When multiple regions exist:
1. **USA** - NTSC timing, English
2. **Europe** - PAL timing, often multi-language
3. **Japan** - Original release, sometimes has cut content restored
4. **World** - If available, universal release

### 8. Hash Matching Priority

For metadata matching accuracy:
1. **CRC32** - Cartridge ROMs (fast, accurate for small files)
2. **MD5** - Disc images and larger files
3. **SHA1** - Verification and security
4. **Filename parsing** - Fallback
5. **Fuzzy matching** - Last resort

## Implementation Recommendations for Remus

### Critical Features

1. **Default to No-Intro/Redump Naming**
   - Build templates around this standard
   - Parse existing filenames to detect variants
   - Normalize to standard format during organization

2. **M3U Auto-Generation**
   - Detect multi-disc games automatically
   - Generate M3U playlists during organization
   - Option to organize discs in subfolder or keep flat

3. **CHD Conversion Pipeline**
   - Integrate chdman tool
   - Batch conversion with progress tracking
   - Space savings calculator before conversion
   - Verification after conversion
   - Optional: delete originals after successful conversion

4. **Frontend Export Modes**
   - RetroArch (.lpl playlists)
   - ES-DE (gamelist.xml)
   - LaunchBox (XML export)
   - Pegasus (.metadata.txt)
   - Generic (organized folders only)

5. **Hash Database**
   - Use No-Intro and Redump DAT files for hash matching
   - CRC32 for cartridges, MD5/SHA1 for discs
   - Fallback to filename parsing

### User Experience

1. **Smart Detection**
   - Auto-detect naming convention from existing files
   - Suggest normalization to No-Intro/Redump
   - Flag non-standard names

2. **Conversion Wizard**
   - Guide users through CHD conversion
   - Show space savings preview
   - Batch select by system or size threshold

3. **M3U Management**
   - Visual grouping of multi-disc games
   - Preview M3U contents
   - Manual override if auto-detection fails

4. **Target Platform Selection**
   - Let users pick target emulator/frontend
   - Auto-configure paths and metadata format
   - Validate against platform requirements

## References

- No-Intro Wiki: https://wiki.no-intro.org/
- Redump.org: http://redump.org/
- MAME chdman: https://docs.mamedev.org/tools/chdman.html
- RetroArch Docs: https://docs.libretro.com/
- ES-DE User Guide: https://gitlab.com/es-de/emulationstation-de/
- EmuDeck: https://www.emudeck.com/
- RetroDeck: https://retrodeck.net/

## Community Standards Summary

| Aspect | Standard | Rationale |
|--------|----------|-----------|
| Naming | No-Intro/Redump | Preservation accuracy, universal recognition |
| Multi-Disc | M3U playlists | Universal frontend support |
| Compression | CHD v5 | Space savings, wide compatibility |
| Folder Structure | By system | Frontend requirements |
| Region Format | Full names | Clarity, cross-platform safety |
| Hash Algorithm | CRC32 (cart), MD5/SHA1 (disc) | Community databases use these |
| Character Set | Low ASCII only | File system compatibility |

## Next Steps

1. ✅ Document research findings
2. ✅ Update requirements to reflect standards
3. ✅ Update data model for CHD tracking and M3U support
4. ✅ Update milestones to include CHD conversion
5. → Begin M1: Core scanning engine with No-Intro/Redump awareness
6. → Implement filename parser for existing naming conventions
7. → Build CHD conversion integration
8. → Create M3U auto-generation logic
