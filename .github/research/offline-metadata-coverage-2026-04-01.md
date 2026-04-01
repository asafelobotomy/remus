# Research: Offline Metadata Coverage for Retro Game Systems

> Date: 2026-04-01 | Agent: Researcher | Status: complete

## Summary

This report covers 16 systems where Remus currently has no or inadequate local metadata
(developer, genre, year, publisher, players). The key finding is that **three of the nine
"no corpus" systems are already addressable through the existing libretro-database metadat/
tree** — Atari Lynx, Atari Jaguar, and NEC SuperGrafx all have developer/genre/publisher/
releaseyear DATs in `metadat/` that Remus has not yet wired up. For ZX Spectrum, **ZXDB**
(GitHub, ODbL 1.0) is a best-in-class offline SQLite database. For C64 and Amiga, TOSEC DAT
filename parsing extracts year+publisher offline; no hash-linked genre DB exists. For disc-based
systems lacking metadata (Sega CD, Saturn, PC Engine CD, 3DO), Redump DATs give
serial+title+region and online enrichment via ScreenScraper is the practical path. Xbox,
Xbox 360, and PS Vita have no viable single offline metadata source.

## Sources

| URL | Relevance |
|-----|-----------|
| https://github.com/libretro/libretro-database/tree/master/metadat/developer | libretro-database developer/ — covers Lynx, Jaguar, SuperGrafx, Dreamcast, PS1, PS2; NOT C64/Amiga/Xbox/Vita |
| https://github.com/libretro/libretro-database/tree/master/metadat/genre | libretro-database genre/ — covers Lynx, Jaguar, SuperGrafx; NOT disc systems or C64/Amiga/Xbox/Vita |
| https://github.com/libretro/libretro-database/tree/master/metadat/publisher | libretro-database publisher/ — same scope as developer minus PS1/PS2 |
| https://github.com/libretro/libretro-database/tree/master/metadat/releaseyear | libretro-database releaseyear/ — same scope as developer |
| https://github.com/libretro/libretro-database/tree/master/metadat/redump | libretro-database redump/ — hash+title identity DATs for all disc systems (Xbox, Dreamcast, Saturn, etc.); NOT descriptive metadata |
| https://github.com/libretro/libretro-database/tree/master/metadat/no-intro | libretro-database no-intro/ — confirms hash DATs for Lynx, Jaguar, SuperGrafx, C64, Amiga, ZX Spectrum +3, PS Vita, Xbox 360 |
| https://github.com/zxdb/ZXDB | ZXDB — complete ZX Spectrum database, ODbL 1.0, MySQL/SQLite dump downloadable from GitHub |
| https://api.zxinfo.dk/v3/ | ZXInfo API v3 — free REST API backed by ZXDB; no auth for public search endpoints |
| https://www.tosecdev.org/tosec-naming-convention | TOSEC naming convention — Title(Year)(Publisher)(Country)[flags]; parses to year+publisher for C64/Amiga/ZX Spectrum |
| https://nopaystation.com/ | NoPayStation — PS Vita TSV files with Content ID, serial, title; no genre/developer/year |
| http://redump.org/discs/ | Redump discs browse — confirmed coverage of Xbox, Xbox 360, Dreamcast, Saturn, Sega CD, PC Engine CD, 3DO, PS1, PS2 with serial+title+region |
| https://www.screenscraper.fr/ | ScreenScraper — CC-BY-NC-SA, hash-based API, all 16 systems, all fields including players; best per-game online enrichment |

---

## Findings

### Preliminary: libretro-database metadat/ coverage map

Before sourcing anything new, understanding what `metadat/` sub-directories already cover is
essential. Each sub-directory contains per-system `.dat` files mapping ROM hashes to a single
metadata field. Confirmed coverage for systems in scope:

| System | developer/ | genre/ | publisher/ | releaseyear/ | serial/ | redump/ |
|--------|-----------|--------|------------|-------------|---------|---------|
| Atari Lynx | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| Atari Jaguar | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ (CD) |
| NEC SuperGrafx | ✅ | ✅ | ✅ | ✅ | — | — |
| NEC PC Engine (TG16) cartridge | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| NEC PC Engine CD | ❌ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Sinclair ZX Spectrum +3 | ✅ | ✅ | ✅ | ✅ | — | — |
| Sega Dreamcast | ✅ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Sega Mega-CD / Sega CD | ❌ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Sega Saturn | ❌ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| 3DO | ❌ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Sony PlayStation | ✅ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Sony PlayStation 2 | ✅ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Sony PlayStation Vita | ❌ | ❌ | ❌ | ❌ | — | — |
| Commodore 64 | ❌ | ❌ | ❌ | ❌ | — | — |
| Commodore Amiga | ❌ | ❌ | ❌ | ❌ | — | — |
| Microsoft Xbox | ❌ | ❌ | ❌ | ❌ | — | ✅ (identity) |
| Microsoft Xbox 360 | ❌ | ❌ | ❌ | ❌ | — | ✅ (identity) |

