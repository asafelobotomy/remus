# Compendium Completeness Plan — 2026-05-26

> **Goal**: Make the Remus Compendium the sole, default, and authoritative metadata source
> for all games on all supported systems — requiring no external API call at runtime.
>
> **Status baseline**: Post-`normalizeMetadataTitle` fix rebuild (commit `267ce5b`).
> 180,716 games · 493,133 signatures · 0 merge conflicts.

---

## Current State

### Field completeness (all 180,716 games)

| Field | Coverage |
|---|---|
| genre | 62.1% |
| description | 55.0% |
| release_year | 51.5% |
| developer | 47.3% |
| publisher | 42.6% |
| players_max | 24.7% |
| rating | 8.9% |
| release_date (full) | **0.0%** — column exists, never written |
| primary_region_code | 62.9% |

### Per-system highlights

| System | Games | Developer | Year | Genre | Description | Notes |
|---|---|---|---|---|---|---|
| Arcade / MAME | 35,942 | 0% | 0% | 100% | 83% | listxml.xml absent |
| IBM PC Compatible | 15,921 | 91% | 99% | **2%** | **1%** | No IGDB slug mapped |
| Sony PlayStation 2 | 11,011 | 65% | 70% | 67% | 68% | Good |
| Nintendo Wii | 6,908 | 86% | 88% | 88% | 86% | Good |
| Nintendo Wii U | 3,400 | 94% | 98% | 91% | 90% | Best overall |
| Commodore Amiga | 5,417 | 46% | 58% | 57% | 50% | No dedicated enricher |
| Amstrad CPC | 3,011 | 2% | 3% | 3% | 2% | No enricher at all |
| Sinclair ZX Spectrum | 2,623 | 10% | 85% | 87% | **2%** | ZXInfo field mapping gap |
| Commodore 64 | 3,329 | 46% | 58% | 57% | 50% | No dedicated enricher |
| Microsoft Xbox 360 | 7,262 | 24% | 26% | 25% | 25% | Mostly DLC/addon titles |

### Systems with zero enrichment (0% developer, 0% description)

Acorn Archimedes (78), Tandy VIS (72), Benesse Pocket Challenge V2 (66), Sega Beena (57),
Bandai Playdia (38), Nintendo Pokémon Mini (24), Konami Picno (20), Mobile Symbian (17),
Funtech Super A'Can (12), Casio Loopy (10), Commodore 16/Plus4 (5), NEC PC-8801 (4),
Sharp X68000 (1), LeapFrog LeapPad (1).

### Fields not tracked by the schema at all

- Cover art / box art / screenshot URLs
- Alternate regional titles
- Series / franchise
- Age rating (ESRB, PEGI, CERO)
- Multiplayer mode detail (online, co-op, competitive)

---

## Gap Analysis

### G1 — MAME listxml absent (35,942 arcade games, 0% developer/year)

The `compendium_enrichment_mame_listxml.cpp` enricher is fully implemented but skipped
because `data/mame/listxml.xml` does not exist. MAME's `listxml` dump provides
`<manufacturer>` (developer/publisher) and `<year>` for every ROM entry.

**Fix**: Download the MAME listxml dump (no MAME installation required — the XML is
distributed standalone). Add a step to `scripts/update_dats.sh` or a new
`scripts/update_mame_listxml.sh` to fetch it. The enricher will automatically activate
on the next build.

**Expected gain**: ~35,900 games gain developer, publisher, and release year.
Genre is already 100% from catver.ini.

---

### G2 — IBM PC IGDB slug **(fixed)**

`system_resolver_provider_mappings.cpp` maps `ID_IBM_PC` to IGDB slug `pc_dos`.

---

### G3 — ZXInfo description not mapped (2,623 ZX Spectrum games, 2% description)

The ZXInfo API returns synopsis/description fields but the enricher is not writing them
to the compendium. Developer is also only 10% despite ZXInfo being a ZX-specific
database with extensive publisher records.

