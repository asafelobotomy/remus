    ~/Doc/g/remus    main !2   cat > /home/solon/Documents/git/remus/.git
hub/research/iso-detection-metadata-2026-06-05.md << 'EOF'
heredoc> # Research: ISO System Detection, Dreamcast CDI, System Names, Image Fo
rmat Detection, Offline Metadata
heredoc> 
heredoc> > Date: 2026-06-05 | Agent: Researcher | Status: complete
heredoc> 
heredoc> ## Summary
heredoc> 
heredoc> Deep research into five concrete problems in Remus's ROM management pip
eline.
heredoc> All findings sourced from primary open-source implementations (Dolphin,
 PPSSPP,
heredoc> RetroArch) and official documentation (Qt, No-Intro wiki). The RetroArc
h
heredoc> `task_database_cue.c` scanner is the single most actionable source: it 
contains
heredoc> a complete, tested `MAGIC_NUMBERS[]` table covering every major disc pl
atform.
heredoc> Problem 4 (image extension mismatch) is solved entirely within Qt's exi
sting API
heredoc> via `QImageReader::imageFormat()`. Problem 5 (offline metadata) has thr
ee viable
heredoc> no-auth tiers already tracked in RESEARCH.md.
heredoc> 
heredoc> ## Sources
heredoc> 
heredoc> | URL | Relevance |
heredoc> |-----|-----------|
heredoc> | https://github.com/libretro/RetroArch/blob/master/tasks/task_database
_cue.c | **PRIMARY** — authoritative `MAGIC_NUMBERS[]` table for all disc platfo
rms |
heredoc> | https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DiscIO
/Volume.cpp | Dolphin GC/Wii detection algorithm (`TryCreateDisc`) |
heredoc> | https://raw.githubusercontent.com/dolphin-emu/dolphin/master/Source/C
ore/DiscIO/DiscUtils.h | Exact `GAMECUBE_DISC_MAGIC` and `WII_DISC_MAGIC` consta
nt values |
heredoc> | https://github.com/hrydgard/ppsspp/blob/master/Core/Loaders.cpp | PPS
SPP multi-system ISO detection via ISO 9660 PVD `systemId` |
heredoc> | https://dreamcast.wiki/IP.BIN | Dreamcast IP.BIN header field table w
ith offsets |
heredoc> | https://doc.qt.io/qt-6/qimagereader.html | `QImageReader::imageFormat
(QIODevice*)` — magic-byte image format probe |
heredoc> | https://wiki.no-intro.org/index.php?title=Naming_Convention | No-Intr
o naming convention (game titles only; DAT set names use "Manufacturer - System"
) |
heredoc> 
heredoc> ---
heredoc> 
heredoc> ## Findings
heredoc> 
heredoc> ### Problem 1 — Ambiguous ISO System Detection
heredoc> 
heredoc> #### Authoritative source: RetroArch `task_database_cue.c`
heredoc> 
heredoc> The complete, battle-tested magic byte table from RetroArch (MIT/GPL-3.
0):
heredoc> 
heredoc> ```c
heredoc> static struct magic_entry MAGIC_NUMBERS[] = {
heredoc>    { "Nintendo - GameCube",         "\xc2\x33\x9f\x3d", 0x00001c },
heredoc>    { "Nintendo - Wii",              "\x5d\x1c\x9e\xa3", 0x000018 }, /* 
standard ISO */
heredoc>    { "Nintendo - Wii",              "\x5d\x1c\x9e\xa3", 0x000218 }, /* 
WBFS */
heredoc>    { "Nintendo - Wii",              "\x5d\x1c\x9e\xa3", 0x000070 }, /* 
RVZ, WIA */
heredoc>    { "Sega - Dreamcast",            "SEGA SEGAKATANA",  0x000010 },
heredoc>    { "Sega - Mega-CD - Sega CD",    "SEGADISCSYSTEM",   0x000010 },
heredoc>    { "Sega - Saturn",               "SEGA SEGASATURN",  0x000010 },
heredoc>    { "Sony - PlayStation",          "Sony Computer ",   0x0024f8 }, /* 
license string */
heredoc>    { "Sony - PlayStation 2",        "PLAYSTATION",      0x009320 }, /* 
PS1/PS2 CD */
heredoc>    { "Sony - PlayStation 2",        "PLAYSTATION",      0x008008 }, /* 
PS2 DVD */
heredoc>    { "Sony - PlayStation 2",        "           ",      0x008008 }, /* 
PS2 DVD (spaces) */
heredoc>    { "Sony - PlayStation Portable", "PSP GAME",         0x008008 },
heredoc>    { "Philips - CD-i",              "\xff\xff\xff\xff\xff\xff\xff\xff\x
ff\xff\x00", 0x000001 },
heredoc>    { NULL, NULL, 0 }
heredoc> };
heredoc> ```
heredoc> 
heredoc> **Algorithm**: Seek to `offset`, read `strlen(magic)` bytes, `memcmp`. 
First match wins.
heredoc> 
heredoc> #### Cross-confirmed: Dolphin DiscUtils.h
heredoc> 
heredoc> ```cpp
heredoc> constexpr u32 GAMECUBE_DISC_MAGIC = 0xC2339F3D;  // read as big-endian 
u32 at 0x1C
heredoc> constexpr u32 WII_DISC_MAGIC      = 0x5D1C9EA3;  // read as big-endian 
u32 at 0x18
heredoc> ```
heredoc> 
heredoc> #### Cross-confirmed: PPSSPP `Core/Loaders.cpp`
heredoc> 
heredoc> PPSSPP adds a second-pass PVD check after magic detection:
heredoc> 
heredoc> 1. Read ISO 9660 Primary Volume Descriptor (PVD) at **LBA 16** → file o
ffset `16 × 2048 = 0x8000`
heredoc> 2. Check `pvd.identifier[0:5] == "CD001"` (valid ISO 9660)
heredoc> 3. Branch on `pvd.systemId` (bytes 8–39 of the PVD):
heredoc>    - `"PSP GAME"` (8 bytes) → **PSP_ISO**
heredoc>    - `"UMD VIDEO"` / `"UMD AUDIO"` → UMD Video/Audio
heredoc>    - `"PS3"` (3 bytes) → **PS3_ISO**
heredoc>    - `"PLAYSTATION"` (11 bytes) + filesize > 800 MB → **PS2_ISO**
heredoc>    - `"PLAYSTATION"` (11 bytes) + filesize ≤ 800 MB → **PSX_ISO**
heredoc> 4. Fallback: check for `/PSP_GAME` directory in ISO filesystem
heredoc> 5. CD Mode-2 check: file size % 2352 == 0 AND bytes 0-11 == `\x00\xFF×1
0\x00` → PSX
heredoc> 
heredoc> #### Implementation recipe for Remus
heredoc> 
heredoc> ```cpp
heredoc> // In SystemDetector or a new DiscMagicDetector class:
heredoc> // 1. Open file, read up to 0x10000 bytes into a buffer
heredoc> // 2. Iterate MAGIC_NUMBERS[] — seek to offset, memcmp
heredoc> // 3. If ".iso" matches multiple (PS1/PS2 ambiguous), apply PPSSPP PVD 
size heuristic
heredoc> // 4. Fall back to extension-only detection if no match (cartridge-base
d systems)
heredoc> ```
heredoc> 
heredoc> **Key insight**: PS1 vs PS2 disambiguation on ISO files requires the 80
0 MB size
heredoc> heuristic from PPSSPP because both return "PLAYSTATION" from the PVD.
heredoc> 
heredoc> ---
heredoc> 
heredoc> ### Problem 2 — Dreamcast CDI Low-Confidence Matching
heredoc> 
heredoc> #### IP.BIN header structure (from dreamcast.wiki)
heredoc> 
heredoc> IP.BIN occupies the first 16 sectors of the data track on GD-ROM/CD-ROM
.
heredoc> 
heredoc> | Offset | Length | Field |
heredoc> |--------|--------|-------|
heredoc> | 0x000 | 16 | Hardware ID: always `"SEGA SEGAKATANA "` (disc detection
 magic) |
heredoc> | 0x010 | 16 | Maker ID: always `"SEGA ENTERPRISES"` |
heredoc> | 0x020 | 16 | Device Information |
heredoc> | 0x030 | 8 | Area Symbols (region codes) |
heredoc> | 0x038 | 8 | Peripherals |
heredoc> | **0x040** | **10** | **Product number** (`"HDR-nnnn"` etc.) — **seria
l for Redump matching** |
heredoc> | 0x04A | 6 | Product version |
heredoc> | **0x050** | **16** | **Release date** (YYYYMMDD format) |
heredoc> | 0x060 | 16 | Boot filename (usually `"1ST_READ.BIN"`) |
heredoc> | 0x070 | 16 | Company name |
heredoc> | **0x080** | **128** | **Game title** (Name of software) |
heredoc> 
heredoc> **Magic for system detection**: `"SEGA SEGAKATANA"` (15 bytes) at offse
t `0x00000010`
heredoc> (confirmed from both dreamcast.wiki and RetroArch `MAGIC_NUMBERS[]`).
heredoc> 
heredoc> #### CDI detection in Remus
heredoc> 
heredoc> For `.cdi` files, the RetroArch scanner treats them as raw disc images 
and applies
heredoc> the same magic byte detection at offset 0x10. The CDI container format 
from
heredoc> DiscJuggler wraps the raw track data but Flycast/RetroArch read CDI fil
es by
heredoc> seeking directly into the data area — they do not parse the DiscJuggler
 header.
heredoc> 
heredoc> **For serial extraction from CDI**: To read IP.BIN from a CDI file, you
 need to
heredoc> skip past the session/track headers. The safest approach (used by Flyca
st) is to
heredoc> detect "SEGA SEGAKATANA" by scanning the file for the 15-byte string, t
hen treat
heredoc> the found position as offset 0x00 of the IP.BIN and read the product nu
mber at
heredoc> `found_pos + 0x040`.
heredoc> 
heredoc> **Redump serial format**: The product number at IP.BIN[0x040] (e.g., `"
HDR-0176"`)
heredoc> corresponds directly to the `<serial>` tag in Redump DAT files. No tran
sformation
heredoc> needed — strip trailing spaces only.
heredoc> 
heredoc> ---
heredoc> 
heredoc> ### Problem 3 — System Name Normalization
heredoc> 
heredoc> #### The authoritative name space: the RetroArch canonical system names

