# CHD Conversion & Compression Guide

## What is CHD?

**CHD (Compressed Hunks of Data)** is a lossless compression format originally developed by the MAME team for storing arcade machine hard drives and CD-ROMs. It has become the standard for compressing disc-based game images in the retro gaming community.

### Benefits of CHD Format

- **Lossless compression** - No quality loss, bit-perfect preservation
- **Space savings** - 30-60% smaller than BIN/CUE or ISO
- **Single file** - Replaces BIN+CUE pairs with one .chd file
- **Faster loading** - Compressed data can load faster on modern hardware
- **Metadata support** - Stores disc metadata internally
- **Wide compatibility** - Supported by RetroArch, MAME, many standalone emulators

### Compression Comparison

**PlayStation 1 Example:**
```
Original:     Final Fantasy VII (Disc 1).bin = 700 MB
              Final Fantasy VII (Disc 1).cue = 1 KB
Compressed:   Final Fantasy VII (Disc 1).chd = 350-450 MB
Savings:      ~40-50%
```

**Dreamcast Example:**
```
Original:     Crazy Taxi.gdi + .bin files = 1.2 GB
Compressed:   Crazy Taxi.chd = 600-800 MB
Savings:      ~33-50%
```

## Supported Formats for Conversion

### Input Formats
- **BIN/CUE** - PlayStation, Sega CD, Saturn, PC Engine CD
- **ISO** - PlayStation, PS2, Dreamcast, GameCube
- **GDI** - Dreamcast (with multiple .bin files)
- **CCD/IMG** - CloneCD format
- **Raw CD dumps** - Various formats

### Output Format
- **CHD** - Single compressed file

## CHD Versions

- **CHD v5** - Current standard (recommended)
- **CHD v4** - Older, some emulators still use
- **CHD v3** - Legacy, avoid

Most tools default to v5. Some older emulators may require v4.

## Tools for CHD Conversion

### 1. chdman (Official MAME Tool)

**Platform:** Windows, Linux, macOS

chdman is the official command-line tool from MAME for creating and managing CHD files.

#### Installation

**Windows:**
- Download from MAME official site or pre-built packages
- Available in: https://github.com/mamedev/mame/releases
- GUI wrappers available (see below)

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install mame-tools

# Arch Linux
sudo pacman -S mame-tools

# From source
git clone https://github.com/mamedev/mame.git
cd mame
make TOOLS=1
```

**macOS:**
```bash
brew install mame
```

#### Basic Usage

**Convert BIN/CUE to CHD:**
```bash
chdman createcd -i "Game Name.cue" -o "Game Name.chd"
```

**Convert ISO to CHD:**
```bash
chdman createcd -i "Game Name.iso" -o "Game Name.chd"
```

**Convert GDI to CHD (Dreamcast):**
```bash
chdman createcd -i "Game Name.gdi" -o "Game Name.chd"
```

**Extract CHD back to BIN/CUE:**
```bash
chdman extractcd -i "Game Name.chd" -o "Game Name.cue"
```

**Verify CHD integrity:**
```bash
chdman verify -i "Game Name.chd"
```

#### Advanced Options

```bash
# Specify compression codec
chdman createcd -i "game.cue" -o "game.chd" -c lzma

# Set number of processors (faster)
chdman createcd -i "game.cue" -o "game.chd" -np 8

# Force CHD version
chdman createcd -i "game.cue" -o "game.chd" -c lzma,zlib -op 4
```

**Compression codecs:**
- `lzma` - Best compression (default for v5)
- `zlib` - Faster, less compression
- `flac` - For audio tracks
- `huff` - Huffman encoding

### 2. GUI Wrappers for chdman

#### CHDmanGUI (Windows)
Simple drag-and-drop interface for chdman.

Download: Community forums / GitHub

**Usage:**
1. Drag CUE/ISO/GDI file onto window
2. Select output folder
3. Click Convert

#### Batch CHD Creator (Windows)
Batch convert entire folders.

**Features:**
- Recursive folder scanning
- Multi-threaded conversion
- Progress tracking

#### Script-based Tools

**Cue/GDI to CHD Batch Script (Windows):**
```batch
@echo off
for %%f in (*.cue) do (
    chdman createcd -i "%%f" -o "%%~nf.chd"
)
```

**Bash Script (Linux/macOS):**
```bash
#!/bin/bash
for file in *.cue; do
    chdman createcd -i "$file" -o "${file%.cue}.chd"
done
```

### 3. Integrated Tools

#### RomM (ROM Manager)
Modern web-based ROM manager with CHD conversion built-in.

#### Romcenter / CLRMamePro
ROM management tools with CHD support.

## Conversion Workflows

### Workflow 1: Single Game Conversion

```bash
# 1. Locate BIN/CUE files
cd /path/to/game/

# 2. Convert to CHD
chdman createcd -i "Final Fantasy VII (USA) (Disc 1).cue" -o "Final Fantasy VII (USA) (Disc 1).chd"

# 3. Verify
chdman verify -i "Final Fantasy VII (USA) (Disc 1).chd"

# 4. Delete originals (after backup!)
rm "Final Fantasy VII (USA) (Disc 1).bin"
rm "Final Fantasy VII (USA) (Disc 1).cue"
```

### Workflow 2: Batch Conversion (Entire Library)

**Linux/macOS Script:**
```bash
#!/bin/bash
# Convert all CUE files in a directory tree to CHD

find . -type f -name "*.cue" | while read cuefile; do
    dir=$(dirname "$cuefile")
    base=$(basename "$cuefile" .cue)
    
    echo "Converting: $cuefile"
    chdman createcd -i "$cuefile" -o "$dir/$base.chd"
    
    if [ $? -eq 0 ]; then
        echo "Success: $base.chd created"
        # Uncomment to delete originals after successful conversion
        # rm "$cuefile"
        # rm "$dir/$base.bin"
    else
        echo "Error converting: $cuefile"
    fi
