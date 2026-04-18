

## Full ROM Conversion Tool & Format Spreadsheet by Platform

| **Platform** | **Recommended Format** | **Conversion Tool** | **Notes** |
|-------------|------------------------|---------------------|---------|
| **Nintendo (NES, SNES, GB, GBC, GBA)** | `.zip` (recommended), `.nes`, `.smc`, `.gb`, `.gbc`, `.gba` | **RetroMultiTools**, **RomVault**, manual rename | Cartridge-based; rename extensions if needed.  Use `.z64` for N64 (BigEndian). |
| **Nintendo 64** | `.z64`, `.n64`, `.v64` | **RetroMultiTools** (N64 Format Converter), **RomVault** | Prefer `.z64` (BigEndian).  Tools can convert between byte orders. |
| **Nintendo DS** | `.zip`, `.nds` | **RetroMultiTools**, manual archive | ROMs must be decrypted for most emulators.  |
| **Nintendo 3DS** | `.zcci` (recommended), `.cci`, `.cia` | **Azahar** (PC/Android), **Batch CIA Decryptor** | Decrypt `.cia`/`.cci` first, then compress to `.zcci` for space savings.  |
| **Nintendo GameCube / Wii** | `.rvz` (recommended), `.iso` | **Dolphin Emulator** (v5.0-12188+) | Convert `.iso` to `.rvz` in Dolphin for compression.  |
| **Nintendo Wii U** | `.wua` | **Cemu Title Manager** | Compress game data into a single `.wua` archive.  |
| **Sega Dreamcast** | `.chd` (recommended), `.gdi`, `.cdi` | **CHDMAN** (Dreamcast-specific), **CHDroid**, **RetroMultiTools** | Convert `.gdi` to `.chd`; `.bin/.cue` cannot be directly converted.  |
| **Sega Saturn** | `.chd` (recommended), `.cue` + `.bin` | **CHDMAN**, **CHDroid**, **RetroMultiTools** | Supports `.cue`, `.iso`, `.gdi` → `.chd`.  |
| **Sega CD / Mega CD** | `.cue` + `.bin`, `.chd` | **CHDMAN**, **RetroMultiTools** | Multi-file ROMs must be in `.zip` or `.7z`.  |
| **Sega Genesis / Master System / Game Gear** | `.zip`, `.md`, `.sms`, `.gg` | **RetroMultiTools**, manual archive | Cartridge-based; simple rename or zip.  |
| **Sony PlayStation (PS1)** | `.chd` (recommended), `.cue` + `.bin` | **CHDMAN**, **CHDroid**, **RetroMultiTools** | Convert `.cue`/`.bin` → `.chd` for space.  Use `createdvd` for PS1. |
| **Sony PlayStation 2** | `.chd` (recommended), `.iso` | **CHDMAN**, **CHDroid** | Convert `.cue`, `.iso`, `.gdi` → `.chd`.  |
| **Sony PlayStation 3** | `.iso`, folder (`PS3_GAME`) | **ps3-disc-dumper**, **PS3 ISO Tools** | Decrypt `.iso` if needed.  Folder dumps work in RPCS3. |
| **Sony PlayStation Portable (PSP)** | `.chd` (recommended), `.iso`, `.cso` | **CHDMAN** (with `createdvd`), **CHDroid**, **PSP Shrink** | Use `createdvd` function.  `.cso` works on real hardware. |
| **Microsoft Xbox** | `.xiso` | **extract-xiso** | Convert `.iso` → `.xiso` for use in `xemu`.  |
| **Arcade (MAME/FinalBurn Neo)** | `.zip`, `.7z` | **RetroMultiTools**, **RomVault**, **CHDMAN** | Use non-merged ROMs.  CHD for CD-based arcade games. |
| **Neo Geo / Neo Geo CD** | `.zip`, `.neo`, `.cue` + `.bin` | **NeoDSConvert**, **RetroMultiTools** | Archive `.neo` in `.zip`.  |
| **Multi-Disc Games** | `.m3u` + `.chd`/`.cue` | Text editor | Create `.m3u` playlist listing all disc files (e.g., `Disc 1.chd`, `Disc 2.chd`).  |
| **ECM-Compressed Files** | `.bin` | **unECM** | Restore `.bin` from `.bin.ecm`.  Available via Homebrew (macOS) or standalone. |

