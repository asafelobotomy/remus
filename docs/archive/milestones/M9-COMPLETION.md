# M9 Completion Report: Verification & Patching

**Date**: 2025-02-05  
**Version**: 0.9.0  
**Status**: ✅ Complete

## Overview

M9 delivers ROM verification against No-Intro/Redump DAT files and ROM patching support. This milestone enables users to validate their ROM library against official checksums and apply translation patches, ROM hacks, and bug fixes.

## Features Implemented

### 1. DAT File Parser (`src/core/dat_parser.h/.cpp`)
- Parse No-Intro and Redump XML DAT files (Logiqx DTD format)
- Extract ROM entries with game names, filenames, sizes, and hashes
- Support for CRC32, MD5, and SHA1 hash extraction
- Parse header metadata (name, version, author, category)
- Auto-detect DAT source based on header content
- Index entries by hash for O(1) verification lookups

### 2. Header Detector (`src/core/header_detector.h/.cpp`)
Detect and strip ROM headers that must be removed for accurate hash comparison:

| System | Header Size | Detection Method |
|--------|-------------|------------------|
| NES (iNES) | 16 bytes | "NES\x1A" magic |
| NES 2.0 | 16 bytes | Byte 7 bits 2-3 |
| Atari Lynx | 64 bytes | "LYNX" magic |
| SNES (SMC) | 512 bytes | File size check |
| FDS | 16 bytes | "FDS\x1A" magic |
| Atari 7800 | 128 bytes | "ATARI7800" magic |

### 3. Verification Engine (`src/core/verification_engine.h/.cpp`)
- Import DAT files per system into database
- New database schema:
  - `verification_dats`: Imported DAT file metadata
  - `dat_entries`: Individual ROM entries from DATs
  - `verification_results`: Verification run results
- Verification status enum:
  - `Verified`: Hash matches DAT entry
  - `Mismatch`: Hash doesn't match
  - `NotInDat`: ROM not found in DAT
  - `HashMissing`: ROM hasn't been hashed yet
  - `Corrupt`: File is damaged/unreadable
- System-appropriate hash selection (CRC32 for cartridges, SHA1/MD5 for discs)
- Track missing games (in DAT but not in library)
- Export verification reports to CSV or JSON

### 4. Patch Engine (`src/core/patch_engine.h/.cpp`)
Supports major ROM patch formats:

| Format | Tool | Status |
|--------|------|--------|
| IPS | Built-in | ✅ Native implementation |
| BPS | Flips | ✅ External tool |
| UPS | Flips | ✅ External tool |
| XDelta3 | xdelta3 | ✅ External tool |
| PPF | - | ⏳ Not yet implemented |

Features:
- Magic byte detection for format identification
- Built-in IPS implementation (no external tools required)
- External tool discovery (`flips`, `xdelta3`)
- Configurable tool paths
- Patch creation support (BPS, IPS, XDelta3)
- Auto-generate output filenames: `BaseROM [PatchName].ext`

### 5. UI Controllers

**VerificationController** (`src/ui/controllers/verification_controller.h/.cpp`):
- `importDatFile(path, system)`: Import DAT file
- `verifyAll()` / `verifySystem(system)`: Run verification
- `getMissingGames(system)`: Get games in DAT but not in library
- `exportResults(path, format)`: Export verification report
- Progress tracking with current file and completion percentage
- Summary statistics: total, verified, not in DAT, no hash, corrupt

**PatchController** (`src/ui/controllers/patch_controller.h/.cpp`):
- `detectPatchFormat(path)`: Detect and validate patch file
- `applyPatch(rom, patch, output)`: Apply patch with progress
- `createPatch(original, modified, output, format)`: Create new patch
- `checkTools()`: Refresh tool availability status
- `getSupportedFormats()`: List available patch formats
- Recent patches history tracking

### 6. QML Views

**VerificationView.qml**:
- DAT file import with system dropdown
- List of imported DATs with version and source info
- Verification progress bar with current file
- Summary cards showing verification statistics
- Filterable results table (All, Verified, Not in DAT, No Hash)
- Search functionality within results
- Color-coded status indicators

**PatchView.qml**:
- Tool availability status cards
- Base ROM file browser
- Patch file browser with format detection
- Patch info display (format, size, validity)
- Output path configuration
- Apply patch button with progress
- Recent patches history list

## Files Created

### Core Library (4 files)
- `src/core/dat_parser.h` (102 lines)
- `src/core/dat_parser.cpp` (211 lines)
- `src/core/header_detector.h` (91 lines)
- `src/core/header_detector.cpp` (298 lines)
- `src/core/verification_engine.h` (144 lines)
- `src/core/verification_engine.cpp` (424 lines)
- `src/core/patch_engine.h` (136 lines)
- `src/core/patch_engine.cpp` (315 lines)

### UI Controllers (2 files)
- `src/ui/controllers/verification_controller.h` (89 lines)
- `src/ui/controllers/verification_controller.cpp` (261 lines)
- `src/ui/controllers/patch_controller.h` (78 lines)
- `src/ui/controllers/patch_controller.cpp` (225 lines)

### QML Views (2 files)
- `src/ui/qml/VerificationView.qml` (340 lines)
- `src/ui/qml/PatchView.qml` (320 lines)

**Total New Code**: ~2,534 lines

## Files Modified

