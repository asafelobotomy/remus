# System Awareness Review: ROM Formats and Explicit Differentiation

Date: 2026-04-20
Scope: Full review of formats Remus may encounter, with explicit disambiguation rules for ambiguous extensions and containers.

## Goals

- Enumerate all currently modeled ROM/disc formats in Remus.
- Identify ambiguous extension groups that can map to multiple systems.
- Define explicit detection rules that differentiate systems sharing the same container (for example PS1 vs PS2 using `.iso`).
- Map current detector coverage vs recommended improvements.

## Source Inputs

### Internal codebase

- src/core/systems.cpp
- src/core/system_detector.cpp
- src/core/disc_magic_detector.cpp

### External technical references used

- MAME CHD format and metadata tags: [chdman docs](https://docs.mamedev.org/tools/chdman.html)
- PlayStation SYSTEM.CNF and boot naming: [PSX-SPX SYSTEM.CNF](https://psx-spx.consoledev.net/cdromfileformats/#systemcnf-and-booting)
- PS2 SYSTEM.CNF boot conventions and BOOT2 field: [PSDevWiki SYSTEM.CNF](https://psdevwiki.com/ps2/index.php?title=SYSTEM.CNF)
- PSP dump/format guidance (ISO/CSO/CHD/PBP): [PPSSPP dumping guide](https://www.ppsspp.org/docs/getting-started/dumping-games/)
- 3DS NCSD (CCI) structure and `NCSD` magic: [3dbrew NCSD](https://www.3dbrew.org/wiki/NCSD)
- 3DS CIA structure: [3dbrew CIA](https://www.3dbrew.org/wiki/CIA)
- Switch XCI structure (`HEAD` at 0x100, `HFS0`): [switchbrew XCI](https://switchbrew.org/wiki/XCI)
- Dreamcast IP.BIN header fields (`SEGA SEGAKATANA`): [dreamcast.wiki IP.BIN](https://dreamcast.wiki/IP.BIN)
- Dolphin RVZ/WIA format rationale and behavior: [Dolphin progress report (May/June 2020)](https://dolphin-emu.org/blog/2020/07/05/dolphin-progress-report-may-and-june-2020/)

## Full Format Inventory From Remus Registry

The list below is the current modeled format surface from `SYSTEMS` and `EXTENSION_TO_SYSTEMS`.

### Cartridge and flat ROM formats

- Nintendo: `.nes`, `.nez`, `.unf`, `.unif`, `.fds`, `.sfc`, `.smc`, `.n64`, `.z64`, `.v64`, `.ndd`, `.gb`, `.gbc`, `.gba`, `.nds`, `.dsi`, `.ids`, `.vb`, `.3ds`, `.3dz`, `.cia`, `.cci`, `.cxi`
- Sega: `.sms`, `.gg`, `.md`, `.gen`, `.smd`, `.32x`, `.68k`
- Atari/SNK/Bandai and others: `.a26`, `.a78`, `.lnx`, `.lyx`, `.j64`, `.jag`, `.neo`, `.ngp`, `.ngc`, `.ws`, `.wsc`, `.sgx`
- Computers: `.d64`, `.d71`, `.d81`, `.t64`, `.prg`, `.p00`, `.crt`, `.g64`, `.adf`, `.adz`, `.dms`, `.ipf`, `.hdf`, `.z80`, `.sna`, `.szx`, `.tzx`, `.pzx`, `.trd`, `.scl`

### Disc/container/image formats

- Optical and descriptors: `.iso`, `.cue`, `.bin`, `.img`, `.chd`, `.cdi`, `.gdi`, `.mdf`, `.mds`, `.ccd`, `.sub`, `.nrg`, `.ecm`, `.m3u`
- Nintendo disc families: `.gcm`, `.gcz`, `.rvz`, `.wbfs`, `.wad`, `.dol`
- Sony and portable variants: `.cso`, `.pbp`, `.elf`, `.isz`
- Microsoft: `.xiso`, `.xbe`, `.xex`
- Switch: `.nsp`, `.xci`, `.nsz`, `.xcz`
- Arcade: `.zip`

### High-risk ambiguity groups in current mapping

- `.iso`: PS1, PS2, GameCube, Wii, PSP, Saturn, Sega CD, Dreamcast, Xbox, Xbox 360, 3DO, Neo Geo CD
- `.bin`: PS1, PS2, Saturn, Sega CD, TurboGrafx-CD, Dreamcast, Genesis, Atari 2600, 3DO, Neo Geo CD
- `.cue`: PS1, PS2, Saturn, Sega CD, TurboGrafx-CD, Dreamcast, 3DO, Neo Geo CD
- `.chd`: PS1, PS2, Saturn, Sega CD, TurboGrafx-CD, Dreamcast, PSP, 3DO, Neo Geo CD
- `.img`: PS1, PS2, Saturn
- `.m3u`: PS1, PS2, Saturn, Sega CD, Dreamcast
- `.cso`: PSP, PS2, GameCube, Wii
- `.app`: NDS, 3DS
- `.tap`: C64, ZX Spectrum
- `.dsk`: ZX Spectrum, Amiga
- `.elf`: PS2, GameCube, Wii
- `.gcz`: GameCube, Wii
- `.rvz`: GameCube, Wii
- `.pbp`: PS1, PSP

## Current Detector Coverage

### Implemented today

- Extension candidate lookup with ordered candidate list.
- Disc magic checks for:
  - GameCube (`0xC2339F3D` at `0x1C`)
  - Wii (`0x5D1C9EA3` at `0x18`, WBFS offset, RVZ/WIA offset)
  - Dreamcast (`SEGA SEGAKATANA`)
  - Saturn (`SEGA SEGASATURN`)
  - Sega CD (`SEGADISCSYSTEM`)
  - PSP (`PSP GAME` in ISO9660 system identifier)
  - PS2 (`PLAYSTATION` markers with PS1 size fallback)
  - PS1 (`Sony Computer` marker)
- Path-token fallback heuristics for PS1/PS2/PSP/GameCube/Wii.
- Special extraction of Dreamcast serial/title from IP.BIN area.

### Gaps and risks

- Switch differentiation currently depends mostly on extension, not content probes.
- 3DS differentiation by extension is good, but container signature probe is missing (`NCSD`, CIA header structure checks).
- Xbox/Xbox360 ISO overlap is extension-only in most paths.
- Multi-track descriptor formats (`.cue` + `.bin`, `.m3u`) do not parse referenced entries to strengthen system inference.
- `.chd` relies heavily on extension; CHD metadata tags are not used (`CHCD`, `CHTR`, `CHT2`, etc.).
- `.app` (NDS vs 3DS) is unresolved when path hints are weak.

## Explicit Differentiation Rules

Use a staged classifier where each stage raises confidence.

### Stage 0: Candidate generation

- Build candidate systems from extension map.
- If singleton candidate, still run quick content sanity checks to avoid mislabeled files.

### Stage 1: Strong binary signatures (high confidence)

- GameCube: big-endian `C2 33 9F 3D` at `0x1C`.
- Wii: big-endian `5D 1C 9E A3` at known offsets (`0x18`, WBFS and RVZ/WIA offsets).
- Dreamcast: `SEGA SEGAKATANA` in IP.BIN region.
- Saturn: `SEGA SEGASATURN` at header offset.
- Sega CD: `SEGADISCSYSTEM` at header offset.
- Switch XCI: `HEAD` at `0x100` and/or `HFS0` in partition header region.
- 3DS NCSD/CCI: `NCSD` at `0x100`.
- PSP ISO: `PSP GAME` in ISO9660 system identifier region.

### Stage 2: Container-structure checks (high to medium confidence)

- PS2 vs PS1 ISO:
  - Parse ISO9660 root for `SYSTEM.CNF`.
  - If `BOOT2 = cdrom0:\...` or PS2-style ELF path, classify PS2.
  - If `BOOT = cdrom:\XXXX_NNN.NN;1`, classify PS1.
  - If only generic `PLAYSTATION` marker present, use tie-breakers below.
- PSP:
  - Presence of `PSP_GAME/` and `UMD_DATA.BIN`.
- Switch NSP/NSZ and XCI/XCZ:
  - Detect PFS0/HFS0/NCA container framing in header blocks.
  - Use compression wrapper handling first (`.nsz`, `.xcz`), then inspect inner header.
- CHD:
  - Read CHD metadata tags if available and map track metadata patterns to system families.

### Stage 3: Descriptor and companion-file parsing (medium confidence)

- `.cue`:
  - Parse referenced track files, sector modes, and filenames.
  - If referenced file names or layout match known system conventions (for example PS serial style), raise score.
- `.m3u`:
  - Parse playlist entries and recursively classify each referenced image.
  - Use plurality vote plus strongest-confidence entry.

### Stage 4: Metadata/path heuristics (medium to low confidence)

- Directory tokens and known serial patterns:
  - PS1 serial style: `SCUS_`, `SLUS_`, `SLES_`, `SLPS_` with PS1-style executable naming.
  - PS2 serial style often appears with `BOOT2` and ELF references.
  - PSP: `ULUS`, `ULES`, `NPJH`, `NPUH` patterns.
  - GameCube/Wii dump folders and tool naming (`dolphin`, `wbfs`, etc.).
- Keep heuristic-only decisions below signature/structure decisions.

### Stage 5: Confidence output and fallback behavior

- Return `{system, confidence, evidence[]}` where confidence is `high|medium|low`.
- If confidence remains low and candidate set is large, keep `unknown` for auto-match and require user/system hint before irreversible actions.

## Explicit Rules For Key Collision Cases

### PS1 vs PS2 on `.iso`

Decision order:

1. If GameCube/Wii signatures exist, exclude PS systems.
2. Parse ISO filesystem for `SYSTEM.CNF`.
3. `BOOT2` implies PS2.
4. `BOOT` with `XXXX_NNN.NN` executable style implies PS1.
5. If only `PLAYSTATION` string appears, use file size and serial/path tie-breakers.
6. If still ambiguous, output both with medium/low confidence rather than forcing first candidate.

### PSP vs PS1 on `.pbp`

Decision order:

1. Parse PBP table and sections.
2. If PSAR/PSP-specific structure and `PSP_GAME` context exist, classify PSP.
3. If EBOOT content points to PS1 packaging, classify PS1.
4. If undecidable, return ambiguous result with evidence.

### GameCube vs Wii on `.rvz` / `.gcz` / `.iso`

- Prefer direct disc magic check before extension/path.
- For RVZ/WIA, probe known wrapped-offset locations for Wii magic.
- Keep extension fallback only as final tie-breaker.

### Switch `.xci` vs `.nsp`

- `.xci`: expect gamecard header with `HEAD` and HFS0 partition model.
- `.nsp`: expect package FS container framing; do not infer only by extension for renamed files.

## Recommended Implementation Changes

1. Introduce a `DetectionEvidence` model with weighted evidence entries.
2. Add container parsers:
   - Minimal ISO9660 directory probe for `SYSTEM.CNF`, `PSP_GAME`, and boot files.
   - Minimal CUE and M3U parser to inspect referenced tracks/images.
   - Minimal NCSD/CIA and XCI/NSP header probes.
3. Extend CHD handling to inspect metadata tags, not just extension.
4. Add a strict "ambiguous" return path for low-confidence outcomes.
5. Log classifier evidence to run logs for postmortem debugging.
6. Add regression fixtures for every ambiguous extension group.

## Test Matrix To Add

- `.iso`: PS1, PS2, PSP, GameCube, Wii, Saturn, Dreamcast, Xbox sample set.
- `.bin+.cue`: PS1, Saturn, Sega CD, TurboGrafx-CD, Neo Geo CD.
- `.chd`: PS1, PS2, Saturn, Dreamcast, PSP.
- `.pbp`: PSP EBOOT vs PS1 EBOOT samples.
- `.app`: NDS and 3DS sample packs.
- `.rvz/.gcz`: both GameCube and Wii real dumps.
- `.xci/.nsp/.nsz/.xcz`: Switch container variants.

## Priority Order

- P0: PS1 vs PS2 `.iso` explicit differentiation via SYSTEM.CNF parsing.
- P0: Strengthen `.iso` classifier to avoid wrong early fallback across Nintendo/Sony/Microsoft optical sets.
- P1: CUE/M3U recursive classification.
- P1: Switch and 3DS binary signature probes.
- P2: CHD metadata-based system differentiation.

## Final Assessment

The current system-awareness implementation is a strong baseline for common cases and already includes important magic checks for multiple disc systems. The main risk remains broad extension collisions where fallback order can still force an incorrect system when content probes are absent or inconclusive.

The highest-value improvement is a weighted evidence classifier with explicit ISO filesystem parsing (especially `SYSTEM.CNF`) and recursive descriptor parsing (`.cue`, `.m3u`). That directly addresses the user-critical case of PS1 vs PS2 using the same `.iso` extension and generalizes to other multi-system optical collisions.
