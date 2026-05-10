# Research: Offline Video Game Metadata & DAT Sources for Compendium Enrichment

> Date: 2026-05-09 | Agent: Researcher | Status: final

## Summary

The compendium's worst-covered systems — Xbox 360, IBM PC/DOS, PS2, Nintendo DS, Commodore Amiga, Commodore 64, Amstrad CPC, and NES — can be meaningfully improved with three high-impact additions: **(1) TOSEC DATs** for ROM identification of Amiga/C64/Amstrad CPC, **(2) MobyGames API** (already wired as a live provider) for rich metadata on all legacy computer platforms and PS2, and **(3) the LaunchBox Metadata.xml bulk XML** which provides developer/publisher/genre/description for 300+ platforms. A fourth option, **OpenVGDB**, is an offline SQLite database with hash-to-description mapping that could immediately fill NES/SNES/GBA descriptions. No source provides a complete hash+metadata offline bulk download for Xbox 360 or DOS in a single package — these require either live API calls or assembling multiple sources.

---

## Sources

| URL | Relevance |
|-----|-----------|
| https://github.com/OpenVGDB/OpenVGDB | SQLite ROM hash→metadata database, offline download |
| https://github.com/OpenVGDB/OpenVGDB/releases | Release assets — openvgdb.zip (~8.7 MB) |
| https://www.tosecdev.org/downloads | TOSEC complete DAT pack download page |
| https://www.tosecdev.org/downloads/category/59-2025-03-13 | TOSEC 2025-03-13 release notes (4743 DATs) |
| https://github.com/libretro/libretro-database/tree/master/metadat | libretro-database metadat/ directory listing |
| https://github.com/libretro/libretro-database/tree/master/metadat/genre | genre/ systems confirmed |
| https://github.com/libretro/libretro-database/tree/master/metadat/developer | developer/ systems — includes PS2, PS1, Dreamcast |
| https://github.com/libretro/libretro-database/tree/master/metadat/publisher | publisher/ systems — includes DS, 3DS, PS2 |
| https://github.com/libretro/libretro-database/tree/master/metadat/releaseyear | releaseyear/ systems |
| https://www.gametdb.com | GameTDB home — confirmed systems: Wii, GC, WiiU, 3DS, DS, Switch, PS3 |
| https://github.com/gaseous-project/hasheous | Hasheous — live API service only, no bulk download |
| https://api-docs.retroachievements.org/v1/get-game-list.html | RA API to get per-system hash list (MD5 per game) |
| https://datomatic.no-intro.org/index.php?page=download | No-Intro DAT-o-MATIC daily download (requires free account) |
| https://gamesdb.launchbox-app.com | LaunchBox Games DB — 300+ platforms, rich metadata |
| https://www.mobygames.com/info/api/ | MobyGames API docs — 720 req/hr, comprehensive legacy coverage |
| https://www.screenscraper.fr/faq.php | Screenscraper — API only, CC BY-NC-SA 4.0, no offline dump |
| https://shiraga.me/ | shiragame — SQLite compiling No-Intro+Redump+TOSEC hashes |
| https://github.com/SnowflakePowered/shiragame | shiragame GitHub — last release 2022-08-13, 87 MB zip, MIT license |

---

## Source Analysis Table

