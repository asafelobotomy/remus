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

| Source | API docs | Auth | IGDB platform slugs |
|--------|----------|------|---------------------|
| **IGDB** | [api-docs.igdb.com](https://api-docs.igdb.com/) | Twitch OAuth (`enrichment-credentials.json`) | Mapped in `system_resolver_provider_mappings.cpp`; query all slugs: `POST /v4/platforms` with `fields name,slug` |
| **RetroAchievements** | [api.retroachievements.org](https://api.retroachievements.org/) | Username + API key | Console IDs in same mappings file |
| **ScreenScraper** | [screenscraper.fr](https://www.screenscraper.fr/) | Dev credentials | Not yet in compendium bulk build (runtime provider only) |
| **TheGamesDB** | [thegamesdb.net](https://thegamesdb.net/) | API key | Mapped for TGDB IDs; no bulk compendium pass yet |

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

| Source | URL | Notes |
|--------|-----|-------|
| No-Intro hacks / translations | [datomatic.no-intro.org](https://datomatic.no-intro.org/) | Non-Redump sections |
| ROMhacking.net | [romhacking.net](https://www.romhacking.net/) | Project-specific patch releases |
| Local import path | `data/patches/**/*.dat` | `scripts/import_patch_catalog.sh` (registration stub) |

## Systems with no DAT coverage (expected empty)

Modern platforms without public hash sets: Switch, PS4, Xbox One, Nuon, etc. Metadata
may come from IGDB if a platform slug exists and credentials are configured.

## Operational checklist after source updates

```bash
bash scripts/update_dats.sh --all
bash scripts/generate_compendium_manifest.sh
bash scripts/build_compendium_full.sh --skip-update   # or full pipeline
build/remus-cli --enrich-compendium --compendium-output data/compendium/remus_compendium.db
build/remus-cli --dedup-compendium --compendium-output data/compendium/remus_compendium.db
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db \
  data/compendium/validation/0002_phase2_quality_checks.sql
```
