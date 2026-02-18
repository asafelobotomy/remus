# Example Workflows and Folder Structures

## Example 1: Basic NES Collection

### Input Structure (Before)
```
/home/user/roms/
  mario.nes
  zelda.nes
  metroid (USA).nes
  Castlevania II - Simon's Quest (U) [!].nes
  Final Fantasy (USA) (Rev 1).nes
```

### After Scan
The app detects:
- System: NES (by .nes extension)
- Calculates CRC32 for each file
- Queries metadata providers

### After Match
```
File: mario.nes
  Match: Super Mario Bros. (USA)
  Confidence: 90% (filename match)
  Metadata: 
    - Title: Super Mario Bros.
    - Region: USA
    - Publisher: Nintendo
    - Year: 1985
    - Genre: Platform

File: zelda.nes
  Match: The Legend of Zelda (USA)
  Confidence: 90% (filename match)
  
File: metroid (USA).nes
  Match: Metroid (USA)
  Confidence: 100% (hash match)
```

### After Organize (System-based template)
```
/home/user/organized-roms/
  /NES/
    Castlevania II - Simon's Quest (USA).nes
    Final Fantasy (USA) (Rev 1).nes
    Metroid (USA).nes
    Super Mario Bros. (USA).nes
    The Legend of Zelda (USA).nes
```

## Example 2: PlayStation Multi-Disc Game

### Input Structure (Before)
```
/home/user/psx/
  ff7/
    ff7_d1.cue
    ff7_d1.bin
    ff7_d2.cue
    ff7_d2.bin
    ff7_d3.cue
    ff7_d3.bin
```

### After Scan
The app detects:
- System: PlayStation (by .cue extension)
- Groups .cue and .bin files together
- Marks .cue as primary, .bin as secondary
- Detects multi-disc game (3 discs)

### After Match
```
File: ff7_d1.cue
  Match: Final Fantasy VII (USA)
  Confidence: 100% (hash match via cue sheet)
  Disc: 1 of 3
  
File: ff7_d2.cue
  Match: Final Fantasy VII (USA)
  Disc: 2 of 3
  
File: ff7_d3.cue
  Match: Final Fantasy VII (USA)
  Disc: 3 of 3
```

### After Organize (System > Game folder template)
```
/home/user/organized-roms/
  /PlayStation/
    /Final Fantasy VII (USA)/
      Final Fantasy VII (Disc 1) (USA).cue
      Final Fantasy VII (Disc 1) (USA).bin
      Final Fantasy VII (Disc 2) (USA).cue
      Final Fantasy VII (Disc 2) (USA).bin
      Final Fantasy VII (Disc 3) (USA).cue
      Final Fantasy VII (Disc 3) (USA).bin
      boxart.jpg
      screenshot.jpg
```

## Example 3: Mixed System Library

### Input Structure (Before)
```
/home/user/RetroGames/
  nes stuff/
    mario1.nes
    mario3.nes
  Super Nintendo/
    Super Mario World.sfc
    A Link to the Past.smc
  genesis/
    sonic.bin
    sonic2.md
  /random/
    game.iso
    /psx/
      crash.cue
      crash.bin
```

### After Scan
```
Detected systems:
  - NES: 2 files
  - SNES: 2 files
  - Genesis: 2 files
  - Unknown: 1 file (game.iso - ambiguous extension)
  - PlayStation: 1 file (crash.cue + crash.bin)
```

### After Match
```
NES:
  - Super Mario Bros. (USA) [mario1.nes]
  - Super Mario Bros. 3 (USA) [mario3.nes]

SNES:
  - Super Mario World (USA) [Super Mario World.sfc]
  - The Legend of Zelda: A Link to the Past (USA) [A Link to the Past.smc]

Genesis:
  - Sonic the Hedgehog (USA) [sonic.bin]
  - Sonic the Hedgehog 2 (USA) [sonic2.md]

PlayStation:
  - Crash Bandicoot (USA) [crash.cue + crash.bin]

Unknown:
  - game.iso (requires manual system selection)
```

### After Organize (System-based)
```
/home/user/organized-roms/
  /NES/
    Super Mario Bros. (USA).nes
    Super Mario Bros. 3 (USA).nes
  /SNES/
    Super Mario World (USA).sfc
    The Legend of Zelda - A Link to the Past (USA).sfc
  /Genesis/
    Sonic the Hedgehog (USA).md
    Sonic the Hedgehog 2 (USA).md
  /PlayStation/
    /Crash Bandicoot (USA)/
      Crash Bandicoot (USA).cue
      Crash Bandicoot (USA).bin
  /Unknown/
    game.iso
```

## Example 4: Region Variants

### Input Structure (Before)
```
/home/user/snes/
  Super Mario World (USA).sfc
  Super Mario World (Europe).sfc
  Super Mario World (Japan).sfc
  スーパーマリオワールド.sfc
```

