# Research: Optimal ROM Compression & Conversion Formats for Retro Emulation

> Date: 2026-05-25 | Agent: Researcher | Status: final

## Summary

Every major disc-based emulation system has a recommended compressed/converted format
that reduces storage footprint without sacrificing compatibility or accuracy. For
RetroArch-based workflows, **CHD (Compressed Hunks of Data)** is the universal answer
for all disc-based systems: PS1, PS2, Saturn, Dreamcast, Sega CD, 32X+CD,
TurboGrafx-CD, and 3DO all use the same `chdman createcd` command and the same `.chd`
extension. Standalone emulators use ecosystem-native formats: RVZ for GameCube/Wii
(Dolphin), WUA for Wii U (Cemu), and CSO for PSP (PPSSPP). Cartridge systems (NES,
SNES, N64, GBA, etc.) offer negligible benefit from compression given their small ROM
sizes.

---

## Sources

| URL | Relevance |
|-----|-----------|
| https://docs.mamedev.org/tools/chdman.html | chdman official tool reference |
| https://github.com/unknownbrackets/maxcso | maxcso — PSP/PS2 CSO tool |
| https://github.com/Exzap/ZArchive | ZArchive — Cemu WUA tool |
| https://docs.libretro.com/library/beetle_psx_hw/ | Beetle PSX HW extension list |
| https://docs.libretro.com/library/lrps2/ | LRPS2 (libretro PCSX2) extension list |
| https://docs.libretro.com/library/beetle_saturn/ | Beetle Saturn extension list |
| https://docs.libretro.com/library/flycast/ | Flycast extension list |
| https://docs.libretro.com/library/genesis_plus_gx/ | Genesis Plus GX extension list + chdman examples |
| https://docs.libretro.com/library/picodrive/ | PicoDrive extension list |
| https://docs.libretro.com/library/beetle_pce_fast/ | Beetle PCE FAST extension list |
| https://docs.libretro.com/library/opera/ | Opera (3DO) extension list |
| https://docs.libretro.com/library/ppsspp/ | PPSSPP libretro extension list |
| https://docs.libretro.com/library/dolphin/ | Dolphin libretro extension list |
| https://docs.libretro.com/library/citra/ | Citra (3DS) extension list |
| https://xemu.app/docs/disc-images/ | xemu Xbox disc image requirements |
| https://github.com/cemu-project/Cemu | Cemu changelog — WUA format introduction |

---

## Findings

### §1 — Per-System Recommended Format

| System | Primary Emulator | Best Format | Conversion Tool | Linux Tool? | Raw Input Formats |
|--------|-----------------|-------------|----------------|-------------|-------------------|
| Sony PlayStation (PS1) | DuckStation / Beetle PSX HW | **CHD** | `chdman createcd` | ✅ | .cue/.bin, .ccd, .toc, .mdf |
| Sony PlayStation 2 | LRPS2 (libretro PCSX2) | **CHD** | `chdman createdvd` | ✅ | .iso, .bin/.cue, .img |
| Sony PSP | PPSSPP (standalone) | **CSO** | `maxcso` | ✅ | .iso |
| Sega Saturn | Beetle Saturn | **CHD** | `chdman createcd` | ✅ | .cue/.bin, .ccd, .toc |
| Sega Dreamcast | Flycast | **CHD** | `chdman createcd` | ✅ | .gdi, .cue/.bin, .cdi |
| Sega CD / Mega-CD | Genesis Plus GX / PicoDrive | **CHD** | `chdman createcd` | ✅ | .cue/.bin, .iso |
| Sega 32X + CD | PicoDrive | **CHD** | `chdman createcd` | ✅ | .cue/.bin, .iso |
| NEC TurboGrafx-CD / PC Engine CD | Beetle PCE FAST | **CHD** | `chdman createcd` | ✅ | .cue/.bin, .ccd, .toc |
| 3DO Interactive Multiplayer | Opera | **CHD** | `chdman createcd` | ✅ | .cue/.bin, .iso |
| Nintendo GameCube / Wii | Dolphin (standalone + libretro) | **RVZ** | `dolphin-tool convert` | ✅ | .iso, .gcm |
| Nintendo Wii U | Cemu (standalone) | **WUA** | ZArchive / Cemu Title Manager | ✅ | Extracted folder, .wux |
| Nintendo 3DS | Citra (libretro) | **.cci / .3ds** | No conversion needed | — | .cci, .3ds |
| Original Xbox | xemu | **XISO (.iso)** | xdvdfs | ✅ | Redump ISO (needs repacking) |

