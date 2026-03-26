# Research: Retro Gaming Frontend ROM Folder Naming Conventions

> Date: 2026-05-24 | Agent: Researcher | Status: complete

## Summary

Nine major retro-gaming frontends and ROM managers each enforce their own ROM folder naming scheme. After source-verified research across primary config files, source code, and official documentation, this document provides the exact folder name each tool expects per system. Key findings: RomM uses the most divergent naming (e.g. `dc`, `ngc`, `sms`, `sega32`, `zxs`); RetroPie uses `coleco` for ColecoVision and always prefixes Atari handhelds (`atarilynx`, `atarijaguar`); Batocera abbreviates WonderSwan to `wswan`/`wswanc` and uses `gamecube`; most others converge on short lowercase slugs. LaunchBox enforces no folder structure — the user defines it.

## Sources

| URL | Relevance |
|-----|-----------|
| https://emudeck.github.io/cheat-sheet/ | EmuDeck — authoritative folder names, emulators, extensions |
| https://gitlab.com/es-de/emulationstation-de/-/raw/master/resources/systems/linux/es_systems.xml | ES-DE — `<name>` tags = exact folder names |
| https://wiki.batocera.org/systems | Batocera — official system list |
| https://raw.githubusercontent.com/rommapp/romm/master/backend/handler/metadata/base_handler.py | RomM — `UniversalPlatformSlug` StrEnum; values = folder names |
| https://docs.romm.app/latest/Platforms-and-Players/Supported-Platforms/ | RomM — human-readable platform→folder mapping |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-genesis-plus-gx.sh | RetroPie — Sega 8/16-bit folder names |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-fbneo.sh | RetroPie — arcade, neogeo, coleco, ngp, ngpc |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-beetle-wswan.sh | RetroPie — wonderswan, wonderswancolor |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-virtualjaguar.sh | RetroPie — atarijaguar |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-beetle-lynx.sh | RetroPie — atarilynx |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-flycast.sh | RetroPie — dreamcast |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-opera.sh | RetroPie — 3do |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-bluemsx.sh | RetroPie — coleco (ColecoVision) |
| https://gamesdb.launchbox-app.com/ | LaunchBox — platform display names (folder-agnostic) |

---

## Findings

### Methodology Notes

- **RetroArch** does not enforce ROM folder names. It uses `"Manufacturer - System Name"` strings for `.lpl` playlist files and thumbnail subdirectories. ROM folders are user-defined.
- **LaunchBox/BigBox** is folder-agnostic — the user specifies a ROM path per platform. Only the *Platform Name* string is standardised.
- **Romulus** (romulus.cc) could not be accessed (JavaScript-protected redirect). Omitted.
- All other tools enforce specific folder names that are source-verified below.

### Key Divergences at a Glance

| Divergence | Systems affected |
|---|---|
| RomM uses IGDB-aligned slugs instead of common names | Dreamcast (`dc`), GameCube (`ngc`), Master System (`sms`), 32X (`sega32`), ZX Spectrum (`zxs`), TG-CD (`turbografx-cd`), NGP (`neo-geo-pocket`), NGPC (`neo-geo-pocket-color`), WonderSwan Color (`wonderswan-color`) |
| RetroPie prefixes Atari handhelds | Lynx (`atarilynx`), Jaguar (`atarijaguar`) — Batocera/RomM/EmuDeck use bare `lynx`/`jaguar` |
| RetroPie abbreviates ColecoVision | `coleco` — all others use `colecovision` |
| Batocera shortens WonderSwan | `wswan`, `wswanc` — all others use `wonderswan`, `wonderswancolor` |
| Batocera uses `gamecube` | ES-DE/RetroPie/EmuDeck use `gc` |
| SG-1000 hyphen split | ES-DE/RetroPie: `sg-1000` (hyphen) — Batocera/RomM/EmuDeck: `sg1000` (no hyphen) |
| 3DS prefix split | ES-DE: `n3ds` — Batocera/RomM/RetroPie: `3ds` |
| Mega Drive naming | ES-DE dual (`genesis`+`megadrive`) — Batocera/RetroPie: `megadrive` only — EmuDeck/RomM: `genesis` only |

---

## Comparison Table

> **Column headers**
> - **RetroArch** = playlist `.lpl` / thumbnail directory name (not a ROM folder)
> - **ES-DE** = `<name>` tag from `es_systems.xml` (dual entries shown where both exist)
> - **Batocera** = folder under `/userdata/roms/`
> - **RetroPie** = folder under `/home/pi/RetroPie/roms/`
> - **EmuDeck** = primary folder under `Emulation/roms/` (common aliases in parentheses)
> - **RomM** = `UniversalPlatformSlug` enum value = exact folder name expected
> - **LaunchBox** = Platform display name used for metadata (folder is user-defined)

