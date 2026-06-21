# Compendium external data sources

Reference for filling gaps in the Remus compendium (hashes, metadata, patches, arcade
fields). URLs are starting points — verify licensing and freshness before automation.

## Hash / identity (ROM verification)

| System / gap | Source | URL | Notes |
|--------------|--------|-----|-------|
| Most cartridge/disc systems | No-Intro | [datomatic.no-intro.org](https://datomatic.no-intro.org/) | Standard hash catalogues; `data/databases/no-intro/` |
| CD-based systems | Redump | [redump.org](https://redump.org/) / [old.redump.info/datfile/](http://old.redump.info/datfile/) | Fetched by `scripts/update_dats.sh` |
| Wii U **disc** dumps | Redump Wii U | [redump.org/datfile/wiiu/](https://redump.org/datfile/wiiu/) | Signature-rich; preferred over hashless libretro GameTDB DAT |
| Wii U **digital** (CDN) | No-Intro Wii U (Digital) | No-Intro “Wii U (Digital) (CDN)” set | Separate from physical Redump set |
| Curated baseline | libretro-database | [github.com/libretro/libretro-database](https://github.com/libretro/libretro-database) | `scripts/update_dats.sh`; some sets are metadata-only (no hashes) |
| Arcade / MAME | MAME + listxml | `mame -listxml` (local) or [GitHub MAME releases](https://github.com/mamedev/mame/releases) | `data/mame/listxml.xml` for developer/year enrichment |

**Wii U policy:** `libretro-dat-nintendo-wii-u` is **disabled** in the manifest — it contains
~2.8k GameTDB entries without crc/md5/sha1. Use Redump + Digital No-Intro instead.

## Metadata enrichment (offline / bulk)

| Field gap | Source | Location / tool | Enricher pass |
|-----------|--------|-----------------|---------------|
| Genre (libretro) | libretro-database metadata | `data/metadata/` | Libretro metadata |
| Console metadata | GameTDB XML | `data/gametdb/` (`*.xml`, `wiiutdb.xml`) | GameTDB |
| Multi-system IDs | OpenVGDB | `data/openvgdb/openvgdb.sqlite` | OpenVGDB |
| Arcade genre | MAME catver | `data/mame/catver.ini` | MAME catver |
| Arcade dev/year/players | MAME listxml | `data/mame/listxml.xml` | MAME listxml |
| ZX Spectrum | ZXInfo API v3 | [zxinfo.dk](https://zxinfo.dk/) | ZXInfo |
| MAME arcade (35k+) | listxml manufacturer/year | Generate via `update_dats.sh` §4b | MAME listxml |

## Metadata enrichment (online API)

| Source | API docs | Auth | Compendium bulk pass | Runtime CLI |
|--------|----------|------|----------------------|-------------|
| **IGDB** | [api-docs.igdb.com](https://api-docs.igdb.com/) | Twitch OAuth | Yes — `enrichFromIGDB` | Yes |
| **RetroAchievements** | [api.retroachievements.org](https://api.retroachievements.org/) | Username + API key | Yes — hash + metadata | Yes |
| **Hasheous** | [hasheous.org/swagger](https://hasheous.org/swagger/index.html) | Optional client API key (`REMUS_HASHEOUS_API_KEY`) for MetadataProxy | Yes — `enrichFromHasheous` (`--enrich-source hasheous`) | Yes (priority 91) |
| **PlayMatch** | [RetroRealm/playmatch](https://github.com/RetroRealm/playmatch) | None on public instance | Yes — `enrichFromPlayMatch` (`--enrich-source playmatch`) | Yes (priority 88) |
| **ScreenScraper** | [screenscraper.fr](https://www.screenscraper.fr/) | Dev credentials | **No** | Yes |
| **TheGamesDB** | [thegamesdb.net](https://thegamesdb.net/) | API key | **No** | Yes |

### Hasheous / PlayMatch (hash→metadata bridges)

Both services map ROM hashes (No-Intro, Redump, TOSEC, MAME) to external metadata IDs.
RomM 4.0+ uses them as complementary hash matchers; Remus already integrates **Hasheous**
at runtime (`hasheous_provider.cpp`) with optional MetadataProxy enrichment when
`REMUS_HASHEOUS_API_KEY` is set.

| Capability | Hasheous | PlayMatch |
|------------|----------|-----------|
| Hash lookup (CRC/MD5/SHA1 POST) | Yes — `/api/v1/Lookup/ByHash` | Yes — Rust microservice |
| IGDB metadata without own Twitch app | Yes — MetadataProxy | No — needs IGDB creds |
| RA game IDs in lookup response | Sometimes | Via IGDB linkage |
| Bulk compendium enrichment | Yes — `enrichFromHasheous` | Yes — `enrichFromPlayMatch` |
| **Future use** | Gap-fill pass for unmatched signatures; proxy IGDB for games missing description | Secondary hash matcher for disc systems; IGDB ID bridge when Hasheous misses |

**Deferred gaps (research):** Multi-track DAT ingest (only Track 01 hashed per disc game block) and
SHA256 in bulk bridge passes — see
[COMPENDIUM-MULTI-DISC-SHA256-RESEARCH.md](COMPENDIUM-MULTI-DISC-SHA256-RESEARCH.md). Full pipeline
audit and roadmap: [COMPENDIUM-BUILD-DEEP-RESEARCH.md](COMPENDIUM-BUILD-DEEP-RESEARCH.md).

Reference: [RomM metadata providers](https://docs.romm.app/latest/Getting-Started/Metadata-Providers/),
[Hasheous repo](https://github.com/gaseous-project/hasheous).

### High-value IGDB slugs to add over time

Query IGDB for authoritative slugs: `fields name,slug; limit 500;` on `/v4/platforms`.

Common additions for gap systems: `msx`, `msx2`, `3do`, `neogeocd`, `fds`, `atari5200`,
`atari8bit`, `atari-st`, `colecovision`, `intellivision`, `pc-fx`, `philips-cdi`,
`amiga-cd32`, `vic-20`, `odyssey-2`, `pc-98`.

## IBM PC / DOS description gap

| Source | URL | Notes |
|--------|-----|-------|
| IGDB `pc_dos` slug | IGDB API | Already mapped for `ID_IBM_PC` |
| MobyGames (manual) | [mobygames.com](https://www.mobygames.com/) | No bulk API in pipeline; consider Hasheous hash bridge |
| PC Gamer / archive.org | Various | Not automated — low priority |

## Patch / translation catalogues

| Source | URL | Sync / import | Notes |
|--------|-----|---------------|-------|
| **libretro hacks DATs** | [metadat/hacks](https://github.com/libretro/libretro-database/tree/master/metadat/hacks) | `update_dats.sh` → `data/patches/hacks/` | **Primary** — ClrMamePro format with hashes + `patch "url"`; imported via `--import-patch-catalog` |
| No-Intro non-Redump | [datomatic.no-intro.org](https://datomatic.no-intro.org/) | Manual `.dat` → `data/patches/` | Hacks, translations, bad dumps — verify set name |
| ROMhacking.net | [romhacking.net](https://www.romhacking.net/) | Linked from libretro hacks entries | Project pages; not a bulk DAT export |
| **RAPatches** | [RetroAchievements/RAPatches](https://github.com/RetroAchievements/RAPatches) | Clone + `rapatches_catalog_builder` | RA achievement patches (zip/7z); separate from compendium `patch_entries` but useful for mod workflow |
| Lost Level DAT | RA hash verification project | Manual | Label `lostlevel` in RA docs — supplementary hash sets |
| Local import | `data/patches/**/*.dat` | `scripts/import_patch_catalog.sh` | Maps libretro DAT basename → `systems.libretro_name` |

**Phase 2 check:** `catalog.patch_sources_nonempty` passes once libretro hacks DATs are imported
(build pipeline runs import automatically after compendium build).

## Additional hash catalogues (not yet in manifest)

| Source | URL | Notes |
|--------|-----|-------|
| TOSEC | [tosecdev.org](https://www.tosecdev.org/) | Broad coverage; lower libretro precedence; Hasheous indexes TOSEC |
| libretro homebrew | [metadat/homebrew](https://github.com/libretro/libretro-database/tree/master/metadat/homebrew) | Independent/homebrew titles — candidate for future manifest entries |
| libretro libretro-dats | [metadat/libretro-dats](https://github.com/libretro/libretro-database/tree/master/metadat/libretro-dats) | Fan translations, FDS extras |
| MAME Software List | MAME `-listxml` / progettosnaps | RA label `mamesl`; partial MAME coverage already via listxml |

## Art / extended metadata (runtime only today)

| Source | URL | Use |
|--------|-----|-----|
| SteamGridDB | [steamgriddb.com](https://www.steamgriddb.com/) | Art-only runtime provider (grids/heroes/logos); bridges via Steam app id from IGDB |
| MobyGames | [mobygames.com](https://www.mobygames.com/) | DOS/PC descriptions — no bulk API in Remus |
| LaunchBox | Community databases | Box art / metadata — not wired |
| ScreenScraper | [screenscraper.fr](https://www.screenscraper.fr/) | Rich media + text — runtime provider |

## Source quality tiers (build + runtime)

Remus assigns each source a **narrow role** so providers are not queried for fields they
cannot supply reliably:

| Tier | Sources | Role |
|------|---------|------|
| Identity | No-Intro, Redump, libretro DATs, compendium | Hashes, title, region, serial |
| Offline metadata | Libretro metadata, GameTDB, OpenVGDB*, MAME catver/listxml | Text fields via COALESCE |
| Offline bridges | Hasheous offline dumps | `igdb_id` + sparse metadata (no API) |
| Bulk online | IGDB, RetroAchievements | Platform bulk + per-id IGDB; RA hash metadata (**not** descriptions) |
| Per-game online | Hasheous API, PlayMatch, ZXInfo | `--online-enrichment-all` only |
| Runtime text | Compendium → SS → Hasheous/PlayMatch → IGDB → TGDB → Wikidata | Name/hash waterfall (capability-gated) |
| Runtime art | SS → GameTDB → SteamGridDB → IGDB → RA → Wikidata | `getArtworkWithFallback` only |

\*OpenVGDB skips systems covered by GameTDB XML (Nintendo/PS3 family) to avoid boilerplate descriptions.

### Build profiles

| Profile | Command | Online API use |
|---------|---------|----------------|
| Offline (~90 min) | `build_compendium_full.sh --skip-update` | None |
| Recommended | `+ --online-enrichment` | Hasheous **offline dumps** + IGDB + RA bulk |
| Full bridges (days) | `+ --online-enrichment-all` | + Hasheous/PlayMatch/ZXInfo per-game APIs |

`--enrich-compendium` defaults to **offline-only** (same flags as `--build-compendium`).
Pass `--online-enrichment` for IGDB/RA on an existing DB without accidental bridge API storms.

### Deferred bulk passes (ScreenScraper / TheGamesDB)

ScreenScraper and TheGamesDB are integrated at **runtime** via `MetadataProvider` but are **not**
compendium bulk enrichment passes today. Reasons:

- Both require per-developer API credentials and rate limits unsuitable for full-catalog rebuilds.
- GameTDB + IGDB + Hasheous already cover most console metadata fields in bulk.
- ScreenScraper excels at box art and regional media — better suited to on-demand fetch than SQLite bulk ingest.

To add a future bulk pass: implement `enrichFromScreenScraper` / `enrichFromTheGamesDB` in
`src/cli/`, register in `cli_compendium_build_phases.cpp`, and gate on `--enrich-source` plus
credential presence (same pattern as IGDB).

## Systems with no DAT coverage (expected empty)

Modern platforms without public hash sets: Switch, PS4, Xbox One, Nuon, etc. Metadata
may come from IGDB if a platform slug exists and credentials are configured.

## Operational checklist after source updates

```bash
bash scripts/update_dats.sh --all          # includes data/patches/hacks/ sync
bash scripts/generate_compendium_manifest.sh
bash scripts/build_compendium_full.sh --skip-update   # imports patch catalog before validation
# Or on an existing DB:
bash scripts/import_patch_catalog.sh data/compendium/remus_compendium.db
build/remus-cli --enrich-compendium --compendium-output data/compendium/remus_compendium.db
# With online bulk passes (IGDB + RA + Hasheous offline dumps):
build/remus-cli --enrich-compendium --online-enrichment --compendium-output data/compendium/remus_compendium.db
build/remus-cli --dedup-compendium --compendium-output data/compendium/remus_compendium.db
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db \
  data/compendium/validation/0002_phase2_quality_checks.sql
```

## Recommended priority order (gap remediation)

| Priority | Action | Status |
|----------|--------|--------|
| P1 | Import libretro hacks patch catalog | `update_dats.sh` + `--import-patch-catalog` |
| P2 | IGDB pass for missing `release_date` | Included in IGDB gap query |
| P3 | RA `ra_game_id` counts as enrichment | Fixed in RA bulk pass |
| P4 | Zero-game systems (Switch, PS4…) | Needs DAT sources + IGDB slug mapping |
| P5 | Duplicate canonical titles | `--dedup-compendium` on existing DB |
| P6 | Hasheous bulk gap-fill (future) | Design: unmatched signatures → MetadataProxy |
| P7 | ScreenScraper / TGDB bulk passes (future) | Reduce IGDB API pressure for art/text |