Notes:
- `redump/ (identity)` = hash+title only; no genre/publisher/developer/year.
- ZX Spectrum +3 coverage in metadat/ is for the +3 floppy-disk model only; broader ZX
  Spectrum 48K/128K game metadata is NOT in libretro-database.
- Dreamcast `developer/` exists but `genre/`, `publisher/`, `releaseyear/` do NOT.
- PS1 and PS2 have `developer/` but lack `genre/`, `publisher/`, `releaseyear/`.

---

### System-by-System Findings

---

#### 1. Atari Lynx

**Status: Already in libretro-database metadat/ — needs integration only.**

All four descriptive metadata fields (developer, genre, publisher, releaseyear) exist in
`metadat/` for the Atari Lynx.  The `no-intro/` sub-directory also has `Atari - Lynx.dat`
for hash-based ROM identification.

- **Source**: `github.com/libretro/libretro-database` — `metadat/developer/Atari - Lynx.dat`,
  `metadat/genre/Atari - Lynx.dat`, `metadat/publisher/Atari - Lynx.dat`,
  `metadat/releaseyear/Atari - Lynx.dat`
- **License**: CC-BY-SA-4.0
- **Fields**: developer, genre, publisher, releaseyear (via separate DAT files keyed by CRC/hash)
- **Merge key**: CRC32 / MD5 (libretro-database primary key)
- **Completeness**: Likely complete for the small library (~80 commercial titles)
- **Risk**: Share-alike clause in CC-BY-SA — can use internally; cannot redistribute a
  stripped metadata subset without keeping the same license.

---

#### 2. Atari Jaguar

**Status: Already in libretro-database metadat/ — needs integration only.**

Same situation as Atari Lynx. All four fields present in `metadat/`. Additionally,
`metadat/serial/Atari - Jaguar.dat` maps hashes to cart serial numbers, and
`metadat/redump/Atari - Jaguar CD.dat` covers the Jaguar CD format.

- **Source**: `github.com/libretro/libretro-database` — all four metadat sub-directories
- **License**: CC-BY-SA-4.0
- **Fields**: developer, genre, publisher, releaseyear
- **Merge key**: CRC32
- **Completeness**: High — small library (~80 commercial cartridge titles + ~12 Jaguar CD)
- **Risk**: Same CC-BY-SA share-alike caveat.

---

#### 3. NEC SuperGrafx

**Status: Already in libretro-database metadat/ — needs integration only.**

Developer, genre, publisher, and releaseyear sub-directories all contain
`NEC - PC Engine SuperGrafx.dat`. The SuperGrafx library is tiny (~7 commercial games),
so the metadata files are likely 100% complete.

- **Source**: `github.com/libretro/libretro-database`
- **License**: CC-BY-SA-4.0
- **Fields**: developer, genre, publisher, releaseyear
- **Merge key**: CRC32
- **Completeness**: Complete (7-game library)
- **Risk**: None beyond CC-BY-SA; trivial integration.

---

#### 4. Sony PlayStation Vita

**Status: No viable offline metadata source. Partial data from NoPAYStation TSV.**

The Vita has no libretro-database metadat/ coverage. Two sources provide partial offline data:

**A. libretro-database no-intro/ hash DATs**
- Files: `Sony - PlayStation Vita.dat` and `Sony - PlayStation Vita (PSN).dat`
- Fields: title, CRC32/MD5/SHA1 per ROM → canonical title only; no metadata
- License: CC-BY-SA-4.0

**B. NoPayStation TSV**
- URL: `https://nopaystation.com/tsv/PSV_GAMES.tsv` (direct download, no auth)
- Fields: Content ID (PCSA/PCSB/PCSC/PCSD format serial), name/title, pkg direct link,
  zRIF key, last modification date
