# Research: Free and Open-Source Retro Game Metadata Sources

> Date: 2026-03-26 | Agent: Researcher | Status: complete

## Summary

This report catalogues every major free, community, and open-source source of retro game metadata usable by a ROM library manager. Sources are grouped into six categories: ROM hash databases, game metadata APIs, artwork/media repositories, community databases, naming standards/tools, and open-source scrapers. For each source the report records its URL, what data it provides, how to access it, authentication requirements, licensing, platform coverage, data format, and relevance to the Remus project. The highest-priority candidates for filling Remus's current metadata gaps (empty `desc`, `genre`, `players`, `region` fields in EmulationStation/CSV/JSON exports) without requiring a paid key are: **libretro-database** (no auth, CC-BY-SA-4.0, includes genre/developer), **ScreenScraper** (free registration, CC-BY-NC-SA, richest retro metadata), **GameTDB** (no signup, free XML download, Nintendo-platform focused), and **TheGamesDB** (free API key, GPLv3, includes players count).

---

## Sources

| URL | Relevance |
|-----|-----------|
| https://datomatic.no-intro.org/ | No-Intro ROM hash database |
| https://www.tosecdev.org/downloads | TOSEC ROM hash database |
| http://redump.org/ | Redump disc hash database |
| https://github.com/libretro/libretro-database | libretro ROM+metadata database |
| https://github.com/libretro/libretro-thumbnails | libretro artwork |
| https://github.com/OpenVGDB/OpenVGDB | OpenVGDB SQLite database |
| https://www.mobygames.com/api/ | MobyGames metadata API |
| https://api.thegamesdb.net/ | TheGamesDB metadata API |
| https://api-docs.igdb.com/ | IGDB metadata API |
| https://www.screenscraper.fr/ | ScreenScraper metadata + artwork |
| https://www.gametdb.com/ | GameTDB Nintendo metadata + art |
| https://www.wikidata.org/wiki/Wikidata:WikiProject_Video_games | Wikidata CC0 game data |
| http://adb.arcadeitalia.net/ | ArcadeDB MAME arcade metadata |
| https://github.com/Gemba/skyscraper | Gemba Skyscraper (active fork) |
| https://gamesdb.launchbox-app.com/ | LaunchBox Games Database (no API) |

---

## Findings

### Category A — ROM Hash Databases

These sources provide checksums (CRC32/MD5/SHA1) for verifying and identifying ROMs. They are the foundation of hash-first identification.

---

#### A1. No-Intro / DAT-o-MATIC

- **URL**: https://datomatic.no-intro.org/
- **Data available**: CRC32, MD5, SHA1 per ROM; filename (includes region/revision flags); dump status
- **Access method**: Web search or file download (ZIP archives per platform)
- **Authentication**: None required for search and download
- **License**: Not explicitly stated; community preservation project; for verification/identification use
- **Platform coverage**: ~150+ non-disc platforms (cartridge-based: NES, SNES, GBA, GBC, N64, Genesis, Game Boy, etc.)
- **Data format**: XML DAT (clrmamepro compatible)
- **Actively maintained**: Yes — daily/weekly entries added (observed Xbox 360 DLC, SMD, NES entries live)
- **ROM hashes**: ✅ Yes — primary purpose
- **Artwork**: ❌ No
- **Notes for Remus**: The gold standard for cartridge ROM verification. Already consumed indirectly via Hasheous. No API; batch download is the integration path. Filenames encode region (`(USA)`, `(Europe)`, `(Japan)`) and can be parsed for the `region` field.

---

#### A2. TOSEC

- **URL**: https://www.tosecdev.org/downloads
- **Data available**: CRC32, MD5, SHA1; filename with extensive flags; publisher, year in filename
- **Access method**: Free ZIP download (full pack or platform packs)
- **Authentication**: None
- **License**: Community; not explicitly stated
- **Platform coverage**: Complete — 4,245 DATs, 1,294,449 ROMs, all platforms including disc; also covers PIX (images), docs, and demo DATs
- **Data format**: XML DAT (clrmamepro compatible)
- **Actively maintained**: Yes — last release 2025-03-13
- **ROM hashes**: ✅ Yes
- **Artwork**: ❌ No (PIX DATs reference filenames of scanned images)
- **Notes for Remus**: Broadest coverage; fills gaps where No-Intro has no set. Already sourced by Hasheous.

