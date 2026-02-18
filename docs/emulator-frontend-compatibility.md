# Emulator Frontend Compatibility

## Overview

Remus is designed to work seamlessly with popular retro gaming frontends and emulators. This document outlines compatibility requirements, naming conventions, and folder structures for each platform.

## RetroArch

**Platform:** Windows, Linux, macOS, Android, iOS  
**Type:** Multi-system emulator frontend

### Requirements

#### ROM Naming
- RetroArch playlist scanner uses **hash-based matching** against internal database
- Supports No-Intro and Redump naming conventions
- Database contains canonical names; displayed name comes from database, not filename
- Manual scanning works with any naming if system is recognized

#### Folder Structure
RetroArch expects ROMs in configured directories (user-defined):
```
/roms/
  /Nintendo - Nintendo Entertainment System/
  /Nintendo - Super Nintendo Entertainment System/
  /Sony - PlayStation/
```

Or simpler:
```
/roms/
  /nes/
  /snes/
  /psx/
```

#### M3U Support
- Full support for .m3u playlists
- Enables disc swapping via "Disc Control" menu
- M3U file should be in same directory as disc images
- Paths in M3U can be relative or absolute

#### Playlist Format
RetroArch uses `.lpl` (JSON) format for playlists:
```json
{
  "version": "1.5",
  "default_core_path": "",
  "default_core_name": "",
  "label_display_mode": 0,
  "right_thumbnail_mode": 0,
  "left_thumbnail_mode": 0,
  "sort_mode": 0,
  "items": [
    {
      "path": "/roms/nes/Super Mario Bros. (USA).nes",
      "label": "Super Mario Bros.",
      "core_path": "DETECT",
      "core_name": "DETECT",
      "crc32": "3337EC46|crc",
      "db_name": "Nintendo - Nintendo Entertainment System.lpl"
    }
  ]
}
```

### Remus Integration

**Export to RetroArch:**
1. Scan and organize ROMs with Remus
2. Generate RetroArch-compatible `.lpl` playlist files
3. Copy playlists to RetroArch playlists directory
4. Thumbnails mapped to RetroArch thumbnail structure

**Thumbnail Structure:**
```
/thumbnails/
  /Nintendo - Nintendo Entertainment System/
    /Named_Boxarts/
      Super Mario Bros..png
    /Named_Snaps/
      Super Mario Bros..png
    /Named_Titles/
      Super Mario Bros..png
```

## EmulationStation DE (ES-DE)

**Platform:** Windows, Linux, macOS  
**Type:** Frontend for launching emulators

### Requirements

#### ROM Naming
- Scraper uses filename parsing + hash matching
- Works best with No-Intro/Redump naming
- Supports custom metadata via `gamelist.xml`

#### Folder Structure
ES-DE expects system-specific folders in ROMs directory:
```
/ROMs/
  /nes/
  /snes/
  /psx/
  /ps2/
```

System folder names are **case-sensitive** and must match ES-DE's system definitions.

#### Gamelist.xml Format
ES-DE stores metadata in `gamelist.xml` per system:
```xml
<?xml version="1.0"?>
<gameList>
  <game>
    <path>./Super Mario Bros. (USA).nes</path>
    <name>Super Mario Bros.</name>
    <desc>A classic platformer...</desc>
    <rating>0.95</rating>
    <releasedate>19850913T000000</releasedate>
    <developer>Nintendo</developer>
    <publisher>Nintendo</publisher>
    <genre>Platform</genre>
    <players>1-2</players>
    <image>./media/covers/Super Mario Bros.-image.jpg</image>
    <video>./media/videos/Super Mario Bros.-video.mp4</video>
  </game>
</gameList>
```

#### Media Structure
```
/ROMs/
  /nes/
    Super Mario Bros. (USA).nes
    /media/
      /covers/
        Super Mario Bros.-image.jpg
      /screenshots/
        Super Mario Bros.-screenshot.jpg
      /videos/
        Super Mario Bros.-video.mp4
```

#### M3U Support
- Supported for multi-disc games
- M3U file appears as single entry in game list
- Paths in M3U must be relative

### Remus Integration

**Export to ES-DE:**
1. Organize ROMs in ES-DE folder structure
2. Generate `gamelist.xml` with fetched metadata
3. Download artwork to `/media/` subfolders
4. Auto-generate M3U for multi-disc games

**Naming Compatibility:**
- Remus naming templates can target ES-DE format
- Option to strip region tags if user prefers cleaner names

## EmuDeck

**Platform:** Steam Deck (Linux)  
**Type:** Automated emulation setup for Steam Deck

### Requirements

EmuDeck installs:
- RetroArch (for most systems)
- EmulationStation DE (frontend)
- Standalone emulators (PCSX2, Dolphin, etc.)
- Steam ROM Manager (adds games to Steam library)

#### Folder Structure
EmuDeck creates:
```
/Emulation/
  /bios/
  /roms/
    /nes/
    /snes/
    /psx/
    /ps2/
    /gc/
    /wii/
    ... (160+ systems)
  /saves/
  /storage/
  /tools/
```

#### ROM Requirements
- Follows ES-DE naming conventions
- Works with No-Intro/Redump naming
- CHD format recommended for disc games
- M3U playlists for multi-disc games

#### Steam ROM Manager
- Generates Steam shortcuts for ROMs
- Downloads artwork from SteamGridDB
- Creates Steam collections by system

### Remus Integration

**EmuDeck Workflow:**
1. User selects EmuDeck as target platform
2. Remus organizes ROMs into `/Emulation/roms/[system]/`
3. Converts disc images to CHD
4. Generates M3U playlists
5. Exports metadata to ES-DE `gamelist.xml`
6. User runs Steam ROM Manager to add to Steam