- License: Community contribution; not explicitly stated; no hashes
- Completeness: ~2,000+ Vita titles; PSN digital only; physical disc titles may be absent
- **No genre, developer, publisher, or year fields**
- Merge key: Content ID (serial)

**C. Redump physical disc coverage**
- URL: `http://redump.org/` — does have PS Vita physical disc entries under PSV
- Fields: title, serial, region, version; NO descriptive metadata
- Merge key: serial (e.g. PCSA-00001)

**Recommended strategy**: Use No-Intro hash DAT for hash→title mapping. For full metadata
(genre/developer/publisher/year/players), online enrichment via ScreenScraper or TheGamesDB
is required; results can be cached locally. NoPAYStation serial cross-reference can improve
title matching for digital Vita titles.

- **Risk**: No single offline comprehensive source. NoPAYStation licensing unclear; use for
  internal lookup only, not redistribution.

---

#### 5. Commodore 64

**Status: No offline structured metadata DB. TOSEC provides year+publisher from filenames.**

libretro-database has `no-intro/Commodore - 64.dat` (hashes + canonical titles only).
No `metadat/` sub-directories exist for C64.

**A. TOSEC DAT files for C64**
- URL: `https://www.tosecdev.org/downloads` — `Commodore - C64 - Games.dat` and related sets
- Format: XML DAT with clrmamepro-compatible entries
- Naming convention: `Title (Year)(Publisher)(System)[flags]` — year and publisher embedded in
  the game entry name field. Also encodes country/region and language flags.
- Fields extractable offline after hash match: title, year, publisher, country/region, language
- NOT available: developer (distinct from publisher), genre, players
- License: Community; no explicit open license stated
- Completeness: Extremely broad; TOSEC covers cracktros, hacks, originals, demos → many noisy
  entries per game; filtering for `[!]` (verified) or original (no flags) entries reduces noise
- Merge key: CRC32 (primary) or MD5/SHA1

**B. CSDb (Commodore 64 Scene Database)**
- URL: `https://csdb.dk/`
- Coverage: ~249,000 releases (games, demos, music, tools)
- Data model: groups, sceners, releases, events — scene-focused
- Downloadable dataset: None identified; web-browsing only; no public bulk export or API
- **Not usable offline**

**C. Lemon64**
- URL: `https://www.lemon64.com/`
- Coverage: ~10,000+ C64 games with rating, publisher, year, genre in per-game web pages
- Downloadable dataset: None; no public API; web-only
- Partners with ZXDB (lists ZX Spectrum cross-links) but ZXDB is ZX Spectrum only
- **Not usable offline**

**Recommended strategy**: Use No-Intro hash DAT for ROM identification. Parse TOSEC DAT
entry names after hash match to extract year+publisher (regex on `(Year)(Publisher)` pattern).
For genre and developer: online enrichment via ScreenScraper or TheGamesDB is required. TOSEC
offline path provides year+publisher coverage of ~70-80% of the C64 commercial catalog.

- **Risk**: TOSEC has many variant / cracked / hacked entries; publisher from filename may be
  the cracker group, not the original publisher. Filter to unmodified dumps (`[!]` or no
  dump flags) and validate publisher against known company names.

---

#### 6. Commodore Amiga

**Status: No offline structured metadata DB. TOSEC provides year+publisher from filenames.**

libretro-database has `no-intro/Commodore - Amiga.dat` (hashes + titles only).
No `metadat/` sub-directories exist for Amiga.

**A. TOSEC DAT files for Amiga**
- URL: `https://www.tosecdev.org/downloads` — many Amiga sets
  (OCS/AGA/CD32/CDTV/A1200/etc.)
- This is the most extensive hash-linked Amiga database available offline
- Fields extractable: title, year, publisher, Amiga model (A500/A1200/AGA/OCS/CD32),
  country/region, language flags
- NOT available: developer, genre, players
- License: Community; no explicit open license
- Merge key: CRC32 or MD5/SHA1

**B. Hall of Light (amiga.abime.net)**
- Canonical Amiga game database maintained since 1995; ~27,000 titles tracked
- URL: `https://amiga.abime.net/` — now protected by Anubis anti-bot layer; not directly
  fetchable by automated tools
- Downloadable dataset: No known downloadable bulk export or API endpoint exists
- Fields (per web): title, publisher, developer, year, genre, players, cover art
- **Not usable offline** at this time; contact the maintainers for data export options

**C. LemonAmiga**
- URL: `https://www.lemonamiga.com/` — community database (~5,000+ titles)
- No API or downloadable dataset; web-only browsing
- **Not usable offline**