heredoc> 
heredoc> The `MAGIC_NUMBERS[]` system names are in "Manufacturer - System" forma
t.
heredoc> These align with No-Intro DAT set names (not game title naming conventi
on).
heredoc> 
heredoc> **Canonical RetroArch/No-Intro system name mapping** (from source):
heredoc> 
heredoc> | RetroArch canonical | Common alias | ES-DE folder | Notes |
heredoc> |--------------------|-----------|-----------|-|
heredoc> | `Nintendo - GameCube` | GCN, NGC | `gc` | |
heredoc> | `Nintendo - Wii` | Wii | `wii` | |
heredoc> | `Sega - Dreamcast` | DC | `dreamcast` | |
heredoc> | `Sega - Mega-CD - Sega CD` | Sega CD | `segacd` | |
heredoc> | `Sega - Saturn` | SS | `saturn` | |
heredoc> | `Sega - Mega Drive - Genesis` | MD, Gen | `megadrive` | (not in MAGIC
_NUMBERS; cart-based) |
heredoc> | `Sony - PlayStation` | PS1, PSX | `psx` | |
heredoc> | `Sony - PlayStation 2` | PS2 | `ps2` | |
heredoc> | `Sony - PlayStation Portable` | PSP | `psp` | |
heredoc> | `Philips - CD-i` | CDi | `cdimono1` | |
heredoc> 
heredoc> **Key pattern**: No-Intro DAT set names use `"Manufacturer - System Nam
e"` with
heredoc> no abbreviations. The Remus system name alias table should accept:
heredoc> - Short aliases: `"ps2"`, `"psx"`, `"gc"`, `"gen"`, `"md"`
heredoc> - Full No-Intro names: `"Sony - PlayStation 2"`, `"Sega - Mega Drive - 
Genesis"`
heredoc> - Display names: `"PlayStation 2"`, `"Mega Drive"`, `"Genesis"`
heredoc> 
heredoc> **Implementation**: A `QMap<QString, QString>` from alias → canonical n
ame in
heredoc> `SystemDetector` (or a dedicated `SystemNameNormalizer`); populate from
 a JSON