- `src/core/CMakeLists.txt`: Added 4 new source files
- `src/ui/CMakeLists.txt`: Added 2 new controller files
- `src/ui/main.cpp`: Added controller includes and registration
- `src/ui/qml/MainWindow.qml`: Added Verification and Patching navigation
- `src/ui/resources.qrc`: Added new QML files
- `src/core/constants/constants.h`: Updated version to 0.9.0, milestone to M9
- `VERSION`: Updated to 0.9.0
- `CHANGELOG.md`: Added M9 release notes

## Database Schema Additions

```sql
-- Imported DAT file tracking
CREATE TABLE verification_dats (
    id INTEGER PRIMARY KEY,
    system_name TEXT NOT NULL UNIQUE,
    dat_name TEXT NOT NULL,
    dat_version TEXT,
    dat_source TEXT,
    dat_description TEXT,
    entry_count INTEGER DEFAULT 0,
    imported_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Individual ROM entries from DAT files
CREATE TABLE dat_entries (
    id INTEGER PRIMARY KEY,
    dat_id INTEGER NOT NULL,
    game_name TEXT NOT NULL,
    rom_name TEXT NOT NULL,
    rom_size INTEGER,
    crc32 TEXT,
    md5 TEXT,
    sha1 TEXT,
    description TEXT,
    status TEXT,
    FOREIGN KEY (dat_id) REFERENCES verification_dats(id)
);

-- Verification results per file
CREATE TABLE verification_results (
    id INTEGER PRIMARY KEY,
    file_id INTEGER NOT NULL,
    dat_id INTEGER,
    status TEXT NOT NULL,
    matched_entry_id INTEGER,
    hash_type TEXT,
    file_hash TEXT,
    dat_hash TEXT,
    header_stripped BOOLEAN DEFAULT 0,
    verified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    notes TEXT,
    FOREIGN KEY (file_id) REFERENCES files(id)
);
```

## Dependencies

### Required
- Qt 6 (Core, Gui, Quick, Qml, Sql)

### Optional (for extended patch format support)
- `flips` (Floating IPS) - BPS/UPS patches
  - Install: `yay -S flips` or build from https://github.com/Alcaro/Flips
- `xdelta3` - XDelta patches
  - Install: `sudo apt install xdelta3` or `brew install xdelta`

## Testing

```bash
# Build
cd build && make -j$(nproc)

# Run tests
ctest --output-on-failure
# Result: 1/1 tests passed

# Manual testing
./remus-gui
# Navigate to Verification → Import a No-Intro DAT
# Navigate to Patching → Apply an IPS patch
```

## Usage Examples

### Verification Workflow

1. Download DAT files from https://datomatic.no-intro.org/
2. Launch Remus GUI → Verification
3. Select system from dropdown (e.g., "NES")
4. Click "Import DAT" and select downloaded .dat file
5. Click "Verify All"
6. Review results with color-coded status
7. Export report to CSV for records

### Patching Workflow

1. Download translation patch from romhacking.net
2. Launch Remus GUI → Patching
3. Click "Browse..." for Base ROM and select original ROM
4. Click "Browse..." for Patch File and select .ips/.bps/.xdelta
5. Review detected format and output path
6. Click "Apply Patch"
7. Patched ROM saved with `[patch name]` suffix

## Architecture

```
src/core/
├── dat_parser.h/.cpp        # XML DAT parsing (Logiqx format)
├── header_detector.h/.cpp   # ROM header detection and stripping
├── verification_engine.h/.cpp   # DAT import, verification logic
└── patch_engine.h/.cpp      # Patch format detection and application

src/ui/
├── controllers/
│   ├── verification_controller.h/.cpp  # QML bindings for verification
│   └── patch_controller.h/.cpp         # QML bindings for patching
└── qml/
    ├── VerificationView.qml  # Verification UI
    └── PatchView.qml         # Patching UI
```

## Future Enhancements (Not in M9)

- **romhacking.net Integration**: Web scraping for patch discovery
- **Patch Tracking**: Store applied patches in database
- **Multi-track Verification**: Per-track hashing for Redump CDs
- **PPF Format**: PlayStation Patch Format support
- **Soft Patching**: Runtime patching without modifying files

## Milestone Status

| Milestone | Status | Version |
|-----------|--------|---------|
| M0: Product Definition | ✅ Complete | 0.0.0 |
| M1: Core Scanning | ✅ Complete | 0.1.0 |
| M2: Metadata Layer | ✅ Complete | 0.2.0 |
| M3: Matching & Confidence | ✅ Complete | 0.3.0 |
| M4: Organize & Rename | ✅ Complete | 0.4.0 |
| M4.5: File Conversion | ✅ Complete | 0.4.5 |
| M5: UI MVP | ✅ Complete | 0.5.0 |
| M6: Constants Library | ✅ Complete | 0.6.0 |
| M7: Packaging & CI/CD | ✅ Complete | 0.7.0 |
| M8: Polish | ✅ Complete | 0.8.0 |
| **M9: Verification & Patching** | ✅ **Complete** | **0.9.0** |

## Conclusion

M9 completes the Remus development roadmap. The application now provides a comprehensive ROM library management solution with:

- **Scanning**: Recursive file discovery with system detection
- **Hashing**: Multi-algorithm hash calculation
- **Matching**: Three-tier matching with confidence scoring
- **Metadata**: 4 provider integration with caching
- **Organizing**: Template-based renaming with undo
- **Conversion**: CHD compression and archive extraction
- **Verification**: No-Intro/Redump DAT validation
- **Patching**: IPS/BPS/UPS/XDelta3 support
- **Export**: RetroArch, EmulationStation-DE, CSV, JSON
- **UI**: Full Qt 6/QML desktop application
- **Packaging**: AppImage with CI/CD automation