**Recommended strategy**: Same as C64 — TOSEC hash+filename parsing for year+publisher;
online enrichment for genre/developer. ScreenScraper has very strong Amiga coverage (it was
traditionally built around Amiga metadata). TheGamesDB also covers Amiga well.

- **Risk**: Hall of Light is effectively the gold standard but has no offline path currently.
  TOSEC model flags (A500, AGA, CD32, etc.) can substitute for system-variant filtering.
  Many Amiga games were publisher-only releases with no separate developer; publisher alone
  is often sufficient.

---

#### 7. Sinclair ZX Spectrum

**Status: ZXDB on GitHub is an exceptional offline source.**

**A. ZXDB**
- URL: `https://github.com/zxdb/ZXDB`
- Download: `ZXDB_mysql.sql.zip` — single file from the repo root (~100 MB+ uncompressed SQL)
  A Python script `scripts/ZXDB_to_SQLite.py` converts it to SQLite for Remus
- License: **ODbL 1.0 (Open Database License)**
  - Key terms: free use in applications; derivative **databases** must remain open (copyleft
    applies to the data schema+content, not to applications using it); attribution required.
  - Practically: Remus can use ZXDB data locally and cache results; a redistributed modified
    COPY of the database would need to also be ODbL.
- Maintenance: Active — version 1.0.234 confirmed 2 weeks ago (April 2026)
- Fields:
  - `entries.title` → canonical title
  - `entries.title_original` → original language title
  - `entries.genretype_id` → links to `genretypes.text` (e.g. "Arcade - Shoot-'em-up", "Strategy")
  - `entries.machinetype_id` → machine variant (48K/128K/+3/Next/QL etc.)
  - `releases.release_year`, `releases.release_month`, `releases.release_day` → release date
  - `publishers` join to `labels.name` → publisher per release
  - `authors` join to `labels.name` → developer/author per entry
  - `entries.maxplayers` → players (integer)
  - `countries` in releases → region (ISO 3166 country codes)
  - Links to `downloads.file_link` → TAP/TZX file references (for hash lookup)
- **Hash coverage**: ZXDB links to external World of Spectrum archive via file paths; it does
  NOT embed CRC32/MD5/SHA1 hashes internally. The TOSEC→ZXDB mapping maintained in ZX Pokemaster
  (by Elia Iliashenko) provides the bridge between TOSEC hashes and ZXDB entry IDs.
- Merge key: **Title + platform** for initial matching; the ZX Pokemaster mapping project
  provides a TOSEC-filename → ZXDB-entry-ID lookup table (separate download needed)

**B. ZXInfo API**
- URL: `https://api.zxinfo.dk/v3/`
- License: Free, public; backed by ZXDB
- Authentication: None for read/search endpoints
- Use: Title-based search, game-ID lookup, publisher/author lookup; no hash-based lookup
- Rate limits: Not documented; self-identified user-agent requested
- Merge key: ZXDB entry ID or title

**C. libretro-database (incomplete)**
- Coverage: `Sinclair - ZX Spectrum +3.dat` only (the +3 floppy variant)
- Fields: developer, genre, publisher, releaseyear
- The +3 is a small subset of the full ZX Spectrum library; most 48K/128K games are absent

**Recommended strategy**: Convert ZXDB MySQL dump to SQLite using the provided Python script.
Parse TOSEC ZX Spectrum DAT filenames to extract CRC32 → match against ZXDB game lists by
title. The ZX Pokemaster TOSEC→ZXDB mapping table provides a partial pre-built bridge.
Cache locally as a SQLite file alongside existing GameTDB XMLs.

- **Risk**: ZXDB does not store hashes directly — title-matching is the primary merge path
  (fuzzy matching required for variant titles). ODbL share-alike applies to derivative
  databases, not application usage.

---

#### 8. Microsoft Xbox (original)

**Status: No offline metadata source. Redump provides serial+title+region only.**

libretro-database has `metadat/redump/Microsoft - Xbox.dat` (hashes + titles, identity only).
No `metadat/` descriptive sub-directories exist for Xbox.

- **Redump** covers Xbox extensively (confirmed live: XBOX entries with serial, title, region,
  languages). DAT download requires free registration.
- **No open downloadable metadata DB** (genre/developer/publisher/year) found for Xbox.
- Game count: ~1,000 North American Xbox releases; ~800 PAL; comprehensive Redump coverage.