### After Scan & Match
```
File: Super Mario World (USA).sfc
  Match: Super Mario World (USA)
  Region: USA
  
File: Super Mario World (Europe).sfc
  Match: Super Mario World (Europe)
  Region: EUR
  
File: Super Mario World (Japan).sfc
  Match: Super Mario World (Japan)
  Region: JPN
  
File: スーパーマリオワールド.sfc
  Match: Super Mario World (Japan)
  Region: JPN
  Confidence: 95% (fuzzy match)
```

### After Organize
```
/home/user/organized-roms/
  /SNES/
    Super Mario World (EUR).sfc
    Super Mario World (JPN).sfc
    Super Mario World (USA).sfc
```

Note: The duplicate Japanese version is detected and user is prompted to keep one.

## Example 5: Hacks and Homebrew

### Input Structure (Before)
```
/home/user/hacks/
  Super Mario Bros (Kaizo Hack).nes
  Super Mario World (Lunar Magic) [Hack].smc
  Original Game by Homebrew Dev.gb
```

### After Scan & Match
```
File: Super Mario Bros (Kaizo Hack).nes
  Match: Super Mario Bros. (USA) [Hack]
  Confidence: 85% (fuzzy match, hack detected)
  Tags: [Hack]
  
File: Super Mario World (Lunar Magic) [Hack].smc
  Match: Super Mario World (USA) [Hack]
  Confidence: 90%
  Tags: [Hack]
  
File: Original Game by Homebrew Dev.gb
  Match: None
  Note: Homebrew detected, no database entry
```

### After Organize (with hack preservation)
```
/home/user/organized-roms/
  /NES/
    /Hacks/
      Super Mario Bros. (USA) [Kaizo Hack].nes
  /SNES/
    /Hacks/
      Super Mario World (USA) [Lunar Magic].smc
  /Game Boy/
    /Homebrew/
      Original Game by Homebrew Dev.gb
```

## Example 6: Dry-run Preview

### Before Organize (Preview Mode)
```
Planned operations:
══════════════════════════════════════════════════════════════

RENAME & MOVE:
  /home/user/roms/mario.nes
  → /home/user/organized/NES/Super Mario Bros. (USA).nes

RENAME & MOVE:
  /home/user/roms/zelda.nes
  → /home/user/organized/NES/The Legend of Zelda (USA).nes

MOVE (no rename needed):
  /home/user/roms/Metroid (USA).nes
  → /home/user/organized/NES/Metroid (USA).nes

COLLISION DETECTED:
  /home/user/roms/mario_copy.nes
  → /home/user/organized/NES/Super Mario Bros. (USA).nes
  ⚠ Target file already exists!
  Resolution: Rename to "Super Mario Bros. (USA) (1).nes"

SKIP (no match):
  /home/user/roms/unknown_game.nes
  Reason: No metadata match found

══════════════════════════════════════════════════════════════
Summary:
  - 3 files to rename and move
  - 1 file to move
  - 1 collision to resolve
  - 1 file skipped
  
Total: 6 files processed

Apply changes? [Yes/No/Review]
```

## Example 7: Metadata Display

### Game Detail View
```
═══════════════════════════════════════════════════════════════
Title: The Legend of Zelda: A Link to the Past
System: Super Nintendo Entertainment System (SNES)
Region: USA
═══════════════════════════════════════════════════════════════

Release Date: April 13, 1992
Publisher: Nintendo
Developer: Nintendo EAD
Genre: Action, Adventure, RPG
Players: 1

Description:
A young boy named Link must traverse the Light World and Dark 
World to save Hyrule from the evil wizard Agahnim and rescue 
the imprisoned descendants of the Seven Sages.

Rating: 9.2/10

Files:
  ✓ The Legend of Zelda - A Link to the Past (USA).sfc
    Size: 1.0 MB
    CRC32: A7BF56E9
    Location: /home/user/organized/SNES/

Artwork:
  [Boxart]     ✓ Downloaded
  [Screenshot] ✓ Downloaded
  [Banner]     ✓ Downloaded
  [Logo]       ✗ Not available

Metadata Source: ScreenScraper
Last Updated: 2026-02-05 14:32:01
═══════════════════════════════════════════════════════════════
```

## Example 8: Undo Operation

### After Organizing Files
```
Operation #142 completed at 2026-02-05 14:45:12
  - Moved 47 files
  - Renamed 35 files
  - Downloaded 94 artwork files
```

### User realizes mistake, triggers undo
```
Reverting operation #142...

MOVE:
  /home/user/organized/NES/Super Mario Bros. (USA).nes
  → /home/user/roms/mario.nes ✓

MOVE:
  /home/user/organized/NES/The Legend of Zelda (USA).nes
  → /home/user/roms/zelda.nes ✓

... (45 more files)

Operation #142 reverted successfully.
All files restored to original locations.
Metadata preserved in database.

Note: Downloaded artwork has been kept in cache.
```