### Nintendo

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| NES | `Nintendo - Nintendo Entertainment System` | `nes` | `nes` | `nes` | `nes` | `nes` | Nintendo Entertainment System |
| SNES | `Nintendo - Super Nintendo Entertainment System` | `snes` | `snes` | `snes` | `snes` | `snes` | Super Nintendo Entertainment System |
| Nintendo 64 | `Nintendo - Nintendo 64` | `n64` | `n64` | `n64` | `n64` | `n64` | Nintendo 64 |
| Game Boy | `Nintendo - Game Boy` | `gb` | `gb` | `gb` | `gb` | `gb` | Nintendo Game Boy |
| Game Boy Color | `Nintendo - Game Boy Color` | `gbc` | `gbc` | `gbc` | `gbc` | `gbc` | Nintendo Game Boy Color |
| Game Boy Advance | `Nintendo - Game Boy Advance` | `gba` | `gba` | `gba` | `gba` | `gba` | Nintendo Game Boy Advance |
| Nintendo DS | `Nintendo - DS` | `nds` | `nds` | `nds` | `nds` | `nds` | Nintendo DS |
| **GameCube** | `Nintendo - GameCube` | `gc` | **`gamecube`** ⚠️ | `gc` | `gc` | **`ngc`** ⚠️ | Nintendo GameCube |
| Wii | `Nintendo - Wii` | `wii` | `wii` | `wii` | `wii` | `wii` | Nintendo Wii |
| **Nintendo 3DS** | `Nintendo - 3DS` | **`n3ds`** ⚠️ | `3ds` | `3ds` | `3ds` (alt: `n3ds`) | `3ds` | Nintendo 3DS |
| Virtual Boy | `Nintendo - Virtual Boy` | `virtualboy` | `virtualboy` | `virtualboy` | `virtualboy` | `virtualboy` | Nintendo Virtual Boy |
| Famicom Disk System | `Nintendo - Family Computer Disk System` | `fds` | `fds` | `fds` | (in `nes`) | `fds` | Nintendo Famicom Disk System |

### Sega

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| **Genesis / Mega Drive** | `Sega - Mega Drive - Genesis` | `genesis` + `megadrive` ⚠️ | `megadrive` | `megadrive` | `genesis` (alt: `megadrive`) | `genesis` | Sega Genesis / Sega Mega Drive |
| **Master System** | `Sega - Master System - Mark III` | `mastersystem` | `mastersystem` | `mastersystem` | `mastersystem` | **`sms`** ⚠️ | Sega Master System |
| Saturn | `Sega - Saturn` | `saturn` | `saturn` | `saturn` | `saturn` | `saturn` | Sega Saturn |
| **Dreamcast** | `Sega - Dreamcast` | `dreamcast` | `dreamcast` | `dreamcast` | `dreamcast` | **`dc`** ⚠️ | Sega Dreamcast |
| Game Gear | `Sega - Game Gear` | `gamegear` | `gamegear` | `gamegear` | `gamegear` | `gamegear` | Sega Game Gear |
| Sega CD / Mega-CD | `Sega - Mega-CD - Sega CD` | `segacd` + `megacd` | `segacd` | `segacd` | `segacd` (alt: `megacd`) | `segacd` | Sega CD |
| **Sega 32X** | `Sega - 32X` | `sega32x` | `sega32x` | `sega32x` | `sega32x` | **`sega32`** ⚠️ | Sega 32X |
| **SG-1000** | `Sega - SG-1000` | `sg-1000` ⚠️ | `sg1000` | `sg-1000` ⚠️ | `sg1000` | `sg1000` | Sega SG-1000 |

> ⚠️ Note: `sg-1000` (hyphen) in ES-DE and RetroPie; `sg1000` (no hyphen) in Batocera/EmuDeck/RomM.

### Sony

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| PlayStation | `Sony - PlayStation` | `psx` | `psx` | `psx` | `psx` | `psx` | Sony PlayStation |
| PlayStation 2 | `Sony - PlayStation 2` | `ps2` | `ps2` | `ps2` | `ps2` | `ps2` | Sony PlayStation 2 |
| PSP | `Sony - PlayStation Portable` | `psp` | `psp` | `psp` | `psp` | `psp` | Sony PSP |
| PS Vita | `Sony - PlayStation Vita` | `psvita` | `psvita` | N/A | `psvita` | `psvita` | Sony PlayStation Vita |

