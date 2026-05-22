# Choose A Canonical Output By System

Use this matrix when you decide which disc or container format Remus should emit after normalization.

This document focuses on systems that have mature conversion or compression formats already present in Remus, already modeled in the library metadata, or backed by stable external tools. Cartridge-only systems are out of scope because formats like `.zip` or `.7z` are transport containers, not canonical runtime formats.

Compression amounts are planning estimates, not guarantees. They compare the canonical output against a clean source dump such as BIN/CUE, GDI, or ISO. Real savings vary with audio, video, garbage data, and update partitions.

Companion documents:

- [plans/archive-format-backlog.md](plans/archive-format-backlog.md) turns this matrix into a delivery backlog.
- [adr/adr-0002-use-system-specific-canonical-archive-formats.md](adr/adr-0002-use-system-specific-canonical-archive-formats.md) locks the archive policy.

## Use The System Matrix

| System | Canonical output for Remus | Accepted library inputs | Conversion-ready inputs today | Tool backend | Typical compression amount |
| -------- | ----------------------------- | ------------------------- | ------------------------------- | -------------- | ---------------------------- |
| PlayStation (PS1) | CHD | `.cue`, `.bin`, `.iso`, `.img`, `.pbp`, `.chd`, `.mdf`, `.mds`, `.ecm`, `.ccd`, `.sub`, `.m3u` | `.cue`, `.iso`, `.img` | `chdman` | About `40-50%` savings |
| Sega CD / Mega CD | CHD | `.cue`, `.bin`, `.iso`, `.chd` | `.cue`, `.iso` | `chdman` | About `40-50%` savings |
| Sega Saturn | CHD | `.cue`, `.bin`, `.iso`, `.chd` | `.cue`, `.iso` | `chdman` | About `35-45%` savings |
| TurboGrafx-CD / PC Engine CD | CHD | `.cue`, `.bin`, `.chd` | `.cue` | `chdman` | About `50-60%` savings |
| Dreamcast | CHD | `.cdi`, `.gdi`, `.chd`, `.bin`, `.cue`, `.iso`, `.dat`, `.lst` | `.gdi`, `.cue`, `.iso` | `chdman` | About `35-50%` savings |
| PlayStation 2 | CHD | `.iso`, `.chd`, `.cso`, `.gz`, `.elf`, `.isz`, `.bin`, `.img`, `.nrg` | `.iso`, `.img` for CHD; `.iso` for CSO | `chdman` for CHD, `maxcso` for optional CSO | About `30-40%` savings for CHD |
| GameCube | RVZ | `.iso`, `.gcm`, `.gcz`, `.rvz`, `.cso`, `.dol` | `.iso`, `.gcm` | `dolphin-tool` | Highly variable. Use `25-60%` for planning. Remus validation on Wind Waker saved `39.6%`. |
| Wii | RVZ | `.iso`, `.wbfs`, `.rvz`, `.gcz`, `.cso`, `.wad`, `.dol` | `.iso` | `dolphin-tool` | Highly variable. Use `20-70%` for planning. Update-heavy or garbage-heavy discs can shrink more. |
| PSP | CSO | `.iso`, `.cso`, `.pbp`, `.chd` | `.iso` to CSO, `.cso` to ISO | `maxcso` | No stable system-wide ratio. Expect modest-to-medium savings and measure each title before treating CSO as canonical. |

## Keep Legacy Formats In A Secondary Role

The matrix above names the recommended canonical output per system. The formats below still matter, but they fit better as import-only, normalization-only, or export-only paths.

| Format | Relevant systems | Recommended role in Remus | Backend | Compression signal |
| -------- | ------------------ | --------------------------- | --------- | -------------------- |
| CHD | PS1, Sega CD, Saturn, TurboGrafx-CD, Dreamcast, PS2 | Canonical archival output | `chdman` | Usually `30-60%`, depending on system and media layout |
| RVZ | GameCube, Wii | Canonical archival output for Dolphin-class workflows | `dolphin-tool` | Very title-dependent. Lossless and often close to or better than older scrubbed formats |
| CSO | PSP, optional for PS2 | Canonical PSP output, optional PS2 export | `maxcso` | Variable. `maxcso` does not promise a fixed ratio, so use measurement mode before committing a library-wide default |
| WBFS | Wii | Export-only for real hardware or USB loader workflows | `wit` or `wwt` in a future integration | Mostly space trimming from removed junk and partitions, not a preservation-grade codec |
| WIA | GameCube, Wii | Input normalization only | `wit` in a future integration | Wiimm documents roughly `25-50%` savings against WDF-style storage, but WIA is slower and no longer the preferred runtime format |
| GCZ | GameCube, Wii | Input normalization only | `wit` or Dolphin in a future integration | Moderate on GameCube, weak on Wii because encrypted Wii data compresses poorly |
| PBP / EBOOT.PBP | PS1 export for PSP or PS3 style use | Export-only, not archival canonical | `PSXPackager` or another popstation-family backend in a future integration | Compression level exists, but there is no stable cross-title savings number. Treat it as a convenience package, not a baseline archive format |
| NKit | GameCube, Wii | Import-only if ever added | NKit toolchain, possibly alongside `wit` | Can save a lot of collection space, but Dolphin documents runtime and compatibility drawbacks. Do not use it as a canonical output |
| CISO / WBI | Wii, GameCube in Wiimm tooling | Input normalization only | `wit` in a future integration | Space savings come from sparse chunk storage. Useful for conversion, but not worth promoting as a primary Remus output |

## Read The Compression Amounts Correctly

Use these rules when you read the numbers in the matrix:

1. CD-based systems compress more consistently than DVD-era systems.
2. RVZ savings swing hard with garbage data and Wii update partitions.
3. CSO savings depend on block size, title data, and compatibility choices.
4. PBP uses per-tool compression levels, so one headline ratio is misleading.
5. When a tool can measure output before writing it, prefer measurement over assumptions.

## Use The Matrix To Drive Implementation

This matrix implies the next implementation order for Remus:

1. Finish CSO as a first-class path for PSP and optional PS2 workflows.
2. Keep RVZ as the only canonical GameCube and Wii output.
3. Add WBFS, WIA, GCZ, and CISO as normalization inputs instead of new canonical outputs.
4. Add PBP only as a dedicated PS1 export feature.

## Reuse The Existing References

Use these repo documents with this matrix:

- [chd-conversion.md](chd-conversion.md) for CHD-specific workflow and system savings.
- [emulator-frontend-compatibility.md](emulator-frontend-compatibility.md) for frontend-facing CHD guidance.
- [archive/reports/TEST-OUTPUT-VALIDATION-SUMMARY-2026-04-05.md](archive/reports/TEST-OUTPUT-VALIDATION-SUMMARY-2026-04-05.md) for the validated RVZ conversion example used in this matrix.
- [requirements.md](requirements.md) for current scope and extension inventory.
- [plans/archive-format-backlog.md](plans/archive-format-backlog.md) for the concrete workstream backlog.
- [adr/adr-0002-use-system-specific-canonical-archive-formats.md](adr/adr-0002-use-system-specific-canonical-archive-formats.md) for the locked archive policy.

## Track The External Sources

This matrix also reflects the current external tool guidance used in the research pass:

- Dolphin Progress Report: May and June 2020 for RVZ and WIA positioning.
- Wiimm ISO Tools documentation for WBFS, WIA, GCZ, and CISO roles.
- `maxcso` documentation for CSO capabilities and compatibility limits.
- `PSXPackager` documentation for PBP export scope.