**Fix**: Audit `compendium_enrichment_zxinfo.cpp` field extraction — check which ZXInfo
response fields are being read and which are silently dropped. The API response includes
`publishers`, `authors`, and `description` (synopsis). Ensure all three are mapped.

**Expected gain**: ~2,000 ZX Spectrum games gain description and developer/publisher.

---

### G4 — `release_date` **(fixed)**

Enrichers now write `games.release_date` alongside `release_year`.

---

### G5 — OpenVGDB title fallback **(fixed)**

Title-fallback uses the same broad gap predicate as the hash path.

---

### G6 — ScreenScraper compendium enricher **(implemented)**

See `src/cli/compendium_enrichment_screenscraper.cpp` (runs with `--online-enrichment`).

---

### G7 — Schema missing artwork, series, age-rating fields

The `games` table has no columns for cover art URLs, franchise/series, ESRB/PEGI rating,
or alternate titles. These fields are available from IGDB (series, age_ratings, cover),
GameTDB (cover art download URLs), and ScreenScraper (media/screenshot/video URLs).

**Fix**: Schema migration adding:

- `cover_url TEXT` — primary box art URL or local path
- `series TEXT` — franchise/series name (IGDB)
- `age_rating TEXT` — e.g. "PEGI 12", "ESRB E" (IGDB)
- `alternate_titles TEXT` — JSON array of regional/alternate names

This is a **schema change** requiring a migration file under `data/compendium/migrations/`.

---

### G8 — LaunchBox Games Database (XML, filename-based) **(implemented)**

See `src/cli/compendium_enrichment_launchbox.cpp` and `scripts/update_launchbox_metadata.sh`.

---

### G9 — Wikidata SPARQL **(implemented)**

See `src/cli/compendium_enrichment_wikidata.cpp` (runs with `--online-enrichment`).

**Note on web scraping**: Community databases that would fill the remaining gaps
(Lemon64 for C64, Hall of Light for Amiga) both actively block automated HTTP access —
Lemon64 via `robots.txt` (`Disallow: /` for all non-search-engine bots) and Hall of
Light via Anubis bot-protection middleware. CPC-Power and Atarimania have no published
API or data export. HTTP scraping is not a practical avenue for these sources.

**Expected gain**: First meaningful enrichment for Amstrad CPC (3,011 games), BBC Micro,
Atari ST, and MSX — systems where no current enricher applies and ScreenScraper would
complement with hash-based depth.

---

### G10 — Hasheous platform dumps (offline hash→metadata map, all platforms, no auth)