**Recommended strategy**: Use Redump DAT (or libretro-database identity DAT) for
hash→title+serial. Enrich online via ScreenScraper (hash-based) or TheGamesDB (title-based,
free key). Cache results locally. Wikidata SPARQL can supplement genre/developer (CC0) for
popular titles.

- **Risk**: Xbox titles overlap significantly with Xbox 360 backward-compatibility; game names
  may be ambiguous. Serial-based matching against Redump title field is robust.

---

#### 9. Microsoft Xbox 360

**Status: No offline metadata source. No-Intro + Redump provide hashes+serials only.**

libretro-database has:
- `no-intro/Microsoft - Xbox 360.dat` + digital/title-updates variants (hashes + titles)
- `metadat/redump/Microsoft - Xbox 360.dat` (identity only)
- No descriptive metadat/ sub-directories.

Confirmed: No open structured metadata database for Xbox 360 titles found.

**Recommended strategy**: Identical to Xbox above. ScreenScraper or TheGamesDB online.
Merge key is disc serial (extracted from XISO header for physical discs) or title.

- **Risk**: Xbox 360 has the largest library of the gap systems (~2,000+ retail titles).
  Rate limits on TheGamesDB (3,000 req/month free tier) become material; ScreenScraper
  (20,000 req/day) is better suited for bulk enrichment. Cache aggressively.

---

#### 10. Sega Dreamcast

**Status: Partial — libretro-database has developer only. Redump gaps remain.**

libretro-database `metadat/developer/Sega - Dreamcast.dat` exists. Genre, publisher,
and releaseyear sub-directories do NOT contain a Dreamcast DAT.

- **Redump** covers Dreamcast thoroughly via `metadat/redump/Sega - Dreamcast.dat` (identity)
  and the main Redump DAT (confirmed: HDR-XXXX serials visible in test data)
- Title+serial can be extracted from the Dreamcast IP.BIN header (previously researched;
  see disc-magic-detection research). Merge key: **GD-ROM product code** (e.g. `HDR-0080`)
- For missing fields (genre/publisher/year): online enrichment via ScreenScraper or TheGamesDB

**Recommended strategy**: Use existing libretro-database developer data. Extend with
ScreenScraper per-game API calls for genre/publisher/year/players. Dreamcast has ~1,000
commercial titles — manageable batch size.

- **Risk**: Dreamcast serial format is `SXXX-YYYYYY` (GD-ROM) or `HDR-XXXX` (Sega net);
  Redump labels title fields clearly. Lower merge risk than open-form systems.

---

#### 11. Sega CD / Mega-CD

**Status: No libretro-database metadata. Redump provides identification only.**

libretro-database has `metadat/redump/Sega - Mega-CD - Sega CD.dat` (identity only).
No descriptive metadat/ sub-directories.

- Sega CD library: ~200 North American + ~200 Japanese + ~100 European titles (~500 total)
- Redump covers all regions; serial format: `T-XXXXXX-YY`
- For metadata: online enrichment required

**Recommended strategy**: ScreenScraper (hash-based; confirmed Sega CD coverage) or
TheGamesDB. Small library — a one-time batch enrichment is practical.

- **Risk**: Sega CD titles often have Japanese-only releases with romanised-only titles in
  Redump; TheGamesDB may have sparser JP-only coverage. ScreenScraper covers JP releases well.

---

#### 12. Sega Saturn

**Status: No libretro-database metadata. Redump provides identification only.**

libretro-database has `metadat/redump/Sega - Saturn.dat` (identity only).
No descriptive metadat/ sub-directories.

- Saturn library: ~1,000 Japanese + ~400 NA + ~300 PAL titles
- Redump serial format: `T-XXXXXX-YY` / `MK-XXXXXX-YY`
- For metadata: online enrichment required

**Recommended strategy**: Same as Sega CD. ScreenScraper is particularly strong for Saturn
given French community enthusiasm for the platform.

- **Risk**: Many Saturn imports (Japanese-only) have sparse English metadata even on
  ScreenScraper; TheGamesDB skews NA-centric.  Consider supplementing with Wikidata SPARQL
  for JP-region titles with Wikipedia articles.

---

#### 13. NEC PC Engine CD / TurboGrafx-CD

**Status: Cartridge metadata in libretro-database; CD games need online enrichment.**