### Atari

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| Atari 2600 | `Atari - 2600` | `atari2600` | `atari2600` | `atari2600` | `atari2600` | `atari2600` | Atari 2600 |
| Atari 7800 | `Atari - 7800` | `atari7800` | `atari7800` | `atari7800` | `atari7800` | `atari7800` | Atari 7800 |
| **Atari Lynx** | `Atari - Lynx` | `atarilynx` | **`lynx`** ⚠️ | `atarilynx` | **`lynx`** ⚠️ | **`lynx`** ⚠️ | Atari Lynx |
| **Atari Jaguar** | `Atari - Jaguar` | `atarijaguar` | **`jaguar`** ⚠️ | `atarijaguar` | `atarijaguar` | **`jaguar`** ⚠️ | Atari Jaguar |

> ⚠️ ES-DE and RetroPie include the `atari` prefix for Lynx and Jaguar; Batocera, EmuDeck (Lynx only), and RomM drop it.

### NEC

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| **TurboGrafx-16 / PC Engine** | `NEC - PC Engine - TurboGrafx 16` | `pcengine` + `tg16` | `pcengine` | `pcengine` | `tg16` (alt: `pcengine`) | `tg16` | NEC TurboGrafx-16 |
| **TurboGrafx-CD / PCE-CD** | `NEC - PC Engine CD - TurboGrafx-CD` | `pcenginecd` + `tg-cd` | `pcenginecd` | `pcenginecd` | `tg-cd` (alt: `pcenginecd`) | **`turbografx-cd`** ⚠️ | NEC TurboGrafx-CD |

### SNK Neo Geo

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| **Neo Geo AES / MVS** | `SNK - Neo Geo AES - MVS` | `neogeo` | `neogeo` | `neogeo` | `fbneo` ⚠️ | `neogeoaes` ⚠️ | SNK Neo Geo AES |
| **Neo Geo Pocket** | `SNK - Neo Geo Pocket` | `ngp` | `ngp` | `ngp` | `ngp` | **`neo-geo-pocket`** ⚠️ | SNK Neo Geo Pocket |
| **Neo Geo Pocket Color** | `SNK - Neo Geo Pocket Color` | `ngpc` | `ngpc` | `ngpc` | `ngpc` | **`neo-geo-pocket-color`** ⚠️ | SNK Neo Geo Pocket Color |

### Microsoft

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| Xbox | `Microsoft - Xbox` | `xbox` | `xbox` | N/A | `xbox` | `xbox` | Microsoft Xbox |
| Xbox 360 | `Microsoft - Xbox 360` | `xbox360` | `xbox360` | N/A | `xbox360/roms` ⚠️ | `xbox360` | Microsoft Xbox 360 |

> ⚠️ EmuDeck Xbox 360 uses a subdirectory: `xbox360/roms` (with XBLA under `xbox360/roms/xbla`).

### Other

| System | RetroArch | ES-DE | Batocera | RetroPie | EmuDeck | RomM | LaunchBox |
|--------|-----------|-------|----------|----------|---------|------|-----------|
| 3DO Interactive Multiplayer | `The 3DO Company - 3DO` | `3do` | `3do` | `3do` | `3do` | `3do` | 3DO Interactive Multiplayer |
| Commodore 64 | `Commodore - 64` | `c64` | `c64` | `c64` | `c64` | `c64` | Commodore 64 |
| **Commodore Amiga** | `Commodore - Amiga` | `amiga` + `amiga1200` | `amiga500` + `amiga1200` ⚠️ | `amiga` | `amiga` + `amiga600` + `amiga1200` | `amiga` | Commodore Amiga |
| **ZX Spectrum** | `Sinclair - ZX Spectrum` | `zxspectrum` | `zxspectrum` | `zxspectrum` | `zxspectrum` | **`zxs`** ⚠️ | Sinclair ZX Spectrum |
| **Arcade / MAME** | `MAME` | `arcade` + `mame` | `mame` (+ `fbneo`) | `arcade` + `fba` + `mame-libretro` | `arcade` + `fbneo` + `mame2003` + `mame2010` | `arcade` | Arcade |
| **WonderSwan** | `Bandai - WonderSwan` | `wonderswan` | **`wswan`** ⚠️ | `wonderswan` | `wonderswan` | `wonderswan` | Bandai WonderSwan |
| **WonderSwan Color** | `Bandai - WonderSwan Color` | `wonderswancolor` | **`wswanc`** ⚠️ | `wonderswancolor` | `wonderswancolor` | **`wonderswan-color`** ⚠️ | Bandai WonderSwan Color |
| **ColecoVision** | `Coleco - ColecoVision` | `colecovision` | `colecovision` | **`coleco`** ⚠️ | `colecovision` | `colecovision` | ColecoVision |
| Vectrex | `GCE - Vectrex` | `vectrex` | `vectrex` | `vectrex` | `vectrex` | `vectrex` | GCE Vectrex |

---

## Recommendations

### For Remus (hash-first ROM identification)

When Remus organises ROMs into tagged folders, it should resolve a canonical slug and then map it to each frontend's expected name at export time. Suggested canonical slugs (favour the majority vote across frontends):