[Hasheous](https://hasheous.org/) publishes **weekly per-platform ZIP dumps** containing the
complete hash→metadata map for every platform in its database. These are downloadable as
static files with no API key, no rate limiting, and no per-query network call. The full
metadata map is also available as a single `MetadataMap.zip`.

**Key finding from the dumps page**: Hasheous has hash data for virtually every platform
Remus supports, including the gap systems:

| Platform | Dump size |
|---|---|
| Commodore 64 | 565 MB |
| Commodore Amiga | 151 MB |
| Sinclair ZX Spectrum | 123 MB |
| Microsoft Windows | 147 MB |
| Amstrad CPC | 17 MB |
| BBC Micro | 12 MB |
| Acorn Archimedes | 1.4 MB |
| Bandai Playdia | 14 KB |
| Benesse Pocket Challenge V2 | 95 KB |
| Nintendo Pokémon Mini | 163 KB |
| Casio Loopy | 60 KB |

The live API also exposes `GET /api/v1/Lookup/ByHash/crc/{crc}`, `/md5/{md5}`,
`/sha1/{sha1}`, and `/sha256/{sha256}` for per-hash lookups, and a
`/api/v1/Lookup/Platforms` catalog. The MetadataProxy routes proxy IGDB, TheGamesDB,
and GiantBomb through Hasheous — so a hash match returns cross-source metadata IDs that
can be resolved via those proxies.

**Dump schema (confirmed — Nintendo DS sample, 5,847 games)**:

Each ZIP contains one JSON file per game named `{Title} ({HasheousId}).json`. Top-level
keys: `Id`, `ObjectType`, `SignatureDataObjects`, `Metadata`, `Attributes`,
`CreatedDate`, `UpdatedDate`, `Name`.

Fields directly usable by the enricher:

| Attribute | Type | Compendium field |
|---|---|---|
| `AIDescription` | LongString | `description` (if null) |
| `Tags.GameGenre[0].Text` | EmbeddedList | `genre` (if null) |
| `Name` | top-level string | `title` fallback |
| `SearchAliases` | LongString | future `alternate_titles` |
| `ROMs[].Crc/Md5/Sha1` | EmbeddedList | match key — links hash → game |
| `ROMs[].SignatureSource` | string | `NoIntros` or `Redump` |

Cross-reference IDs resolved from the `Metadata` array (used to seed further enrichment):

| Metadata Source | What it provides |
|---|---|
| `IGDB` | IGDB game ID → existing IGDB enricher can fetch full record |
| `TheGamesDb` | TGDB ID (future enricher) |
| `GiantBomb` | GiantBomb ID (future enricher) |
| `RetroAchievements` | RA game ID (supplements existing RA enricher) |
| `Wikipedia` | Wikipedia URL (Wikidata enricher companion) |

**Missing from dump (require live API or other source)**: Developer, Publisher,
ReleaseYear/Date, Rating, artwork URLs (only a logo `ImageId` SHA1 is stored, not a
resolved URL).

**Integration approach — offline dump (preferred)**:

1. Add `scripts/update_hasheous_dumps.sh` (analogous to `update_dats.sh`) to download
   per-platform ZIPs from `https://hasheous.org/api/v1/Dumps/platforms/{platformname}.zip`
   into `data/hasheous/`.
2. Implement `src/cli/compendium_enrichment_hasheous.cpp`:
   - Walk all JSON files in the platform dump directory.
   - Build a hash index: `{crc, md5, sha1} → game record`.
   - For each compendium game with a matching hash and null `description`: write
     `AIDescription`.
   - For each compendium game with a matching hash and null `genre`: write first
     `Tags.GameGenre` text.
   - Optionally store IGDB/RA cross-reference IDs so subsequent enrichment passes can
     use them without re-matching by title.

**Expected gain**: Hash-based description and genre fill for ALL gap systems including
Commodore 64, Amiga, Amstrad CPC, ZX Spectrum, BBC Micro, Acorn Archimedes, Bandai
Playdia, Pokémon Mini, and every other zero-enrichment system. This is likely the
single highest-value new enricher after ScreenScraper — it requires no credentials and
covers every platform Remus supports.

---

## External Sources — Capability Map

| Source | Hash-linked | Auth required | Covers retro obscure | Art/media | Notes |
|---|---|---|---|---|---|
| **ScreenScraper** | ✅ (CRC/MD5/SHA1) | User+pass (free) | ✅ best-in-class | ✅ | Highest priority new enricher |
| **Hasheous (dump)** | ✅ (CRC/MD5/SHA1/SHA256) | None | ✅ all platforms | ⚠️ via proxy | G10; weekly offline ZIPs; format TBD — inspect before building |
| **LaunchBox DB** | ❌ filename | None (ToS: personal use) | ✅ 250+ platforms | ✅ | XML download; G8; scripts ship, not data — ToS OK |
| **Wikidata** | ❌ title only | None | ✅ esp. EU home computers | ✅ P18 image | G9; WikidataProvider implemented, not in batch pipeline |
| **IGDB** (active) | ❌ title only | Twitch OAuth | ⚠️ weak on Amiga/CPC | ⚠️ cover only | G2 fix adds IBM PC |
| **MobyGames** | ❌ | Paid API | ✅ deepest credits | ✅ | ~340K games, strict TOS — monitor |
| **TheGamesDB** | ❌ | Optional key | ⚠️ | ✅ banners | Open API; no hash linkage |
| **GameTDB** (active) | ✅ | None | ❌ Nintendo only | ✅ | Already in pipeline |
| **OpenVGDB** (active) | ✅ | None | ⚠️ | ❌ | Unmaintained since ~2022; G5 fix |
| **RetroAchievements** (active) | ✅ | API key | ⚠️ curated subset | ❌ | Already in pipeline |
| **MAME listxml** (active, broken) | ❌ ROM name | None | ✅ arcade | ❌ | G1 fix: download XML |
| **ZXInfo** (active) | ❌ title only | None | ✅ ZX Spectrum | ❌ | G3 fix: field mapping |

---

## Phased Roadmap

### Phase 1 — No-code / low-code quick wins (highest ROI)

| Item | Gap | Effort | Expected gain |
|---|---|---|---|
| **P1-A** Download MAME listxml | G1 | Script only | +35,900 games get dev/year |
| **P1-B** Add IBM PC IGDB slug | G2 | 1-line mapping + test | +14,500 games get genre/desc |
| **P1-C** Write `release_date` in enrichers | G4 | ~20 LOC × 3 files | 93,000 games get full date |
| **P1-D** Fix OpenVGDB title fallback condition | G5 | ~5 LOC + test | Unknown; removes systematic gap |

### Phase 2 — Enricher fixes (medium effort)

| Item | Gap | Effort |
|---|---|---|
| **P2-A** ZXInfo field mapping audit | G3 | Read + fix + test |
| **P2-B** ScreenScraper compendium enricher | G6 | New enricher file (~300 LOC) |
| **P2-C** Wikidata compendium enricher (CPC, Amiga, BBC Micro, Atari ST, MSX) | G9 | New enricher file (~200 LOC) |
| **P2-D** Hasheous hash-based pass (no-credential gap filler) | G10 | New enricher file (~150 LOC) |
| **P2-E** LaunchBox XML enricher | G8 | XML parser + filename index (~250 LOC) |

### Phase 3 — Schema expansion (requires migration)

| Item | Gap | Effort |
|---|---|---|
| **P3-A** Add `cover_url`, `series`, `age_rating`, `alternate_titles` | G7 | Migration + enricher updates |
| **P3-B** Populate artwork via ScreenScraper / GameTDB | G7 | Enricher extension after P2-B |

---

## Success Metrics

| Metric | Current | Phase 1 target | Phase 2 target |
|---|---|---|---|
| Games with developer | 47.3% | ~70% | ~80% |
| Games with release_year | 51.5% | ~55% | ~60% |
| Games with release_date | 0.0% | ~52% | ~52% |
| Games with genre | 62.1% | ~65% | ~75% |
| Games with description | 55.0% | ~58% | ~70% |
| Systems with zero enrichment | 14 | 13 (Arcade fixed) | ≤5 |

---

## Source Files by Gap

| Gap | Primary files |
|---|---|
| G1 | `scripts/update_dats.sh`, `src/cli/compendium_enrichment_mame_listxml.cpp` |
| G2 | `src/core/system_resolver_provider_mappings.cpp` |
| G3 | `src/cli/compendium_enrichment_zxinfo.cpp` |
| G4 | `src/cli/compendium_enrichment_gametdb.cpp`, `compendium_enrichment_igdb.cpp`, `compendium_enrichment_openvgdb.cpp` |
| G5 | `src/cli/compendium_enrichment_openvgdb.cpp` |
| G6 | New: `src/cli/compendium_enrichment_screenscraper.cpp` |
| G7 | `data/compendium/migrations/`, `src/cli/compendium_enrichment_*.cpp` |
| G8 | New: `src/cli/compendium_enrichment_launchbox.cpp` |
| G9 | New: `src/cli/compendium_enrichment_wikidata.cpp` |
| G10 | New: `src/cli/compendium_enrichment_hasheous.cpp` |