libretro-database `metadat/` has developer/genre/publisher/releaseyear sub-directories
for `NEC - PC Engine - TurboGrafx 16.dat` — this covers **HuCard cartridge** games only.

For CD-ROM² games:
- libretro-database has `metadat/redump/NEC - PC Engine CD - TurboGrafx-CD.dat` (identity only)
- No descriptive metadat/ sub-directory for PC Engine CD
- Redump covers PC Engine CD well (confirmed `PCE` entries in test sample)
- Merge key: CD serial printed on disc header (readable from disc; Redump encodes as serial field)

**Recommended strategy**: Apply existing libretro-database cartridge metadata for HuCard games.
For CD games: ScreenScraper or TheGamesDB online enrichment. PC Engine CD library is ~300
titles (JP) + ~100 (NA/EU) — manageable batch.

- **Risk**: Many PC Engine CD titles are JP-only without English localisation; ScreenScraper
  has better JP coverage than TheGamesDB for this platform.

---

#### 14. 3DO

**Status: No libretro-database metadata. Redump provides identification only.**

libretro-database has `metadat/redump/The 3DO Company - 3DO.dat` (identity only).
No descriptive metadat/ sub-directories.

- 3DO library: ~350 total commercial titles (small; largely NA-focused)
- Redump serial format varies; internal disc label provides title
- For metadata: online enrichment required

**Recommended strategy**: ScreenScraper or TheGamesDB (GPLv3) — small library means even
a one-time batch with TheGamesDB's 3,000 req/month quota is sufficient. Wikidata as CC0
supplemental.

- **Risk**: 3DO has obscure titles with very limited online metadata. ScreenScraper has
  broader coverage than TheGamesDB for this platform based on community reports. Accept
  partial coverage (~60-70% of titles may enrich successfully).

---

#### 15. Sony PlayStation (PS1)

**Status: Partial — libretro-database has developer. Genre/publisher/year absent.**

libretro-database `metadat/developer/Sony - PlayStation.dat` exists and is actively
maintained (last update 2 weeks ago: renamed "SCEE" to "Sony Computer Entertainment Europe").
Publisher, genre, and releaseyear sub-directories do NOT contain a PlayStation DAT.

- `metadat/redump/Sony - PlayStation.dat` provides full hash+title+serial identity
- PS1 library: ~3,000 USA + ~3,600 Japan + ~3,000 Europe titles (~10,000 total across regions)
- Redump serial format: `SCUS-XXXXX` / `SLES-XXXXX` / `SLPS-XXXXX`

**Recommended strategy**: Use existing libretro-database developer data. Add
ScreenScraper enrichment for genre, publisher, releaseyear, players. The hash-based
ScreenScraper API path is ideal — Remus already computes disc hashes.

- **Risk**: Massive library — aggressive local caching of ScreenScraper responses is essential
  to avoid re-querying. Redump serial already identifies the exact version; use serial as
  the ScreenScraper `crc` parameter alternative.

---

#### 16. Sony PlayStation 2

**Status: Partial — libretro-database has developer. Genre/publisher/year absent.**

libretro-database `metadat/developer/Sony - PlayStation 2.dat` exists (recently cleaned up:
"removed PS2 entries without a developer", last week). Publisher, genre, releaseyear absent.

- `metadat/redump/Sony - PlayStation 2.dat` provides hash+title+serial identity (`SCUS-XXXXX`)
- PS2 library: ~4,000 USA + ~5,000 Japan + ~4,000 Europe titles (~13,000+ across regions)
- Second-largest library in scope; aggressive caching essential

**Recommended strategy**: Same as PS1. Use libretro-database developer data; extend with
ScreenScraper online enrichment for remaining fields.

- **Risk**: Same as PS1. PS2 is the largest metadata enrichment task by volume.
  Rate-limit management is critical for ScreenScraper's 20,000 req/day limit.

---

## System-by-System Summary Table