| Remus canonical | Rationale |
|---|---|
| `nes`, `snes`, `n64`, `gb`, `gbc`, `gba`, `nds`, `wii`, `virtualboy`, `fds` | Universal agreement |
| `gc` | 4 of 5 use `gc`; Batocera outlier (`gamecube`); RomM outlier (`ngc`) |
| `n3ds` | ES-DE uses `n3ds`; others use `3ds` — prefer `n3ds` for clarity |
| `genesis` | Majority across Sega-heavy users; ES-DE/Batocera maps `megadrive` as alias |
| `mastersystem` | 4 of 5 use this; RomM is only outlier (`sms`) |
| `dreamcast` | 4 of 5; RomM-only outlier (`dc`) |
| `segacd`, `sega32x` | Wide agreement |
| `sg1000` | No-hyphen majority (Batocera/EmuDeck/RomM); ES-DE/RetroPie use `sg-1000` |
| `psx`, `ps2`, `psp`, `psvita` | Universal agreement |
| `atari2600`, `atari7800` | Universal agreement |
| `atarilynx`, `atarijaguar` | ES-DE and RetroPie use prefixed form; document mapping to `lynx`/`jaguar` |
| `pcengine`, `pcenginecd` | Slight majority; EmuDeck/RomM use `tg16`/`turbografx-cd` |
| `neogeo` | 3 of 5 use this; EmuDeck routes through `fbneo`; RomM uses `neogeoaes` |
| `ngp`, `ngpc` | 4 of 5; RomM is outlier with long-form slugs |
| `xbox`, `xbox360` | Wide agreement |
| `3do`, `c64`, `amiga` | Universal/near-universal agreement |
| `zxspectrum` | 4 of 5; RomM-only outlier (`zxs`) |
| `arcade` | Majority; multi-emulator setups add `fbneo`, `mame2003`, etc. as siblings |
| `wonderswan`, `wonderswancolor` | 4 of 5; Batocera outlier (`wswan`/`wswanc`); RomM outlier (`wonderswan-color`) |
| `colecovision` | 4 of 5; RetroPie-only outlier (`coleco`) |
| `vectrex` | Universal agreement |

### Frontend Mapping Table (for export/organisation features)

When Remus outputs to a specific frontend, apply these translations on top of the canonical slug:

| System | Remus canonical | → Batocera | → RomM | → RetroPie |
|--------|----------------|------------|--------|------------|
| GameCube | `gc` | `gamecube` | `ngc` | `gc` |
| 3DS | `n3ds` | `3ds` | `3ds` | `3ds` |
| Mega Drive | `genesis` | `megadrive` | `genesis` | `megadrive` |
| Master System | `mastersystem` | `mastersystem` | `sms` | `mastersystem` |
| Dreamcast | `dreamcast` | `dreamcast` | `dc` | `dreamcast` |
| 32X | `sega32x` | `sega32x` | `sega32` | `sega32x` |
| SG-1000 | `sg1000` | `sg1000` | `sg1000` | `sg-1000` |
| Atari Lynx | `atarilynx` | `lynx` | `lynx` | `atarilynx` |
| Atari Jaguar | `atarijaguar` | `jaguar` | `jaguar` | `atarijaguar` |
| TG-16/PCE | `pcengine` | `pcengine` | `tg16` | `pcengine` |
| PCE-CD | `pcenginecd` | `pcenginecd` | `turbografx-cd` | `pcenginecd` |
| Neo Geo AES | `neogeo` | `neogeo` | `neogeoaes` | `neogeo` |
| Neo Geo Pocket | `ngp` | `ngp` | `neo-geo-pocket` | `ngp` |
| NGPC | `ngpc` | `ngpc` | `neo-geo-pocket-color` | `ngpc` |
| ZX Spectrum | `zxspectrum` | `zxspectrum` | `zxs` | `zxspectrum` |
| WonderSwan | `wonderswan` | `wswan` | `wonderswan` | `wonderswan` |
| WonderSwan Color | `wonderswancolor` | `wswanc` | `wonderswan-color` | `wonderswancolor` |
| ColecoVision | `colecovision` | `colecovision` | `colecovision` | `coleco` |

---

## Gaps / Further Research Needed

1. **Romulus** (romulus.cc) — JavaScript-protected SPA; folder naming unknown. Likely user-defined since it is a ROM auditing tool, not an organiser.
2. **LaunchBox BigBox** — officially folder-agnostic; no canonical folder names. Platform Name strings are standardised (documented above) but do not map to filesystem paths.
3. **RetroPie PSP, PS Vita, Xbox, Xbox 360** — not supported on Raspberry Pi hardware; absent from source scripts.
4. **Remus integration spec** — if Remus adds a "copy to frontend" export feature, the mapping table above should live in `src/core/constants/` as a constexpr or static map.