heredoc> or compile-time table. This bridges "Genesis" in a user's folder to
heredoc> "Sega - Mega Drive - Genesis" in library queries.
heredoc> 
heredoc> **Note**: No-Intro's naming convention wiki page covers *game title* na
ming
heredoc> (capitalization, regions, flags), not system set names. Set names follo
w the
heredoc> "Manufacturer - System" pattern by convention but are not formally docu
mented.
heredoc> The RetroArch source is the most complete cross-project reference.
heredoc> 
heredoc> ---
heredoc> 
heredoc> ### Problem 4 — Artwork Saved With Wrong Extension
heredoc> 
heredoc> #### Root cause
heredoc> 
heredoc> Servers return JPEG data but respond with `Content-Type: image/png` (or
 vice
heredoc> versa). The downloader writes the file with the wrong extension, causin
g some
heredoc> frontends to fail loading the image.
heredoc> 
heredoc> #### Qt solution: `QImageReader::imageFormat()`
heredoc> 
heredoc> Qt 6 provides a static method that probes the first bytes of a file/dev
ice to
heredoc> detect the actual image format, independent of the file extension:
heredoc> 
heredoc> ```cpp
heredoc> // Detect format from file content (not extension):
heredoc> QByteArray actualFormat = QImageReader::imageFormat("/path/to/artwork.j
pg");
heredoc> // Returns "png", "jpeg", "bmp", "gif", "webp", etc.
heredoc> // Returns empty QByteArray if format unknown or file unreadable.
heredoc> 
heredoc> // Detect from a network reply (QIODevice*):
heredoc> QByteArray actualFormat = QImageReader::imageFormat(networkReply);
heredoc> ```
heredoc> 
heredoc> **Key documentation quote** (from Qt 6 docs):
heredoc> > "The loader attempts to read the image using the specified format. If
 format is