**Optimizations:**
- Detect EmuDeck installation path
- Auto-configure system folders
- Suggest CHD conversion for disc games
- Validate BIOS files

## RetroDeck

**Platform:** Steam Deck, Linux  
**Type:** All-in-one retro gaming solution (Flatpak)

### Requirements

RetroDeck is similar to EmuDeck but packaged as a single Flatpak application.

#### Folder Structure
```
/retrodeck/
  /roms/
    /nes/
    /snes/
    /psx/
    ... (system folders)
  /bios/
  /saves/
  /screenshots/
```

#### ROM Requirements
- Same as EmuDeck (ES-DE + RetroArch)
- CHD format preferred
- M3U for multi-disc games

### Remus Integration

Same as EmuDeck, with RetroDeck-specific paths.

## Batocera

**Platform:** Multi-platform (x86, Raspberry Pi, SBCs)  
**Type:** Dedicated retro gaming Linux distribution

### Requirements

#### Folder Structure
```
/userdata/
  /roms/
    /nes/
    /snes/
    /psx/
  /bios/
  /saves/
```

#### ROM Naming
- Uses EmulationStation (like ES-DE)
- Supports No-Intro/Redump naming
- `gamelist.xml` for metadata

#### M3U Support
- Full support
- Required for multi-disc games

### Remus Integration

Export to Batocera format (same as ES-DE).

## Recalbox

**Platform:** Raspberry Pi, x86  
**Type:** Retro gaming distribution

Similar to Batocera:
- `/recalbox/share/roms/[system]/`
- EmulationStation frontend
- `gamelist.xml` metadata
- M3U support

## LaunchBox / Big Box

**Platform:** Windows  
**Type:** Premium frontend (free + paid tiers)

### Requirements

#### ROM Organization
- Flexible folder structure (user-defined)
- Can handle mixed folders or organized by system

#### Metadata
- Uses own XML format and database
- Scrapes from LaunchBox Games DB
- Extensive metadata fields

#### M3U Support
- Supports multi-disc games
- Can auto-generate M3U playlists

### Remus Integration

**Export:**
- Generate LaunchBox-compatible XML
- Map Remus metadata to LaunchBox fields
- Export artwork to LaunchBox images folder

## Pegasus Frontend

**Platform:** Windows, Linux, macOS, Android  
**Type:** Modern, themeable frontend

### Requirements

#### Metadata Format
Uses `.metadata.txt` or `.metadata.pegasus.txt`:
```
collection: Nintendo Entertainment System
shortname: nes
extensions: nes, unf
launch: retroarch -L {file.path}

game: Super Mario Bros.
file: Super Mario Bros. (USA).nes
developer: Nintendo
publisher: Nintendo
genre: Platform
players: 2
description: A classic platformer...
rating: 95%
assets.boxFront: media/box_front.jpg
```

### Remus Integration

Export to Pegasus metadata format.

## Compatibility Matrix

| Frontend | Naming | Folder Structure | M3U | CHD | Metadata Format | Remus Export |
|----------|--------|------------------|-----|-----|-----------------|--------------|
| RetroArch | No-Intro/Redump | Flexible | ✓ | ✓ | .lpl (JSON) | ✓ |
| ES-DE | No-Intro/Redump | Fixed (by system) | ✓ | ✓ | gamelist.xml | ✓ |
| EmuDeck | No-Intro/Redump | Fixed (/Emulation/roms/) | ✓ | ✓ (preferred) | gamelist.xml | ✓ |
| RetroDeck | No-Intro/Redump | Fixed (/retrodeck/roms/) | ✓ | ✓ (preferred) | gamelist.xml | ✓ |
| Batocera | No-Intro/Redump | Fixed (/userdata/roms/) | ✓ | ✓ | gamelist.xml | ✓ |
| LaunchBox | Flexible | Flexible | ✓ | ✓ | LaunchBox XML | ✓ |
| Pegasus | Flexible | Flexible | ✓ | ✓ | .metadata.txt | ✓ |

## Best Practices for Multi-Platform Support

### 1. Naming
Use **No-Intro/Redump** naming as the default. This works across all platforms.

### 2. Organization
Organize by system in dedicated folders:
```
/roms/
  /nes/
  /snes/
  /psx/
```

### 3. Multi-Disc Games
Always create M3U playlists for multi-disc games. All frontends support this.

### 4. Compression
Use CHD for disc-based games. Universally supported and saves space.

### 5. Metadata Export
Remus can export to multiple formats simultaneously:
- `gamelist.xml` for ES-DE family
- `.lpl` for RetroArch
- `.metadata.txt` for Pegasus

## Remus Export Workflow

### Quick Export
1. Organize library with Remus
2. Select target frontend(s)
3. Remus generates:
   - Organized folder structure
   - Metadata files
   - M3U playlists
   - Artwork in correct locations
4. Copy to target device/directory

### Advanced Export
- Multiple simultaneous exports
- Custom templates per frontend
- Selective game export
- Incremental updates

## Platform-Specific Notes

### Steam Deck (EmuDeck/RetroDeck)
- Install EmuDeck/RetroDeck first
- Use Remus on desktop to organize
- Copy organized library to Steam Deck via:
  - USB drive
  - Network transfer
  - Direct microSD card access
- Run Steam ROM Manager (EmuDeck) to add to Steam

### Raspberry Pi (Batocera/Recalbox)
- Organize on desktop with Remus
- Transfer to Pi via network or USB
- Scrape artwork on Pi or pre-download with Remus

### Windows (LaunchBox)
- Remus can run alongside LaunchBox
- Export directly to LaunchBox folders
- Import metadata into LaunchBox

## Future Compatibility

Remus will track emerging frontends and update export formats as needed. Community contributions for new frontend support are welcome.