done
```

**Windows PowerShell:**
```powershell
Get-ChildItem -Recurse -Filter *.cue | ForEach-Object {
    $cue = $_.FullName
    $chd = $_.FullName -replace '\.cue$', '.chd'
    
    Write-Host "Converting: $cue"
    & chdman createcd -i "$cue" -o "$chd"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Success: $chd"
        # Remove-Item $cue
        # Remove-Item ($cue -replace '\.cue$', '.bin')
    }
}
```

### Workflow 3: Multi-Disc Game with M3U

```bash
# 1. Convert each disc
chdman createcd -i "FF7 (Disc 1).cue" -o "Final Fantasy VII (USA) (Disc 1).chd"
chdman createcd -i "FF7 (Disc 2).cue" -o "Final Fantasy VII (USA) (Disc 2).chd"
chdman createcd -i "FF7 (Disc 3).cue" -o "Final Fantasy VII (USA) (Disc 3).chd"

# 2. Create M3U playlist
cat > "Final Fantasy VII (USA).m3u" << EOF
Final Fantasy VII (USA) (Disc 1).chd
Final Fantasy VII (USA) (Disc 2).chd
Final Fantasy VII (USA) (Disc 3).chd
EOF

# 3. Clean up originals
rm *.cue *.bin
```

## System-Specific Notes

### PlayStation (PSX)
- CUE/BIN → CHD works perfectly
- Multi-track audio games supported
- Use M3U for multi-disc games
- Compression: ~40-50% savings

### PlayStation 2 (PS2)
- ISO → CHD recommended for DVD games
- Compression: ~30-40% savings
- Some games have issues (test before deleting originals)

### Dreamcast
- GDI → CHD most common
- CDI also supported but less accurate
- Compression: ~40-50% savings

### Sega CD / Mega CD
- CUE/BIN → CHD
- BIN+CUE must be in same directory
- Multi-track audio supported

### Sega Saturn
- CUE/BIN → CHD
- Complex multi-track discs work fine
- Compression: ~30-40% savings

### PC Engine CD / TurboGrafx-CD
- CUE/BIN → CHD
- Often excellent compression (~50%+)

### GameCube
- ISO → CHD or use RVZ format (Dolphin-specific, better compression)
- CHD works but RVZ preferred for Dolphin emulator

### Wii
- ISO/WBFS → CHD or RVZ
- RVZ preferred for Dolphin

## Troubleshooting

### Error: "Can't open file"
- Ensure CUE file references correct BIN filename
- Check file paths (avoid spaces, special characters)
- Run from same directory as files

### Error: "Invalid track type"
- CUE sheet may have incorrect track definitions
- Try using `createcd` instead of `createdvd`

### CHD won't load in emulator
- Check CHD version (some emulators need v4)
- Re-convert with: `chdman createcd -i input.cue -o output.chd -c zlib`
- Verify CHD: `chdman verify -i game.chd`

### Large file won't compress much
- DVD-based games (PS2, GameCube) compress less than CD
- Pre-compressed data (videos, audio) doesn't compress well
- This is normal for some games

### M3U not working
- Ensure M3U uses correct filenames (case-sensitive on Linux)
- Use relative paths in M3U
- Check that all discs are in same directory

## Integration with Remus

Remus will support:

1. **Automatic CHD detection** - Recognize .chd files same as .bin/.cue
2. **Built-in conversion** - Convert BIN/CUE to CHD via UI
3. **Batch conversion** - Convert entire library or selected games
4. **M3U auto-generation** - Create M3U playlists for multi-disc CHD games
5. **Verification** - Check CHD integrity before/after conversion
6. **Reversible** - Option to extract CHD back to BIN/CUE if needed
7. **Space calculator** - Show potential space savings before converting

### Planned UI Workflow

```
1. Scan library → Detect BIN/CUE files
2. User selects games to convert
3. Preview space savings
4. Convert to CHD (with progress bar)
5. Auto-generate M3U for multi-disc
6. Verify CHD integrity
7. Optionally delete originals
8. Update database with new file paths
```

## Best Practices

1. **Always backup originals** before deleting after conversion
2. **Verify CHD files** after creation: `chdman verify -i game.chd`
3. **Test in emulator** before deleting originals
4. **Keep M3U files** for multi-disc games
5. **Use batch scripts** for large libraries
6. **Monitor disk space** during conversion (temporary files created)
7. **Use latest chdman** for best compression and compatibility

## Performance Tips

- **Multi-threading:** Use `-np` flag to set processor count
- **Compression codec:** LZMA (default) is slowest but best compression
- **Batch processing:** Convert multiple games in parallel with separate chdman instances
- **SSD storage:** Faster I/O significantly speeds up conversion

## File Size Reference

Typical compression results:

| System | Original | CHD | Savings |
|--------|----------|-----|---------|
| PSX (single disc) | 700 MB | 300-400 MB | 40-50% |
| PS2 (DVD) | 4.7 GB | 2.5-3.5 GB | 30-40% |
| Dreamcast | 1.2 GB | 600-800 MB | 35-50% |
| Sega CD | 600 MB | 300-400 MB | 40-50% |
| Sega Saturn | 600 MB | 350-450 MB | 35-45% |
| PC Engine CD | 600 MB | 200-300 MB | 50-60% |

## Resources

- MAME chdman docs: https://docs.mamedev.org/tools/chdman.html
- RetroArch CHD guide: https://docs.libretro.com/
- Recalbox CHD wiki: https://wiki.recalbox.com/en/tutorials/utilities/rom-conversion/chdman
- RetroPie CHD guide: https://retropie.org.uk/docs/CHD-files/