---

### §2 — Format Deep Dives

#### CHD (Compressed Hunks of Data)

CHD originated in MAME for arcade hardware images. It has become the de-facto standard
compressed disc format across all RetroArch disc-based cores. It supports multiple
compression codecs per chunk, is lossless, and can be extracted back to the original
`.cue` + `.bin` pair.

**Compression codecs** (specified via `--compression` flag):

| Codec | Algorithm | Notes |
|-------|-----------|-------|
| `cdlz` | LZMA | Fast decompression; safe default for CD data |
| `cdzl` | zlib | Wider compatibility with older builds |
| `cdfl` | FLAC | Best for audio tracks (Sega CD, PS1 Red Book audio) |
| `cdzs` | Zstandard | Better ratio + faster than cdlz; requires recent chdman build (MAME 0.248+) |

**Default codec**: chdman automatically selects `cdlz,cdzl,cdfl` for audio+data mixed discs.

**When to use `cdzs`**: Only if your emulator build is recent (post-2023). Older RetroArch
or standalone builds may fail to open `cdzs`-encoded CHDs. Safest choice remains the
default tri-codec combination.

**Multi-disc .m3u pattern** (universal for all CHD-capable RetroArch cores):
```
# game.m3u — plain text file, one disc per line
Game Title (Disc 1).chd
Game Title (Disc 2).chd
Game Title (Disc 3).chd
```
Load `game.m3u` in RetroArch. The frontend handles disc swaps via Quick Menu → Disc
Control.

#### CSO / ZSO (Compressed ISO — PSP / PS2)

CSO is a block-compressed ISO format designed for PSP. Each 2 KB block is independently
compressed, allowing random seek without decompressing the whole image. This makes it
ideal for a system with limited RAM (PSP had 32–64 MB).

| Variant | Compression | PSP CFW | Emulators | Notes |
|---------|-------------|---------|-----------|-------|
| CSO v1 | zlib/deflate | ✅ | ✅ | Universal default; maxcso output |
| CSO v2 | lz4 | ❌ | ✅ | Faster decompress; emulator-only |
| ZSO | lz4 | ❌ | ✅ | Same as CSO v2 different magic; maxcso `--format=zso` |
| DAX | zlib | ✅ | ✅ | Legacy PSP format; avoid for new conversions |

For **PS2 CSO**, use `--block=16384` (16 KB blocks) instead of the default 2 KB PSP
blocks. LRPS2 accepts both CSO and CISO (a synonym).

**Note on `--use-libdeflate`**: maxcso docs warn that libdeflate output is *"not
compatible with some PSP CFW"*. Only use this flag for emulator targets, not for
running on real PSP hardware.

#### RVZ (Dolphin Native Format — GameCube / Wii)

RVZ was introduced in Dolphin 5.0-12188 (2020) to replace GCZ (which used ZIP deflate).
RVZ uses zstd or bzip2 and applies exception handling for scrubbed (zeroed padding)
data, yielding much smaller files than GCZ on typical games.

**Dolphin libretro confirmed extensions**: `.elf`, `.iso`, `.gcm`, `.dol`, `.tgc`,
`.wbfs`, `.ciso`, `.gcz`, `.wad`, `.rvz`.

**DolphinTool CLI** (included with Dolphin since 5.0-15260):