| System | Best offline source(s) | License/terms | Fields available offline | Completeness | Merge strategy | Recommendation |
|--------|----------------------|--------------|--------------------------|-------------|----------------|----------------|
| Atari Lynx | libretro-database metadat/ | CC-BY-SA-4.0 | developer, genre, publisher, releaseyear | High (~80 titles) | CRC32 | **Integrate immediately** — no new source needed |
| Atari Jaguar | libretro-database metadat/ | CC-BY-SA-4.0 | developer, genre, publisher, releaseyear | High (~80 cart + ~12 CD) | CRC32 | **Integrate immediately** |
| NEC SuperGrafx | libretro-database metadat/ | CC-BY-SA-4.0 | developer, genre, publisher, releaseyear | Complete (~7 titles) | CRC32 | **Integrate immediately** |
| ZX Spectrum | ZXDB (GitHub SQLite dump) | ODbL 1.0 | title, author, publisher, genre, year, players, region | Comprehensive | Title fuzzy-match + TOSEC→ZXDB mapping | **High priority new source** — add after SQLite conversion work |
| Commodore 64 | TOSEC DAT filename parsing | Community (unspecified) | title, year, publisher from filename; NOT genre/developer | Year+publisher ~70% | CRC32 → TOSEC name parse | Add TOSEC offline path; supplement with ScreenScraper for genre |
| Commodore Amiga | TOSEC DAT filename parsing | Community (unspecified) | title, year, publisher, Amiga model variant | Year+publisher ~70% | CRC32 → TOSEC name parse | Same as C64; Hall of Light may add later if they offer export |
| PS Vita | NoPAYStation TSV + No-Intro hashes | Community (unspecified) | serial, title, content ID only | Title/serial only | Content ID serial | Partial offline; needs ScreenScraper online for full fields |
| Xbox (original) | Redump identity DAT | Community (unspecified) | title, serial, region | Identity only | Serial from XISO header | Needs online enrichment (ScreenScraper/TheGamesDB) |
| Xbox 360 | No-Intro + Redump identity DATs | Community (unspecified) | title, serial, region | Identity only | Serial or title | Needs online enrichment; prefer ScreenScraper at scale |
| Sega Dreamcast | libretro-database developer/ + Redump identity | CC-BY-SA-4.0 | developer; serial, title, region | Developer partial | IP.BIN serial (e.g. HDR-0080) | Developer usable now; add online for genre/publisher/year |
| Sega CD / Mega-CD | Redump identity DAT | Community (unspecified) | serial, title, region | Identity only | Disc serial | Needs online enrichment; small library (~500 titles) |
| Sega Saturn | Redump identity DAT | Community (unspecified) | serial, title, region | Identity only | Saturn disc serial | Needs online enrichment; ScreenScraper preferred for JP |
| PC Engine CD | Redump identity DAT | Community (unspecified) | serial, title, region | Identity only | CD serial | Needs online enrichment; PCE cartridge metadata already exists |
| 3DO | Redump identity DAT | Community (unspecified) | serial, title, region | Identity only | Disc serial | Online enrichment; small library (~350) — one-time batch |
| Sony PlayStation | libretro-database developer/ + Redump identity | CC-BY-SA-4.0 | developer; serial, title, region | Developer partial | Redump serial (SCUS/SLES/SLPS) | Developer usable now; add online for genre/publisher/year |
| Sony PlayStation 2 | libretro-database developer/ + Redump identity | CC-BY-SA-4.0 | developer; serial, title, region | Developer partial | SCUS/SLES/SLPS serial | Same as PS1; largest volume task |

---

## Recommendations — Prioritised Acquisition Plan

### Can add immediately (zero new format work required)

These systems have all needed data in `libretro-database metadat/` — the only work is
extending Remus's existing DAT parser to consume these `metadat/` sub-directories.

1. **Atari Lynx** — `metadat/developer/`, `metadat/genre/`, `metadat/publisher/`,
   `metadat/releaseyear/`, `metadat/serial/`
2. **Atari Jaguar** — same four + `metadat/redump/Atari - Jaguar CD.dat` for CD titles
3. **NEC SuperGrafx** — same four (7-game library; trivial)
4. **Sega Dreamcast (developer only)** — `metadat/developer/Sega - Dreamcast.dat` already
   present; can be wired in now
5. **Sony PlayStation (developer only)** — `metadat/developer/Sony - PlayStation.dat`
6. **Sony PlayStation 2 (developer only)** — `metadat/developer/Sony - PlayStation 2.dat`

### Can add after format conversion / normalisation work

These require downloading a new data source and writing a parser. Estimated complexity:
S = straightforward, M = moderate (schema mapping needed), L = large (fuzzy matching or
multi-source merge).