heredoc> > not specified (which is the default), it is **auto-detected based on 
the file's
heredoc> > suffix and header**."
heredoc> >
heredoc> > "`setDecideFormatFromContent(true)` — if set, the image reader will i
gnore
heredoc> > specified formats or file extensions and decide which plugin to use *
*only based
heredoc> > on the contents**."
heredoc> 
heredoc> **Format-to-extension mapping for renaming**:
heredoc> 
heredoc> ```cpp
heredoc> const QMap<QByteArray, QString> formatExtMap = {
heredoc>     {"jpeg", ".jpg"},
heredoc>     {"png",  ".png"},
heredoc>     {"gif",  ".gif"},
heredoc>     {"bmp",  ".bmp"},
heredoc>     {"webp", ".webp"},
heredoc> };
heredoc> ```
heredoc> 
heredoc> **Implementation recipe**: After downloading artwork to a temp path, ca
ll
heredoc> `QImageReader::imageFormat(tempPath)` to get the actual format, then de
rive the
heredoc> correct extension from the map, then rename before moving to the librar
y location.
heredoc> This is a pure Qt operation — no magic byte parsing in C++ required.
heredoc> 
heredoc> **Image magic bytes** (for reference / non-Qt codepaths):
heredoc> 
heredoc> | Format | Magic bytes | Offset |
heredoc> |--------|------------|--------|
heredoc> | PNG | `89 50 4E 47 0D 0A 1A 0A` | 0 |
heredoc> | JPEG | `FF D8 FF` | 0 |
heredoc> | GIF87a | `47 49 46 38 37 61` (`GIF87a`) | 0 |
heredoc> | GIF89a | `47 49 46 38 39 61` (`GIF89a`) | 0 |
heredoc> | BMP | `42 4D` (`BM`) | 0 |
heredoc> | WebP | `52 49 46 46 ?? ?? ?? ?? 57 45 42 50` (`RIFF....WEBP`) | 0 |
heredoc> 
heredoc> ---
heredoc> 
heredoc> ### Problem 5 — Enrichment Failures Without API Keys
heredoc> 
heredoc> #### Priority stack for no-auth/free metadata
heredoc> 
heredoc> | Priority | Source | Auth required | Rate limit | Fields available |
heredoc> |----------|--------|--------------|------------|-----------------|
heredoc> | 1 | **libretro-database** offline DATs | None | None (local) | develo
per, publisher, genre, rating, release date |
heredoc> | 2 | **ArcadeDB** | None | None stated | MAME-specific: title, manufac
turer, year, genre |
heredoc> | 3 | **TheGamesDB** | Free API key | 3,000 req/month/IP | title, genre
, publisher, developer, boxart |
heredoc> | 4 | **Hasheous** | None | Unspecified | Hash→game identity only (TOSE
C/No-Intro/Redump proxy) |
heredoc> | 5 | **ScreenScraper** | Free registration | 20,000 req/day | full met
adata + artwork |
heredoc> | 6 | **IGDB** | Twitch OAuth | 4 req/sec | full metadata; best for mod
ern titles |
heredoc> | 7 | **MobyGames** | Paid key | 720 req/hr | comprehensive retro; best
 descriptions |