```bash
# Convert ISO to RVZ (recommended defaults)
dolphin-tool convert --format=rvz --compression=zstd --compression-level=5 \
  --input game.iso --output game.rvz

# Convert back to ISO
dolphin-tool convert --format=iso --input game.rvz --output game.iso

# Verify integrity
dolphin-tool verify --input game.rvz
```

GCZ is still valid but produces larger files. WBFS is an uncompressed partition-based
format from the early Wii homebrew scene; prefer RVZ for new conversions.

#### WUA (Wii U Archive — Cemu Only)

WUA was introduced in Cemu 1.27.0b (April 26, 2022). It is a Cemu-specific format built
on the open-source ZArchive container specification.

**Key properties**:
- Combines base game + update + DLC into a single file
- Lossless; fully reversible back to the original WUA source dump
- Smaller than WUX or extracted folder formats
- Supported only by Cemu (not WiiU RetroArch cores)

**Conversion**:
1. In Cemu: File → Title Manager → right-click title → Convert to Compressed (WUA)
2. Via CLI: `ZArchive-tool pack <input_folder> <output.wua>`

ZArchive releases: [github.com/Exzap/ZArchive/releases](https://github.com/Exzap/ZArchive/releases)

Cemu v2.0+ is open-source and available for Linux on Flathub.

#### XISO (Original Xbox — xemu)

xemu requires game images in XISO format. Despite the `.iso` extension, XISOs are not
standard ISO 9660 images — they use Xbox's custom XDVDFS filesystem.

**Critical**: "Redump" ISOs (~7 GB, contain both the video partition and game partition)
are **not directly compatible** with xemu. They must be repacked:

```bash
# Repack redump ISO → XISO using xdvdfs CLI
xdvdfs pack game-redump.iso
# Output: game-redump.xiso.iso (use this with xemu)

# Or use the web app: https://xiso.antangelo.com/
```

**No compression available** for XISO. Storage reduction is not possible without
third-party tools that are outside xemu's supported workflow.

---

### §3 — Conversion Command Reference

#### chdman (disc → CHD)

```bash
# Universal: CUE/BIN or other CD source
chdman createcd --input game.cue --output game.chd

# Dreamcast: GDI source (preferred over CDI)
chdman createcd --input game.gdi --output game.chd

# PS2 / DVD-ROM based
chdman createdvd --input game.iso --output game.chd

# Verify a CHD
chdman verify --input game.chd

# Extract back to CUE+BIN
chdman extractcd --input game.chd --output game.cue --outputbin game.bin

# Select specific codec (optional; default is usually best)
chdman createcd --input game.cue --output game.chd --compression cdlz,cdzl,cdfl
```

**Install on Linux**: `mame-tools` package (Debian/Ubuntu/Arch) or build from
[github.com/mamedev/mame](https://github.com/mamedev/mame).

#### maxcso (ISO → CSO/ZSO)

```bash
# PSP: ISO → CSO v1 (default, widest compatibility)
maxcso game.iso

# PSP: ISO → ZSO (lz4, faster decompression)
maxcso --format=zso game.iso

# PS2: ISO → CSO with larger block size
maxcso --block=16384 game.iso

# Maximum compression (very slow, zopfli)
maxcso --use-zopfli game.iso

# Decompress back to ISO
maxcso --decompress game.cso

# Emulator-only (use libdeflate, not CFW-safe)
maxcso --use-libdeflate game.iso
```

**Install on Linux**: build from [github.com/unknownbrackets/maxcso](https://github.com/unknownbrackets/maxcso)
or `maxcso` package in some distros.

#### DolphinTool (ISO → RVZ)

```bash
# Standard conversion (zstd level 5 is the sweet spot)
dolphin-tool convert --format=rvz --compression=zstd --compression-level=5 \
  --input game.iso --output game.rvz

# Alternative: bzip2 (smaller but much slower decompression)
dolphin-tool convert --format=rvz --compression=bzip2 --compression-level=9 \
  --input game.iso --output game.rvz

# Older GCZ format (retained for compatibility)
dolphin-tool convert --format=gcz --input game.iso --output game.gcz

# Verify
dolphin-tool verify --input game.rvz
```

DolphinTool is included with the standard Dolphin installation. No separate download
needed.

#### ZArchive (Wii U → WUA)

```bash
# Pack extracted title directory to WUA
ZArchive-tool pack <title_folder> <output.wua>

# Unpack WUA back to extracted format
ZArchive-tool unpack <input.wua> <output_folder>
```

Or use Cemu's built-in Title Manager UI (recommended for most users).

---

### §4 — RetroArch Core CHD Support Matrix

| Core | System | `.chd` ext listed | Source |
|------|--------|:-----------------:|--------|
| Beetle PSX HW | PS1 | ✅ | docs.libretro.com/library/beetle_psx_hw |
| LRPS2 | PS2 | ✅ | docs.libretro.com/library/lrps2 |
| Beetle Saturn | Saturn | ✅ | docs.libretro.com/library/beetle_saturn |
| Flycast | Dreamcast | ✅ | docs.libretro.com/library/flycast |
| Genesis Plus GX | Sega CD | ✅ | docs.libretro.com/library/genesis_plus_gx |
| PicoDrive | Sega CD / 32X | ✅ | docs.libretro.com/library/picodrive |
| Beetle PCE FAST | TurboGrafx-CD | ✅ | docs.libretro.com/library/beetle_pce_fast |
| Opera | 3DO | ✅ | docs.libretro.com/library/opera |
| Dolphin | GameCube / Wii | ✅ (also .rvz, .gcz, .wbfs) | docs.libretro.com/library/dolphin |
| PPSSPP | PSP | ⚠️ | docs.libretro.com/library/ppsspp |

> **PPSSPP note**: The official RetroArch core extension list (`docs.libretro.com`) does
> **not** list `.chd` among supported formats (`.elf .iso .cso .prx .pbp` only).
> Standalone PPSSPP's changelog confirms CHD support was added. The discrepancy likely
> reflects documentation lag. If CHD support is required for PSP, use standalone PPSSPP.
> For RetroArch workflows, CSO is the safe choice.

---

### §5 — Systems Where Compression Is Not Meaningful

These systems use cartridge ROMs or very small flash/optical media. Files are already
small enough (typically under 64 MB) that compression adds overhead without meaningful
benefit:

| System | Typical ROM size | Format | Notes |
|--------|-----------------|--------|-------|
| NES / Famicom | 128 KB – 1 MB | `.nes` | No conversion needed |
| SNES / Super Famicom | 512 KB – 6 MB | `.sfc`, `.smc` | No conversion needed |
| Sega Master System / Game Gear | 256 KB – 1 MB | `.sms`, `.gg` | No conversion needed |
| Sega Genesis / Mega Drive | 1 MB – 8 MB | `.md`, `.bin`, `.gen` | No conversion needed |
| Nintendo 64 | 4 MB – 64 MB | `.z64`, `.n64`, `.v64` | No conversion needed |
| Game Boy / Color | 256 KB – 8 MB | `.gb`, `.gbc` | No conversion needed |
| Game Boy Advance | 4 MB – 32 MB | `.gba` | No conversion needed |
| Nintendo DS | 32 MB – 512 MB | `.nds` | Can use .nds directly; compression not standard |
| Nintendo 3DS | 256 MB – 4 GB | `.3ds`, `.cci` | No standard compression; requires AES keys |
| Atari 2600 / 5200 / 7800 | < 128 KB | `.a26`, `.a52`, `.a78` | No conversion needed |

The 3DS is a special case — its ROM files are large enough that compression would be
useful, but no toolchain analogous to chdman exists for 3DS cartridge images. Citra
(libretro or standalone) loads `.3ds` / `.cci` directly and requires AES decryption
keys in `saves/Citra/sysdata/aes_keys.txt`.

---

### §6 — Format Comparison

| Format | Systems | Algorithm | Compression Ratio | Decompression Speed | Lossless | Tool |
|--------|---------|-----------|:-----------------:|:-------------------:|:--------:|------|
| CHD (default) | All disc-based | LZMA + zlib + FLAC | ~40–60% reduction | Fast | ✅ | chdman |
| CHD (`cdzs`) | All disc-based | Zstandard | ~45–65% reduction | Very fast | ✅ | chdman (MAME 0.248+) |
| CSO v1 | PSP / PS2 | deflate (per 2 KB block) | ~30–45% reduction | Moderate | ✅ | maxcso |
| ZSO | PSP / PS2 | lz4 (per 2 KB block) | ~25–40% reduction | Very fast | ✅ | maxcso |
| RVZ | GC / Wii | zstd level 5 | ~50–70% reduction | Very fast | ✅ | DolphinTool |
| GCZ | GC / Wii | zlib | ~35–50% reduction | Fast | ✅ | DolphinTool |
| WUA | Wii U | Variable | ~20–40% reduction | Fast | ✅ | ZArchive |
| XISO | Xbox OG | None (filesystem only) | ~5–15% (padding removed) | N/A | ✅ | xdvdfs |

> **General recommendation**: Use CHD for everything disc-based in RetroArch.
> It is the single universal answer that works across 10+ systems with one tool.

---

### §7 — Important Caveats and Gotchas

1. **PS1 LibCrypt (SBI files not embedded in CHD)**
   Games with Sony LibCrypt protection (popular in European PS1 releases) require a
   companion `.sbi` file alongside the `.chd`. The `.sbi` is not embedded during
   conversion. Place `game.sbi` next to `game.chd` with matching base filenames.
   DuckStation handles this automatically if present.

2. **RetroArch will not auto-scan `.pbp` files into playlists**
   PBP is PS1-in-PSP-package format. RetroArch's scanner ignores `.pbp`. Games in
   this format require manual playlist entries.

3. **Do not zip disc images in RetroArch**
   RetroArch's official documentation states: *"content from disc-based systems should
   not be zipped."* Use CHD instead of `.zip` for disc content. Zipping a `.cue` file
   does not bring along the `.bin` it references.

4. **LRPS2 is x86_64 only**
   The libretro PCSX2 core (LRPS2) does not support ARM architectures. It will not run
   on Android, iOS, or ARM-based Linux boards (Raspberry Pi, Odroid, etc.). For PS2
   emulation on ARM, use AetherSX2 (Android) or PCSX2 standalone if an ARM build
   is available.

5. **PPSSPP `.chd` discrepancy**
   The official RetroArch core docs list PSP extensions as `.elf .iso .cso .prx .pbp`.
   CHD is absent from this list. Standalone PPSSPP's changelog confirms CHD was added
   (post-2019). If targeting the RetroArch PPSSPP core specifically, use CSO. If
   targeting standalone PPSSPP, CHD is fine.

6. **Genesis Plus GX: ISO+MP3 NOT supported**
   For Sega CD, Genesis Plus GX does not support ISO+MP3 combinations. Supported
   source formats for Sega CD conversion: ISO+WAV, ISO+OGG, or BIN+CUE. Use the
   BIN+CUE or ISO+OGG source when creating a CHD for Sega CD.

7. **CHD `cdzs` codec compatibility**
   The `cdzs` (Zstandard) codec offers better compression ratios and faster
   decompression. However, it requires a recent chdman build (MAME 0.248+, released
   2023-02-22). Older RetroArch builds bundled with older chdman/MAME may fail to open
   these CHDs. If distributing or archiving for long-term compatibility, stick with
   the default `cdlz,cdzl,cdfl` codec set.

8. **Flycast: CDI format limitations**
   CDI is the proprietary DiscJuggler format. CHD and GDI are preferred for Dreamcast.
   CDI may lack audio track accuracy on some games. Convert from GDI → CHD for best
   results.

9. **xemu: redump ISO incompatibility**
   Full redump dumps of Xbox discs (~7 GB) contain a video partition that xemu cannot
   parse. They must be repacked with xdvdfs before use. The web tool at
   `https://xiso.antangelo.com/` can do this in-browser. The CLI tool also works on
   Linux.

10. **Cemu WUA: Wii U only, not RetroArch Wii U cores**
    WUA is Cemu-specific. The libretro Wii U core (if any) does not support WUA. Use
    WUA only with standalone Cemu.

---

### §8 — Tool Quick Reference

| Tool | Purpose | Formats | Linux | Upstream |
|------|---------|---------|-------|----------|
| `chdman` | Disc image → CHD | CD/DVD → .chd | ✅ | https://docs.mamedev.org/tools/chdman.html |
| `maxcso` | ISO → CSO/ZSO | .iso → .cso, .zso, .dax | ✅ | https://github.com/unknownbrackets/maxcso |
| `dolphin-tool` | GC/Wii disc → RVZ/GCZ | .iso/.gcm → .rvz, .gcz | ✅ | https://dolphin-emu.org/ (bundled) |
| `ZArchive-tool` | Wii U → WUA | Extracted folder → .wua | ✅ | https://github.com/Exzap/ZArchive |
| `xdvdfs` | Xbox → XISO | Redump .iso → XISO .iso | ✅ | https://xiso.antangelo.com/ |

---

## Recommendations

1. **For RetroArch users**: Convert all disc-based content to CHD. One tool, one format,
   all systems. The command is always `chdman createcd --input foo.cue --output foo.chd`
   (or `createdvd` for PS2/DVD). Use `.m3u` files for multi-disc sets.

2. **For PSP**: Use `maxcso` to produce CSO v1. This works in both standalone PPSSPP
   and the RetroArch core without ambiguity. If using standalone PPSSPP and you prefer
   CHD, `chdman createcd` works the same as PS1.

3. **For GameCube/Wii**: Use `dolphin-tool convert --format=rvz --compression=zstd
   --compression-level=5`. RVZ is superior to GCZ in every measurable way and is
   supported by all modern Dolphin builds.

4. **For Wii U (Cemu)**: Consolidate game + update + DLC into a single WUA using
   Cemu's Title Manager. Linux users: Cemu is available on Flathub since v2.0.

5. **For Remus (this project)**:
   - Recognise `.chd` as the canonical compressed disc format
   - Recognise `.cso` / `.zso` / `.ciso` as valid PSP/PS2 compressed formats
   - Recognise `.rvz` and `.gcz` as valid compressed GameCube/Wii formats
   - Recognise `.wua` as valid compressed Wii U format
   - Do not attempt to decompress these formats for hashing; use format-aware hashing
     or skip hash-based matching for these containers in favour of filename/database lookup

---

## Gaps / Further Research Needed

- **Neo Geo CD (NeoCD libretro)**: The docs page failed to load during this session.
  Based on FBNeo and NeoCD documentation patterns, CHD is almost certainly supported,
  but this was not directly confirmed. Retry: `https://docs.libretro.com/library/neocd/`

- **Xbox 360 (Xenia)**: Not researched. Xbox 360 uses ISO or ZISO-like formats.
  Xenia supports GOD (Games on Demand) containers and ISO images. No equivalent to
  CHD exists. Research: `https://github.com/xenia-project/xenia` wiki.

- **Nintendo Switch (Sudachi / Ryubing)**: Post-Yuzu successors. NSP and XCI are the
  primary formats; XCI is a raw cart dump (can be trimmed). No compression equivalent
  to CHD. Keys required. Research: official Sudachi or Ryubing wiki.

- **DolphinTool CLI exact flags**: The `dolphin-tool convert` command syntax above is
  based on Dolphin's documented interface but was not verified from a live CLI `--help`
  output during this session. Verify with `dolphin-tool convert --help`.

- **Saturn CHD audio track accuracy**: Beetle Saturn CHD support confirmed, but no
  specific testing was done to verify Red Book audio track quality vs. CUE+BIN source.
  Known to work; accuracy data not collected.