---

#### A3. Redump

- **URL**: http://redump.org/
- **Data available**: CRC32, MD5, SHA1 + sector-level hashes; dump region visible in entries; platform
- **Access method**: Web search (no auth) + DAT file downloads (registration required for bulk downloads)
- **Authentication**: Free registration required for DAT downloads
- **License**: Community preservation; not explicitly licensed
- **Platform coverage**: Disc-based platforms only: PlayStation 1/2/3, GameCube, Wii, Saturn, Dreamcast, Xbox/360, PC, etc.
- **Data format**: XML DAT (clrmamepro compatible)
- **Actively maintained**: Yes — entries added daily (Mar 26 2026 entries confirmed)
- **ROM hashes**: ✅ Yes
- **Artwork**: ❌ No
- **Notes for Remus**: The gold standard for disc-based hash verification. Already sourced by Hasheous. Region information embedded in entry filenames.

---

#### A4. libretro-database

- **URL**: https://github.com/libretro/libretro-database
- **Data available**: CRC32, MD5, SHA1; additionally: developer, publisher, genre, description, rating, players in separate metadata DATs
- **Access method**: `git clone` or download ZIP from GitHub releases
- **Authentication**: None
- **License**: CC-BY-SA-4.0
- **Platform coverage**: 80+ systems; sources aggregated from No-Intro, Redump, TOSEC, MAME, GameTDB
- **Data format**: clrmamepro DAT (`.dat`) + compiled binary RDB (`.rdb`)
- **Actively maintained**: Yes
- **ROM hashes**: ✅ Yes
- **Artwork**: ❌ No (sibling `libretro-thumbnails` repo provides art)
- **Notes for Remus**: Best single-file source for combining hash lookup AND metadata (genre, developer). The separate metadata DATs contain `description`, `genre`, `developer`, `publisher`, `players` fields. Directly actionable: parse the game-specific `.dat` files for extra metadata once a hash match is found. CC-BY-SA-4.0 requires share-alike for redistributed data.

---

#### A5. OpenVGDB

- **URL**: https://github.com/OpenVGDB/OpenVGDB
- **Data available**: CRC32/MD5, title, description, genre, publisher, developer, release date, region, US box art URLs
- **Access method**: GitHub releases — single SQLite ZIP download (~8.7 MB)
- **Authentication**: None
- **License**: No explicit license declared
- **Platform coverage**: Major retro platforms; primarily NTSC-U/US-centric
- **Data format**: SQLite database
- **Actively maintained**: ❌ **Last release v29.0 November 2021; effectively abandoned** (1 contributor, wiki empty)
- **ROM hashes**: ✅ Yes (CRC32/MD5)
- **Artwork**: ⚠️ Box art URLs only (hotlinks may break)
- **Notes for Remus**: Historically useful but **stale since 2021**. Art URL hotlinks may no longer work. Not recommended as a primary source; use only as a tertiary fallback.

---

#### A6. MAME XML

- **URL**: https://www.mamedev.org/
- **Data available**: SHA1 + CRC32 per ROM set; manufacturer, year, full title, emulation status for all ~40,000+ arcade sets
- **Access method**: Built into every MAME release: `mame -listxml > mame.xml`
- **Authentication**: None
- **License**: GPL/BSD (MAME itself)
- **Platform coverage**: Arcade only
- **Data format**: XML
- **Actively maintained**: Yes — monthly MAME releases
- **ROM hashes**: ✅ Yes
- **Artwork**: ❌ No
- **Notes for Remus**: Definitive source for arcade ROM identification. Already sourced by libretro-database and Hasheous. Can extract manufacturer and year as publisher/date metadata for arcade titles.

---

### Category B — Game Metadata APIs

These sources provide rich textual metadata: titles, descriptions, genres, player counts, and release dates.

---

#### B1. TheGamesDB