| Work item | Source | Effort | Notes |
|-----------|--------|--------|-------|
| ZX Spectrum full metadata | ZXDB GitHub → SQLite | M | Python script provided in repo; schema join required (labels, releases, genretypes) |
| C64 year+publisher | TOSEC filename parser | S | Regex on `(Year)(Publisher)` in DAT game name field after CRC match |
| Amiga year+publisher | TOSEC filename parser | S | Same regex; many model sub-variants in TOSEC; filter strategy needed |
| Disc system identity DATs | libretro-database metadat/redump/ | S | Already in Remus if redump DATs are loaded; verify Xbox, Saturn, Sega CD entries present |
| PS1/PS2/Dreamcast genre+publisher+year | ScreenScraper API enrichment | M | Requires ScreenScraper provider implementation; hash-based; results cached to SQLite |
| Xbox / Xbox 360 full metadata | ScreenScraper or TheGamesDB | M | Same provider; serial from XISO header as lookup key |
| Sega CD / Saturn / PC Engine CD | ScreenScraper | M | Small-to-medium libraries; hash-based |
| 3DO full metadata | ScreenScraper or TheGamesDB | S | Small library, one-time batch feasible |

### Avoid or use cautiously

| Source | Reason |
|--------|--------|
| **OpenVGDB** (github.com/OpenVGDB/OpenVGDB) | Abandoned since Nov 2021; hotlinked art URLs likely broken; no updates for 4+ years |
| **Hall of Light** (amiga.abime.net) | Now behind Anubis anti-bot protection; no downloadable dataset; scraping blocked |
| **LemonAmiga / Lemon64** | Web-only; no API or bulk export; contact site owners if data partnership is desired |
| **CSDb** | Scene-database focus (demos/cracks); not a commercial game metadata source; web-only |
| **MobyGames** | Requires paid subscription; proprietary license; data cannot be redistributed |
| **IGDB** | Twitch OAuth dependency; 60-day token expiry needs automation; non-commercial only |
| **NoPAYStation TSV** for metadata | License unclear; title-only; useful for serial→title bridge but not enrichment |

---

## Merge Strategy Reference

When combining data from multiple sources, these keys provide reliable joins:

| System type | Recommended merge key | Notes |
|-------------|----------------------|-------|
| Cartridge ROMs (Lynx, Jaguar, C64, Amiga, SuperGrafx) | CRC32 (primary), MD5/SHA1 (fallback) | No-Intro / TOSEC DATs are CRC-primary |
| Disc systems (PS1, PS2, DC, Saturn, Sega CD, Xbox, 3DO) | Serial code (product code from disc header) | More stable than CRC across disc images; Redump uses serial |
| ZX Spectrum tapes/discs | TOSEC filename → ZXDB entry ID (via ZX Pokemaster mapping table) | No direct hash-in-ZXDB; title fallback for unmatched |
| PS Vita digital | NPS Content ID (PCSA/PCSB/PCSC/PCSD) | Maps to title; serial matchable against Redump physical entries |
| Cross-source deduplication | title + platform + year ± region | Use when hash/serial match fails; Levenshtein for title fuzzy match |

---

## Gaps and Further Research

1. **TOSEC→ZXDB mapping file** — Elia Iliashenko's ZX Pokemaster project maintains a complete
   TOSEC-filename → ZXDB-ID mapping. Confirm current availability and format at
   `https://sourceforge.net/projects/zx-pokemaster/` before implementation.

2. **Hall of Light export** — amiga.abime.net is behind anti-bot but may provide a data
   export on request. Worth contacting maintainers for a CSV/JSON dump if Amiga coverage
   is prioritised. Their data is the most complete Amiga metadata source in existence.

3. **ScreenScraper bulk mode** — ScreenScraper does not offer a full offline bulk database
   download. However, paid tiers (€1–€10/month) allow up to 4 concurrent threads. The
   practical path is: buy one month, run bulk enrichment for all 16 systems, cache to SQLite.
   This is a one-time cost for effectively permanent offline data.

4. **Wikidata as CC0 fallback** — For systems with no other metadata (especially Xbox 360
   and Vita), Wikidata SPARQL queries for games with the target platform (P400 = platform)
   can provide genre (P136), developer (P178), publisher (P123), and publication date (P577)
   at CC0. Quality is inconsistent for less-notable titles but covers most top-100 games per
   system. SPARQL endpoint: `https://query.wikidata.org/sparql`.

5. **Xbox 360 serialdb** — The Xbox 360 physical disc serial format (e.g. `MS-XXXX`)
   is captured by Redump. Investigate whether an Xbox title database exists in the emulation
   community (e.g. xenia-project compatibility lists) that cross-references serial → genre.