| # | Source | License | Format | Offline Bulk | Size est. | Key Systems (worst-coverage) | Hash types | Key metadata fields | Live provider? | Limitations |
|---|--------|---------|--------|-------------|-----------|------------------------------|------------|---------------------|----------------|-------------|
| 1 | OpenVGDB | Public domain (data) | SQLite | ✅ Yes | ~9 MB zip (~65 MB DB) | NES, SNES, GBA, GBC, N64, PS1, PS2, GCG, Wii, DS, 3DS, PSP, Xbox, Xbox 360 partial | MD5 | name, year, dev, genre, desc, artwork URLs | ❌ No | Last updated Nov 2021; missing DOS/Amiga/C64/Amstrad; artwork URLs may be dead |
| 2 | TOSEC DATs | CC BY-NC-SA 4.0 (DAT text) | ClrMAMePro DAT | ✅ Yes | ~50 MB zip (3111 DATs) | Amiga ✅, C64 ✅, Amstrad CPC ✅, Atari ST, MSX, DOS partial | CRC32, MD5, SHA1 | name only (filename-encoded region/year flags) | ❌ No | No rich metadata; title=filename; ROM identification only |
| 3 | Libretro-database metadat | CC BY-NC-SA 4.0 | ClrMAMePro DAT | ✅ Yes (git clone) | ~3 MB | DS ✅ (genre/dev/pub), PS2 ✅ (dev/pub only), NOT: Xbox 360, Amiga, C64, Amstrad | CRC32, SHA1 | genre, developer, publisher, year (system-dependent) | ✅ Already wired | Sparse coverage (~2-4% of games in worst systems); no description |
| 4 | GameTDB | CC BY-NC-SA 3.0 | XML | ✅ Yes | 1-5 MB per system | DS ✅, 3DS ✅, Wii ✅, WiiU ✅, Switch ✅, PS3 ✅, GC ✅; NO PS2, NO Xbox/360 | Game serial ID (not file hash) | title, genre, date, dev, pub, description (multi-language) | ✅ Already wired | Identification by disc serial, not file hash; limited to Nintendo+PS3 |
| 5 | Hasheous | AGPL-3.0 | REST JSON API | ❌ API only | N/A | Most via TOSEC/No-Intro/Redump/MAME | CRC32, MD5, SHA1, SHA256 | name via IGDB proxy | ✅ Already wired | No bulk download; requires API calls; IGDB license restrictions |
| 6 | RetroAchievements | GPL-3.0 (platform) | REST JSON API | ❌ API only (paginated) | N/A | Most retro consoles; DS, GBA, PS2, PS1, Xbox 360 limited | MD5 (RA-specific algorithm) | name, achievement count only | ✅ Already wired | Per-console API crawl needed; MD5 algorithm is RA-custom; no year/dev/genre/desc |
| 7 | No-Intro DAT-o-MATIC | Proprietary (no redistribution) | ClrMAMePro XML + DB XML | ⚠️ Yes (free account req'd) | ~30 MB (daily pack) | DS ✅, Xbox 360 ✅, C64 ✅, Amiga ✅, Amstrad CPC (as Flux/Misc only) | SHA256, MD5, SHA1, CRC32 | name, region, languages (limited metadata beyond hash+name) | Partially (libretro mirrors) | Requires account; no bulk desc/dev/genre; redistribution not allowed |
| 8 | LaunchBox Metadata.xml | Proprietary (no redistribution) | XML (in LaunchBox install) | ⚠️ Yes (via LaunchBox app) | ~500 MB uncompressed | Xbox 360 ✅, DOS ✅, Amiga ✅, C64 ✅, Amstrad CPC ✅, PS2 ✅, NES ✅, ALL platforms | None (name-match only) | title, year, dev, pub, genre, description, rating, players | ✅ Yes (TheGamesDB/LB) | License prohibits redistribution; requires LaunchBox software; no hash matching |
| 9 | MobyGames | Proprietary (CC restrictions) | JSON API | ❌ API only | N/A | DOS ✅, Amiga ✅, C64 ✅, Amstrad CPC ✅, Xbox 360 ✅, PS2 ✅ | None (name-match only) | title, year, dev, pub, genre, description, screenshots, covers | ✅ Already wired | 720 req/hr rate limit; no hash matching; bulk dump is 2016-era (Archive.org) |
| 10 | Screenscraper | CC BY-NC-SA 4.0 | REST JSON API | ❌ API only | N/A | 400+ systems including all worst-coverage systems | CRC32, MD5, SHA1, SHA256 | title, year, dev, pub, genre, description, artwork, video | ✅ Already wired | Daily thread limits; no bulk download; requires dev registration |

---

## Per-Source Details

### 1. OpenVGDB
- **URL**: https://github.com/OpenVGDB/OpenVGDB
- **Download**: https://github.com/OpenVGDB/OpenVGDB/releases/download/v29.0/openvgdb.zip (~8.7 MB compressed, ~65 MB SQLite)
- **License**: Public domain (data sourced from various community contributions)
- **Format**: SQLite database (`openvgdb.sqlite`)
- **Last updated**: November 11, 2021 (v29.0) — **abandoned/unmaintained**
- **Hash types**: MD5 (primary), some SHA1
- **Systems confirmed**: NES, SNES, N64, GB, GBC, GBA, DS, 3DS, PS1, PS2, PSP, GameCube, Wii, Xbox, Xbox 360 (partial coverage), Atari 2600/5200/7800, Lynx, Jaguar, Game Gear, Master System, Mega Drive, Saturn (disc serial), Dreamcast, PC Engine
- **Metadata fields**: `romFileName`, `romMD5`, `releaseTitle`, `releaseDate`, `releaseDeveloper`, `releasePublisher`, `releaseGenre`, `releaseDescription`, `releaseCoverFront` (URL), `releaseCoverBack` (URL), `releaseReferenceURL`
- **NOT covered**: DOS/PC, Commodore Amiga, Commodore 64, Amstrad CPC, ZX Spectrum
- **Recommendation**: **High priority**. Import into compendium as hash→metadata enrichment for NES (descriptions!), PS2 (desc), SNES, GBA. Hash join on MD5. Validate artwork URLs before storing — many may be dead since 2021.

### 2. TOSEC DATs
- **URL**: https://www.tosecdev.org/downloads
- **Download**: Direct zip from tosecdev.org (2025-03-13 release: `TOSEC - DAT Pack - Complete (4743).zip`, ~50 MB)
- **License**: CC BY-NC-SA 4.0 for the DAT text itself; ROM data unaffected
- **Format**: ClrMAMePro DAT (same format Remus already parses)
- **Last updated**: 2025-03-13 (active project, ~1 release/year)
- **DAT count**: 3111 TOSEC-Main + 283 TOSEC-ISO + 1132 TOSEC-PIX = 4745 total
- **Hash types**: CRC32, MD5, SHA1 (per ROM entry)
- **Systems with strong coverage**: Commodore Amiga (massive; hundreds of DATs), Commodore 64 (hundreds), Amstrad CPC (dozens), Atari 8-bit, Atari ST, MSX/MSX2, ZX Spectrum, DOS (limited), Apple II, Acorn, BBC Micro, Sinclair
- **Metadata fields**: Game title (from filename), region/country flags, year (sometimes encoded in filename as `(YYYY)`), language flags — **no structured year/dev/genre/desc fields**
- **Critical limitation**: TOSEC DATs identify ROMs by hash+filename but contain NO structured metadata beyond what's encoded in the TOSEC naming convention. The year is sometimes parseable from filename (e.g., `Game (1987)(Publisher)`).
- **Recommendation**: **High priority for ROM identification**. Parse TOSEC naming convention to extract year and publisher from filenames for Amiga/C64/Amstrad. The real value is enabling hash→gamename matching so live providers (MobyGames, Screenscraper) can then fetch rich metadata.

### 3. Libretro-database metadat (current coverage gap analysis)
- **URL**: https://github.com/libretro/libretro-database/tree/master/metadat
- **Already wired**: Yes (196 DATs from libretro-database)
- **What IS covered in metadat/**:
  - `genre/`: Atari 2600/5200/7800/Jaguar/Lynx, GB/GBC/GBA/NDS/N64/SNES/NES/3DS, Mega Drive, Game Gear, Master System, MSX, PSP, WonderSwan, ZX Spectrum +3 — **~50 systems**
  - `developer/`: All of genre/ PLUS Sony PlayStation, Sony PlayStation 2, Sega Dreamcast
  - `publisher/`: All of developer/ PLUS Nintendo DS, Nintendo 3DS (publisher present for DS!)
  - `releaseyear/`: Same scope as genre/ — **excludes PS2, disc-based systems**
- **NOT covered in any metadat/ subdir**: Xbox 360, Xbox, Commodore Amiga, Commodore 64, Amstrad CPC, IBM PC/DOS, Sega Saturn, PS3, Wii, GameCube
- **Key finding**: libretro-database HAS publisher+developer DATs for PS2 and DS. If these are not currently being ingested, they represent an easy win.
- **Recommendation**: Audit whether the compendium pipeline already ingests `metadat/developer/Sony - PlayStation 2.dat` and `metadat/publisher/Nintendo - Nintendo DS.dat`. If not, add them.

### 4. GameTDB Additional Platforms
- **URL**: https://www.gametdb.com
- **Already wired**: Yes (Wii, WiiU, 3DS, DS, PS3, Switch)
- **Confirmed platform downloads**: `wiitdb.zip`, `wiiutdb.zip`, `3dstdb.zip`, `dstdb.zip`, `switchtdb.zip`, `ps3tdb.zip`, `gc.zip` (GameCube — fetchable!)
- **PS2 confirmed ABSENT**: `gametdb.com/ps2tdb.zip` returns empty (PS2 is not supported by GameTDB)
- **Xbox/Xbox 360 confirmed ABSENT**: Not listed on the site
- **Format**: XML with `<game id="XXXXX">` entries; identifies games by disc serial, not file hash
- **Metadata**: title (multilingual), synopsis, genre, date, developer, publisher, online players, rating
- **Key finding**: GameCube (`gc.zip`) may not be in the current ingest — the site confirms GC data exists. This could improve GameCube coverage.
- **Recommendation**: Add GameCube XML if not already ingested. No benefit for worst-coverage systems.

### 5. Hasheous
- **URL**: https://hasheous.org / https://github.com/gaseous-project/hasheous
- **Already wired**: Yes (live API provider)
- **Bulk download**: ❌ None. Hasheous is a server-side application that consumes TOSEC/No-Intro/Redump/MAME DATs internally and exposes them via REST API.
- **To self-host**: Requires MariaDB 11.1.2+, IGDB API key, Docker. Would allow running a local instance fed with TOSEC/No-Intro DATs.
- **Recommendation**: No offline bulk benefit. Already wired. Could self-host for rate-limit-free local instance.

### 6. RetroAchievements Hash DB
- **URL**: https://retroachievements.org
- **Bulk download**: ❌ No static download (`/download.php` returns 403). Per-console API: `GET /API/API_GetGameList.php?i=<consoleId>&h=1`
- **Format**: JSON — returns `[{Title, ID, ConsoleID, Hashes: ["md5...", ...]}]`
- **Hash type**: MD5, but using the RetroAchievements custom hashing algorithm (strips headers, partial buffer for certain systems — not standard MD5 of whole file)
- **Metadata**: game title and achievement count only — **no year/dev/genre/desc**
- **Systems**: Most retro consoles (NES, SNES, GBA, Mega Drive, PS1, N64, DS, PS2, Xbox 360 subset)
- **Recommendation**: Not useful for metadata enrichment. Hash algorithm incompatibility with standard MD5. Skip for compendium purposes.

### 7. No-Intro DAT-o-MATIC
- **URL**: https://datomatic.no-intro.org/index.php?page=download
- **Download**: ⚠️ Free account required (confirmed by community). Daily bundle available: `Standard DAT`, `DB XML`, `P/C XML` formats.
- **Format**: ClrMAMePro XML (Standard DAT) or extended DB XML
- **Hash types**: SHA256, MD5, SHA1, CRC32 (all per ROM)
- **Systems confirmed in daily pack**: Nintendo DS, Xbox 360 (Digital, 17,439 entries!), Xbox 360 (Development), Commodore 64 (350 + 1665 PP entries), Commodore Amiga (Bitstream, Flux, standard), Amstrad CPC (Flux, Misc), IBM PC (various), NES, SNES, all major consoles — 200+ systems
- **DB XML format**: Includes additional fields beyond Standard DAT — confirm exact fields by downloading sample
- **License**: Proprietary; redistribution prohibited; for personal use
- **Key finding**: No-Intro already provides DATs via libretro-database mirror. The libretro-database `metadat/no-intro/` directory includes `Microsoft - Xbox 360.dat` and `Commodore - 64.dat` — so hash identification is already possible via the current compendium pipeline.
- **Recommendation**: The value-add is the DB XML format which may include extended metadata. Evaluate if DB XML adds year/dev fields beyond what libretro-database already provides. Requires account to download directly.

### 8. LaunchBox Games Database
- **URL**: https://gamesdb.launchbox-app.com
- **Bulk download**: ⚠️ `Metadata.xml` is packaged inside the LaunchBox Windows application (free tier). File is ~500 MB uncompressed XML. Community tools exist to extract it without running LaunchBox.
- **Format**: Large XML with `<Game>` elements containing all metadata
- **Hash types**: None — LaunchBox identifies games by title+platform matching, not file hash
- **Platforms**: 300+ including: Xbox 360, IBM PC Compatible, Commodore Amiga, Commodore 64, Amstrad CPC, PS2, NES, and everything else
- **Metadata fields**: `Title`, `ReleaseDate`, `Developer`, `Publisher`, `Genres`, `Overview` (description), `Rating`, `Players`, `DatabaseID`
- **License**: Proprietary (LaunchBox database license); non-commercial use permitted with attribution
- **Already wired**: Yes — `TheGamesDB` (which is the LaunchBox backend) is a live provider
- **Key insight**: The live TheGamesDB provider already hits this data via API. The Metadata.xml bulk download is only useful if rate-limited API calls are the bottleneck. The description field in `Overview` would directly address the 0% desc coverage on Xbox 360 and NES.
- **Recommendation**: Low priority for offline bulk since the API is already wired. If API rate limits are a bottleneck, the Metadata.xml extraction could provide a one-time bulk enrichment pass.

### 9. MobyGames
- **URL**: https://www.mobygames.com
- **API**: https://api.mobygames.com/v1/ (requires API key, paid subscription)
- **Rate limit**: 720 requests/hour for non-commercial (1 req/sec max)
- **Bulk download**: No current bulk download. An archived dump exists on the Internet Archive (pre-2016) but is severely outdated.
- **Format**: JSON API — `/games`, `/games/{id}`, `/games/{id}/platforms/{pid}`
- **Platforms with excellent coverage**: DOS (platform 2), Amiga (platform 19), Commodore 64 (platform 4), Amstrad CPC (platform 60), Xbox 360 (platform 69), PS2 (platform 7)
- **Metadata fields**: title, description, genres (multi-category), developer, publisher, year, screenshots, covers, ESRB rating, alternate titles
- **License**: Proprietary (MobyPro subscription for API access); data © MobyGames
- **Already wired**: Yes
- **Key insight**: MobyGames is the **single best source for DOS, Amiga, C64, and Amstrad CPC metadata** — these legacy platforms are MobyGames' strongest area and have been their focus since 1999. The DOS platform alone has 15,000+ games documented.
- **Recommendation**: Optimize the live provider's caching strategy rather than seeking an offline alternative. Ensure the MobyGames provider is called for Amiga/C64/Amstrad/DOS games after TOSEC identification.

### 10. Screenscraper
- **URL**: https://www.screenscraper.fr
- **License**: CC BY-NC-SA 4.0 (game metadata)
- **Bulk download**: ❌ API-only. No offline database dump available.
- **Hash types**: CRC32, MD5, SHA1, SHA256 — **most comprehensive hash matching**
- **Systems**: 400+ including all worst-coverage systems
- **Metadata**: title, year, dev, pub, genre, description (multilingual), artwork (box/screenshot/video)
- **Already wired**: Yes
- **Key finding**: Screenscraper uses all 4 hash types for matching and has strong Amiga/C64/Amstrad CPC coverage via community contributions.
- **Recommendation**: Already wired. Ensure it is being called for TOSEC-identified Amiga/C64/Amstrad games with CRC32.

---

## Additional Discovery: shiragame

Discovered during research — not in original scope:

- **URL**: https://shiraga.me / https://github.com/SnowflakePowered/shiragame
- **License**: MIT (compiled data from Redump/No-Intro/TOSEC)
- **Format**: SQLite database (`shiragame.db.zip` — ~87 MB compressed, ~170 MB uncompressed)
- **Last release**: 2022-08-13 (manual release; project shows reduced activity)
- **Content**: Hash→gamename lookup compiled from No-Intro + Redump + TOSEC DATs; Stone platform identifiers
- **Metadata**: ROM hash (SHA1/MD5/CRC), platform_id, game_title, region — **no year/dev/genre/desc**
- **Verdict**: Partially redundant with the compendium's existing libretro-database DAT ingestion. Useful as a pre-compiled cross-source hash index, but stale since 2022.

---

## Priority Ranking by Estimated Impact

Ranked by: `(games in worst-coverage system) × (metadata fields that would be filled) × (feasibility)`

| Rank | Source | Impact | Target Systems | Fields Filled | Effort | Coverage Improvement Est. |
|------|--------|--------|----------------|---------------|--------|---------------------------|
| 1 | **TOSEC DATs** | 🔴 Critical | Amiga (7,232), C64 (3,329), Amstrad (3,011) | ROM identification → enables live provider calls | Low (already parse ClrMAMePro) | ~70% of Amiga/C64/Amstrad ROMs now identifiable by hash |
| 2 | **OpenVGDB SQLite** | 🔴 Critical | NES (7,651 desc=0%), PS2 (desc=0%), Xbox 360 (partial), SNES/GBA | year, dev, genre, **desc**, artwork URL | Low (import SQLite→SQL JOIN on MD5) | NES desc: ~60-80%; PS2 desc: ~40%; SNES/GBA desc: ~70% |
| 3 | **libretro metadat/developer + publisher for PS2/DS** | 🟡 Medium | PS2 (dev 34%→fill gap), DS (dev 2%→fill more) | developer, publisher | Trivial (add 2 DAT files to ingest) | PS2 dev/pub: +10-20%; DS dev/pub: +5-10% |
| 4 | **MobyGames API optimization** | 🟡 Medium | DOS (15,925), Amiga, C64, Amstrad after TOSEC ident. | year, dev, pub, genre, desc | Medium (optimize retry/caching) | DOS genre/desc: 20-40% once games are name-matched |
| 5 | **GameTDB GameCube XML (gc.zip)** | 🟢 Low-Med | GameCube (if not already ingested) | title, genre, date, dev, pub, desc | Low | ~80% of GC games have GameTDB entries |
| 6 | **No-Intro DB XML (daily)** | 🟢 Low | DS, Xbox 360, C64, Amiga | Extended hash+name fields (if DB format adds year/dev) | Medium (account + parsing) | Uncertain — depends on DB XML schema |
| 7 | **LaunchBox Metadata.xml** | 🟢 Low | Xbox 360, DOS, Amiga (desc) | year, dev, pub, genre, desc | High (extract from app, no hash match) | Name-only match (~50-70% hit rate for well-known games) |
| 8 | **RetroAchievements API** | ⚪ Minimal | Most consoles | name only (no enrichment) | Low | 0% improvement on metadata fields |
| 9 | **Hasheous (self-hosted)** | ⚪ Minimal | All via TOSEC/No-Intro | Same as current live provider | Very High (infrastructure) | 0% improvement over current wired provider |
| 10 | **Screenscraper offline** | ⚪ None | N/A | No offline option exists | N/A | Already wired as live provider |

---

## Implementation Notes for Top-3 Sources

### 1. TOSEC DATs — Hash Identification for Amiga/C64/Amstrad

**What to parse**: Download `TOSEC - DAT Pack - Complete (4743).zip` from tosecdev.org. Extract the ClrMAMePro DATs for:
- `Commodore Amiga - Games - [ADF] (TOSEC-*.dat)`
- `Amstrad CPC - Games - [DSK] (TOSEC-*.dat)`
- `Commodore C64 - Games - [T64] (TOSEC-*.dat)` (and .d64 variants)

**Format join**: Same ClrMAMePro parser Remus already uses (`test_clrmamepro_parser.cpp`). Each `<rom>` entry has `crc`, `md5`, `sha1` and `name` (the TOSEC-formatted filename).

**TOSEC filename parsing** — extract structured fields from the game name:
```
Alien Breed (1991)(Team 17)[!].adf
         ^year     ^publisher ^flags
```
Regex: `^(.+?)\s*\((\d{4})\)\((.+?)\)` → captures title, year, publisher.

**Store in compendium**: Map TOSEC CRC/MD5/SHA1 → `(game_title, year, publisher)` in a new `tosec_signatures` table. Then join on existing ROM hashes to backfill `year` and `publisher` for Amiga/C64/Amstrad where live providers returned nothing.

**Database tables needed**:
```sql
CREATE TABLE tosec_signatures (
  crc32 TEXT, md5 TEXT, sha1 TEXT,
  title TEXT, year INTEGER, publisher TEXT,
  system TEXT, flags TEXT
);
```

---

### 2. OpenVGDB SQLite — Hash-to-Description for NES/PS2/SNES/GBA

**What to download**: `openvgdb.zip` from https://github.com/OpenVGDB/OpenVGDB/releases/download/v29.0/openvgdb.zip (~8.7 MB → ~65 MB SQLite).

**Key tables** (from OpenVGDB schema):
- `RELEASES`: `releaseID`, `romID`, `releaseTitleName`, `releaseDate`, `releaseDeveloper`, `releasePublisher`, `releaseGenre`, `releaseDescription`, `releaseCoverFront`
- `ROMS`: `romID`, `romFileName`, `romMD5`, `romSize`, `systemID`
- `SYSTEMS`: `systemID`, `systemName`

**Format join**: `JOIN on lower(hex(md5_hash)) = ROMS.romMD5`. The MD5 in OpenVGDB is stored as a hex string.

**Metadata fields to import**: `releaseTitleName`, `releaseDate` (parse year), `releaseDeveloper`, `releasePublisher`, `releaseGenre`, `releaseDescription` (desc!), `releaseCoverFront` (artwork URL).

**Target systems mapping**:
- `Nintendo Entertainment System (NES)` → fill `desc` for 7,651 NES games
- `Sony PlayStation 2` → fill `desc` for 11,046 PS2 games
- `Microsoft Xbox 360` → check if present; likely sparse but worth checking
- `Super Nintendo` → fill desc
- `Game Boy Advance` → fill desc

**Caveats**: OpenVGDB is frozen at Nov 2021. All artwork URLs should be validated before persisting (many hotlink to external CDNs that may have changed). Store `source=openvgdb` in the metadata source column for traceability.

---

### 3. libretro metadat developer/publisher for PS2 and DS

**What to add**: Two DAT files not currently in the ingest pipeline (verify first):
- `https://github.com/libretro/libretro-database/raw/master/metadat/developer/Sony - PlayStation 2.dat`
- `https://github.com/libretro/libretro-database/raw/master/metadat/publisher/Nintendo - Nintendo DS.dat`
- `https://github.com/libretro/libretro-database/raw/master/metadat/publisher/Sony - PlayStation 2.dat`
- `https://github.com/libretro/libretro-database/raw/master/metadat/developer/Nintendo - Nintendo DS.dat` (if present)

**Format**: Same ClrMAMePro DAT format as current libretro DATs. Each `<game>` entry has `name`, `description`, and a metadata field like `developer "Konami"` or `publisher "Sony"`.

**Join key**: Game name matching from the existing `dat` entries → join on SHA1/CRC32 to fill `developer`/`publisher` for games where these fields are currently NULL.

**Effort**: Extremely low — just add these files to the `build_compendium_full.sh` ingest list.

---

## Gaps / Further Research Needed

1. **No-Intro DB XML schema**: Unknown if DB XML format includes year/developer fields beyond hash+name. Requires account to download and inspect. Worth a one-time investigation.

2. **TOSEC-ISO PS2 coverage**: TOSEC-ISO has PS2 WIP DATs. These are disc images identified by track hashes, which would require Redump-style CRC matching. Not currently parsed by Remus.

3. **Xbox 360 metadata void**: No offline bulk source covers Xbox 360 adequately. The only reliable approach is live API calls (MobyGames, TheGamesDB/LaunchBox, IGDB via Hasheous). Scraped offline option does not exist.

4. **DOS identification without hash**: IBM PC games often appear as loose file archives (.zip) or directories, making file hash matching extremely unreliable (the "ROM" is usually not a single file). The identification problem for DOS is structurally different from console ROMs.

5. **MobyGames 2016 Archive.org dump**: A partial MobyGames dump exists on Archive.org from ~2016. Severely outdated but could pre-seed DOS/Amiga descriptions for older games. URL to investigate: `https://archive.org/search?query=mobygames`.