- **URL**: https://api.thegamesdb.net/ (Swagger UI)
- **Data available**: Title, description, developer, publisher, genres, `players` (numeric), release dates, box art, screenshots, fanart, banners
- **Access method**: REST API v2 (JSON), GET requests
- **Authentication**: 🔑 Free API key required (registration at thegamesdb.net)
- **License**: GPLv3 for data
- **Platform coverage**: 80+ platforms, strong retro coverage
- **Rate limits**: 3,000 requests/month per IP (free tier)
- **Data format**: JSON REST
- **Actively maintained**: Yes
- **ROM hashes**: ❌ No
- **Artwork**: ✅ Yes (box art, screenshots, fanart, banners — hosted CDN)
- **Notes for Remus**: Already integrated. The `players` field directly maps to EmulationStation's `<players>`. GPLv3 data license is compatible with Remus's use. The low monthly quota (3,000/IP) is a constraint for large library enrichment; consider caching aggressively.

---

#### B2. IGDB

- **URL**: https://api-docs.igdb.com/
- **Data available**: Title, summary/storyline, genres, themes, game modes, multiplayer modes (`offlinemax`/`onlinemax` for player counts), release dates (per region), developers, publishers, cover art, screenshots, artworks, age ratings; region data via `game_localizations` endpoint
- **Access method**: REST API v4 with Apicalypse (POST, application/text body)
- **Authentication**: 🔑 Free Twitch OAuth required — register app at dev.twitch.tv, get client-id + secret, exchange for bearer token
- **License**: Free for non-commercial use (Twitch Developer Services Agreement); commercial needs partnership at partner@igdb.com
- **Rate limits**: 4 req/sec; up to 8 concurrent; token expires in ~60 days (auto-refresh needed)
- **Data format**: JSON REST
- **Actively maintained**: Yes (Twitch/IGDB)
- **ROM hashes**: ❌ No
- **Artwork**: ✅ Yes (covers, screenshots, artworks — via image CDN: `images.igdb.com/igdb/image/upload/t_{size}/{image_id}.jpg`)
- **Notes for Remus**: Already integrated. Use `multiplayer_modes.offlinemax` for the players field. For region, query the `game_localizations` endpoint with `where region = {1,2,5}` (Europe/NA/Japan). The `release_dates` endpoint has per-region data. Token rotation must be automated (60-day expiry).

---

#### B3. MobyGames

- **URL**: https://www.mobygames.com/api/
- **Data available**: Title, description/overview, genres, publisher, developer, release dates, screenshots, cover art, moby score; very deep retro catalog
- **Access method**: REST API v1 (JSON), GET requests
- **Authentication**: 🔑 Paid API key required — hobbyist tier available at mobygames.com/api/subscribe/
- **License**: Proprietary (MobyGames owns data); non-commercial use with attribution
- **Rate limits**: 720 req/hr (hobbyist tier)
- **Data format**: JSON REST
- **Actively maintained**: Yes
- **ROM hashes**: ❌ No
- **Artwork**: ✅ Yes (screenshots, cover scans)
- **Notes for Remus**: The richest metadata source for obscure retro titles but **requires a paid subscription**. The hobbyist tier is affordable. Worth considering once free sources are exhausted; excellent descriptions and genre depth. Proprietary license means data cannot be redistributed or stored in a public database.

---

#### B4. Wikidata