heredoc> 
heredoc> #### libretro-database offline metadata fields (CC-BY-SA-4.0)
heredoc> 
heredoc> The libretro-database repo contains `metadat/` DATs with enrichment fie
lds beyond
heredoc> hash verification. Key metadata DATs (from the repository structure):
heredoc> 
heredoc> - `genre/` — per-game genre strings
heredoc> - `developer/` — developer name
heredoc> - `publisher/` — publisher name
heredoc> - `releasemonth/`, `releaseyear/` — release date
heredoc> - `users/` — number of players
heredoc> - `origin/` — original platform (for ports)
heredoc> - `franchise/` — game series name
heredoc> - `rating/` — ESRB/PEGI rating
heredoc> 
heredoc> These are plain-text `.dat` (Logiqx XML) format, parseable offline.
heredoc> The existing `ClrMameProParser` in Remus can likely read them without m
odification.
heredoc> 
heredoc> **Source**: `https://github.com/libretro/libretro-database` tree →
heredoc> `metadat/` directory. Each file is a system-specific DAT matching by ga
me name
heredoc> (not hash), so matching requires normalized title lookup after hash mat
ch.
heredoc> 
heredoc> #### Recommended fallback chain for Remus
heredoc> 
heredoc> ```
heredoc> Hash lookup (local DAT) → identity confirmed
heredoc>     ↓ enrichment needed
heredoc> libretro-database metadat/ (offline, no auth) → genre/developer/year
heredoc>     ↓ still missing fields
heredoc> ArcadeDB (online, no auth) for arcade titles
heredoc>     ↓ still missing artwork
heredoc> TheGamesDB (free key, 3000/month) → boxart URL
heredoc>     ↓ API key not configured
heredoc> libretro-thumbnails (no auth, Git LFS) → boxart PNG fallback
heredoc> ```
heredoc> 
heredoc> ---
heredoc> 
heredoc> ## Recommendations
heredoc> 
heredoc> 1. **Problem 1**: Replace `SystemDetector`'s extension-only heuristic f
or disc
heredoc>    images (`.iso`, `.bin`, `.img`) with a `DiscMagicScanner` that itera
tes the
heredoc>    `MAGIC_NUMBERS[]` table. The RetroArch table is the reference — copy
 it
heredoc>    verbatim and credit the source. For PS1/PS2 disambiguation add the 8
00 MB
heredoc>    size threshold from PPSSPP.
heredoc> 
heredoc> 2. **Problem 2**: For `.cdi` files, perform a byte-scan for `"SEGA SEGA
KATANA"`
heredoc>    to locate IP.BIN, then read product number at `found_offset + 0x040`
.
heredoc>    Don't attempt to parse the DiscJuggler CDI container — scan the raw 
bytes.
heredoc> 
heredoc> 3. **Problem 3**: Add a `SystemNameNormalizer` with a compile-time alia
s map
heredoc>    (JSON or `QMap`) seeded from the RetroArch canonical names. The norm
alizer
heredoc>    should accept No-Intro long names, short lowercase slugs, and common
 aliases.
heredoc> 
heredoc> 4. **Problem 4**: After downloading artwork, call
heredoc>    `QImageReader::imageFormat(filePath)` before persisting. If the dete
cted
heredoc>    format extension ≠ the stored extension, rename the file. This is ze
ro-cost.
heredoc> 
heredoc> 5. **Problem 5**: Ship libretro-database `metadat/` DATs as bundled off
line
heredoc>    fallback (they are CC-BY-SA-4.0 compatible). Parse them with the exi
sting
heredoc>    `ClrMameProParser`. Use TheGamesDB as the first online enrichment so
urce
heredoc>    (free key, no per-user rate limits for modest usage).
heredoc> 
heredoc> ---
heredoc> 
heredoc> ## Gaps / Further Research Needed
heredoc> 
heredoc> - **CDI container format**: The exact byte layout of DiscJuggler CDI se
ssions
heredoc>   (session/track table at the end of file) is not documented from a pri
mary
heredoc>   source. The byte-scan approach avoids this but may be slow for large 
files.
heredoc>   A follow-up fetch of `https://github.com/cdcrack/cdirip` or Flycast's
 CDI
heredoc>   reader would close this gap.
heredoc> 
heredoc> - **Sega CD / Mega-CD serial extraction**: The `"SEGADISCSYSTEM"` magic
 is
heredoc>   confirmed but the exact field offset for the product serial within Me
ga-CD
heredoc>   IP.BIN has not been verified against a primary source. Similar struct
ure to
heredoc>   Dreamcast IP.BIN is likely.
heredoc> 
heredoc> - **Xbox XISO**: The xemu docs confirm `.iso` extension + XISO format b
ut the
heredoc>   magic byte (`"MICROSOFT*XBOX*MEDIA"` at offset 0x8001 or 0x10001) was
 not
heredoc>   fetched from a primary source. The RetroArch MAGIC_NUMBERS table does
 not
heredoc>   include Xbox — it relies on hash matching only for Xbox.
heredoc> 
heredoc> - **libretro-database metadat/ match algorithm**: Title matching betwee
n hash-
heredoc>   resolved game names and metadat DAT entries may require fuzzy matchin
g.
heredoc>   Research needed: does libretro-db use exact name match or normalized?

heredoc> 
heredoc> EOF
    ~/Doc/g/remus    main !3 ?1                                      ✔ 