- **URL**: https://www.wikidata.org/wiki/Wikidata:WikiProject_Video_games
- **Data access**: https://www.wikidata.org/wiki/Wikidata:Data_access
- **Data available**: Title, description (Wikipedia excerpt), genre, publisher, developer, release date, platforms; cross-reference IDs for 100+ other databases (IGDB ID, RetroAchievements ID, GameTDB ID, etc.)
- **Access method**: SPARQL query service (https://query.wikidata.org/) or REST API (`wikidata.org/api/rest_v1/`)
- **Authentication**: ✅ None — fully public, no API key
- **License**: **CC0 (public domain)** — most permissive possible
- **Coverage**: Community-maintained; variable quality; ambitious project to be "the hub for all video game metadata"
- **Data format**: JSON/XML via REST or SPARQL
- **Actively maintained**: Yes (community)
- **ROM hashes**: ❌ No
- **Artwork**: ❌ No (Wikimedia Commons has some logo images; different API)
- **Notes for Remus**: The CC0 license is unique — data can be embedded in Remus without any attribution or share-alike obligation. Quality is inconsistent; use as supplemental/fallback. The cross-reference ID properties are especially valuable for resolving the same game across different databases. Example SPARQL: `SELECT ?item ?name WHERE { ?item wdt:P31 wd:Q7889; rdfs:label ?name. FILTER(LANG(?name)="en") }` for all games.

---

### Category C — Artwork and Media

---

#### C1. libretro-thumbnails

- **URL**: https://github.com/libretro/libretro-thumbnails
- **Data available**: Box art (`Named_Boxarts`), title screens (`Named_Titles`), gameplay screenshots (`Named_Snaps`)
- **Access method**: `git clone --recurse-submodules` (full repo) **or** direct HTTP URL access via CDN: `http://thumbnails.libretro.com/{System}/Named_Boxarts/{GameName}.png`
- **Authentication**: ✅ None
- **License**: Mixed (sources from community and MobyGames); effectively permissive per-platform
- **Platform coverage**: 80+ systems; naming matches libretro-database game names exactly
- **Data format**: PNG files organised as `{System}/{Type}/{GameName}.png`
- **Actively maintained**: Yes — updated every ~2 days
- **Notes for Remus**: Best paired with a libretro-database hash match. After resolving a ROM's canonical name via libretro-database, construct the thumbnail URL directly (no API call needed). Works for offline use too if cloned. URL-encoding game names containing special characters is required.

---

#### C2. ScreenScraper

- **URL**: https://www.screenscraper.fr/
- **Data available**: Box art (2D/3D), screenshots, wheel/logo art, marquee, fanart, manual PDFs, video snaps; **metadata**: title, description, genre, players, publisher, developer, release date, region, rating
- **Access method**: REST API (username+password in query params); hash-based lookup (MD5/SHA1/CRC32) + filename fallback
- **Authentication**: ⚠️ Free registration required (no paid plan needed for basic use)
- **License**: CC-BY-NC-SA 4.0 (non-commercial; share-alike; attribution required)
- **Rate limits**: 20,000 requests/day (registered), 1 thread concurrent (free); paid tiers (€1–€10/month) unlock 2–4 threads
- **Platform coverage**: Comprehensive retro + modern; ~815k registered members; disc and cartridge
- **Data format**: JSON or XML REST
- **Actively maintained**: Yes — 42 million API accesses the day of research
- **Notes for Remus**: **One of the highest-quality free sources** for retro metadata + artwork. The hash-based lookup means Remus can pass the already-computed CRC/MD5/SHA1 and receive full enrichment. CC-BY-NC-SA restricts commercial redistribution of the data but is fine for a personal tool. The `--enrich` flow described in `cli-integration-improvements.md` maps directly to this API. Skyscraper (Gemba fork) can serve as an implementation reference for the API call structure. API endpoint: `https://www.screenscraper.fr/api2/jeuInfos.php`.

---

#### C3. GameTDB

- **URL**: https://www.gametdb.com/
- **Artwork CDN**: `https://art.gametdb.com/{platform}/{artType}/{region}/{gameID}.{ext}`
- **Data available**: Cover art (2D/3D/HQ full scan/back/disc/cart/box), XML database with: title (per-language), synopsis (per-language), publisher, developer, genre, players, region, multiple rating systems (ESRB/PEGI/CERO/GRB), content descriptors, online features, input accessories, checksums (CRC32/MD5/SHA1), release date
- **Access method**: Direct ZIP download (no signup): e.g. https://www.gametdb.com/wiitdb.zip for Wii; art via CDN URL with game ID
- **Authentication**: ✅ None for downloads and CDN art
- **License**: Community-contributed; FAQ explicitly authorises software use with attribution + contact request; website embedding requires explicit permission; no CC license stated; underlying artwork owned by respective publishers
- **Platform coverage**: Wii + GameCube (`wiitdb.zip`), Wii U (`wiiutdb.zip`), 3DS (`3dstdb.zip`), DS (`dstdb.zip`), Switch (`switchtdb.zip`), PS3 (`ps3tdb.zip`). **No separate `gamecubetdb.zip`** — GameCube is bundled inside `wiitdb.zip`.
- **Data format**: ZIP archive containing XML database (`wiitdb.xml` etc.); UTF-8 encoded
- **Actively maintained**: Yes — art updated weekly; database updated continuously by community
- **Notes for Remus**: **Best free source for Nintendo/PS3 metadata** — no API key, no scraping, bulk download and parse. Full per-language titles and descriptions across 12 locales. Art CDN requires no auth. See dedicated deep-dive in [gametdb-2026-03-26.md](gametdb-2026-03-26.md).

---

### Category D — Community Identification Databases---

#### D1. RetroAchievements

- **URL**: https://retroachievements.org/
- **API base**: https://api.retroachievements.org/
- **Data available**: Game names, per-system hash tables (RA-specific hash format), achievement data, mastery counts, developer credits
- **Access method**: REST API (JSON); game lookup by MD5/hash
- **Authentication**: 🔑 Free registration + API key from account settings
- **License**: Community data; CC-BY (implied)
- **Platform coverage**: ~17,000+ supported games across popular retro platforms
- **Data format**: JSON REST
- **Actively maintained**: Yes
- **ROM hashes**: ✅ Yes (RA-specific hash format; documented at docs.retroachievements.org/developer-docs/game-identification.html)
- **Notes for Remus**: RA uses custom per-system hashing algorithms (documented, already in RESEARCH.md). Useful for identifying specific versions/revisions that other databases miss. API key required but free. The API blocked direct fetching during this research (403/404); consult the RA developer docs for the correct API endpoint structure. Useful as a secondary identifier for cross-referencing.

---

### Category E — Naming Standards and DAT Tools

---

#### E1. No-Intro Naming Convention

- **URL**: https://wiki.no-intro.org/index.php?title=Naming_Convention (already in RESEARCH.md)
- **Summary**: `Title (Region) (Version) [Flags]` — canonical format for cartridge ROMs; region codes: (USA) (Europe) (Japan) (World); `[!]` = verified good dump
- **Relevance for Remus**: Region can be parsed from No-Intro filenames with high confidence using the parenthetical region codes.

#### E2. TOSEC Naming Convention

- **URL**: https://www.tosecdev.org/tosec-naming-convention (already in RESEARCH.md)
- **Summary**: `Title (date)(publisher)(system)(video)(country)(language)(copyright)(devstatus)(media type)(media label)[cr][f][h][m][p][t][tr][b][a][!][overdump][alias][u]`
- **Relevance for Remus**: Country/region embedded in filename; can be parsed for `region` field.

#### E3. Redump Naming Convention

- **URL**: http://wiki.redump.org/index.php?title=Naming_Convention
- **Summary**: `Title (Region) (Special Flags)` — similar to No-Intro; region in parentheses
- **Relevance for Remus**: Same region-parsing approach as No-Intro.

---

### Category F — Open-Source Scrapers and Aggregators

---

#### F1. Skyscraper (Gemba fork)

- **URL**: https://github.com/Gemba/skyscraper
- **Original**: https://github.com/muldjord/skyscraper (archived June 2022)
- **Language**: C++/Qt5
- **License**: GPL-3.0
- **Supported scraping modules**:
  | Module | Auth | Rate Limit | Media | Notes |
  |--------|------|-----------|-------|-------|
  | screenscraper | Optional (free registration) | 20k/day | Full media | Hash + filename |
  | thegamesdb | None needed (IP-based) | 3000/month/IP | Artwork | Filename-based |
  | arcadedb | None | None stated | Full media | MAME filename |
  | openretro | None | None stated | Art | Amiga/WHDLoad |
  | mobygames | None (global rate-limited) | ~35 ROMs/run | Artwork | Filename |
  | igdb | Twitch client-id | 4/sec | Artwork | Filename |
  | worldofspectrum | None | — | Art | ZX Spectrum only |
  | esgamelist / import | Local | — | — | Import from file |
- **Notes for Remus**: The Gemba fork is actively maintained. Its scraping module implementations (especially for ScreenScraper) serve as excellent implementation references for C++/Qt integration. Note that the muldjord original is ARCHIVED — always use Gemba.

---

#### F2. ArcadeDB

- **URL**: http://adb.arcadeitalia.net/
- **Data available**: MAME game info, screenshots, marquees, cabinet art, shortplay videos (VideoSnaps project); manufacturer, year, description
- **Access method**: REST API (MAME set name-based); free
- **Authentication**: ✅ None for basic queries
- **License**: Italian community project; free use; no explicit license
- **Platform coverage**: Arcade only (MAME)
- **Data format**: JSON REST
- **Actively maintained**: Yes — last MAME update Feb 27 2026 (MAME 0.286)
- **Notes for Remus**: Best free source for arcade-specific metadata and media. The only source providing shortplay video snaps for arcade titles at no cost.

---

## Recommendations

### Priority 1 — Implement now (no API key, free, high value)

| Source | What it adds | Integration effort |
|--------|-------------|-------------------|
| **libretro-database** metadata DATs | `genre`, `developer`, `publisher`, `description`, `players` for hash-matched games | Parse DATs offline; bundle or download on demand |
| **libretro-thumbnails** CDN | `boxart` for hash-matched games | HTTP GET to `thumbnails.libretro.com/{System}/Named_Boxarts/{Name}.png` |
| **GameTDB** XML databases | `genre`, `players`, `publisher`, `description` for Wii/GC/WiiU/DS/3DS/Switch/PS3 | Download wiitdb.zip etc. on demand; parse XML |
| **Wikidata** SPARQL | `description`, `genre`, `publisher`, `developer`; CC0 | SPARQL query by game name; good fallback |

### Priority 2 — Implement with free registration

| Source | What it adds | Integration effort |
|--------|-------------|-------------------|
| **ScreenScraper** | Full metadata + all artwork (hash-based) | Account required; pass already-computed hash; reference Skyscraper implementation |
| **RetroAchievements** API | Hash-based game identification; cross-reference for version/region | Free API key; useful for edge-case ROM identification |

### Priority 3 — Implement with API key (already done or budgeted)

| Source | Status |
|--------|--------|
| **TheGamesDB** | Already integrated; focus on caching to extend 3k/month limit |
| **IGDB** | Already integrated; automate token refresh; query `multiplayer_modes` for `players` |
| **MobyGames** | Add when free sources insufficient; excellent description quality |

### Do not implement

| Source | Reason |
|--------|--------|
| LaunchBox Games DB | No public API or bulk download |
| OpenVGDB | Abandoned since 2021; stale data |

---

## Addressing the Specific Gaps in cli-integration-improvements.md

The four improvement items from [docs/plans/cli-integration-improvements.md](../../docs/plans/cli-integration-improvements.md):

### Item 2 — Empty `region` field
**Recommended solution**: Parse region from filename using No-Intro/TOSEC/Redump conventions.
- `(USA)` → `us`, `(Europe)` → `eu`, `(Japan)` → `jp`, `(World)` → `wor`
- Also available: ScreenScraper returns region per disk entry; IGDB `game_localizations` returns per-region data

### Item 3 — Sparse EmulationStation metadata (`desc`, `genre`, `players`)
**Recommended solution (no-auth path)**:
1. Hash match → libretro-database metadata DAT → `genre`, `developer`, `players`
2. GameTDB XML lookup (for Nintendo platforms) → `description`, `genre`, `players`
3. Wikidata SPARQL fallback → `description`, `genre`, `publisher`

**Recommended solution (registered account)**:
- ScreenScraper hash-based lookup → full metadata + artwork in one call

**Already-integrated path**:
- IGDB `summary` → `desc`; `genres` → `genre`; `multiplayer_modes.offlinemax` → `players`
- TheGamesDB `overview` → `desc`; `genres` → `genre`; `players` → `players` (direct field)

---

## Gaps and Further Research Needed

1. **ScreenScraper API v2 endpoint structure** — The canonical API URL `api2.php` returned 404 during research. Consult the Gemba/skyscraper source code (`src/scrapers/ScreenScraper*`) for the correct parameter names and endpoint path.
2. **RetroAchievements API v1** — Direct fetch was blocked (403/404). Consult the RA developer docs or the [ConnectorRetroAchievements documentation](https://api.retroachievements.org/) once authenticated.
3. **Redump DAT bulk download** — Requires free registration; process needs to be documented in a future tool script.
4. **libretro-database metadata DAT coverage audit** — Which platforms have `description`/`genre`/`players` populated? Needs local inspection of the repo.
5. **Wikidata query reliability** — Test SPARQL queries for representative retro titles to gauge actual coverage and latency.
