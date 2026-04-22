# Metadata Source Improvements Plan

> Date: 2026-03-26 | Status: Mostly Implemented | Research: [retro-game-metadata-sources-2026-03-26.md](../../.github/research/retro-game-metadata-sources-2026-03-26.md)

## Goal

Shift Remus metadata acquisition to prioritise free, no-auth sources.
APIs that require keys or accounts become optional enrichment, not the primary path.

## Current State

| Provider | Priority | Auth | Hash | Fields populated |
|----------|----------|------|------|-----------------|
| Hasheous | 100 | None | Yes | title, externalIds (basic); full IGDB fields via optional key |
| ScreenScraper | 90 | User+pass | Yes | All fields — most comprehensive |
| TheGamesDB | 50 | Optional key | No | title, releaseDate, artwork |
| IGDB | 40 | Twitch OAuth | No | title, description, genres, developer, publisher, rating |
| LocalDatabase | 110* | None | Yes | title, region, description (fallback), externalIds |

\*LocalDatabase is **not registered** in the orchestrator — runs as a separate offline path only.

**Key gaps**: Only 1 DAT file ships (Genesis). `genre`, `developer`, `publisher`, `players` are empty unless an authed provider fills them. No free artwork path exists.

---

## Tier 1 — No Auth, Highest Priority

### 1.1 Expand Local Database with libretro-database DATs

**Problem**: `data/databases/` contains a single `Sega - Mega Drive - Genesis.dat`. libretro-database has 80+ system DATs.

**Solution**:

1. **Compendium build workflow** — use `remus-cli --build-compendium --compendium-manifest <path> --compendium-output <path>` to rebuild bundled catalogs instead of shipping raw DAT files.
2. **Ship core DATs** — bundle the top 15–20 systems (NES, SNES, GB, GBC, GBA, N64, Genesis, Master System, Game Gear, Atari 2600, Atari 7800, TG-16, Neo Geo, PSX, Saturn) so the first run works offline.
3. **Register in orchestrator** — add `LocalDatabaseProvider` to `buildOrchestrator()` at priority 110 (above Hasheous). Hash matches from local DATs resolve instantly with no network.

**Fields gained**: title, region, serial, CRC32/MD5/SHA1 (for cross-referencing).

**LOC estimate**: ~30 (script) + ~20 (orchestrator registration).

### 1.2 Parse libretro-database Metadata DATs

**Problem**: The basic DATs only have hash + name. The libretro-database repo also ships **metadata DATs** under `metadat/` with `genre`, `developer`, `publisher`, `players`, and `description` per game.

**Solution**:

1. **Extend compendium refresh** — include metadata catalog ingestion (genre, developer, publisher, players) in the compendium update pipeline.
2. **Add metadata DAT parser** — parse the simple `key = value` format used in metadat DATs. These use game name as the key, mapping to a genre string, developer name, etc.
3. **Enrich in `datEntryToMetadata()`** — after a hash match gives us the canonical game name, look up that name in the metadata DAT indexes to fill `genre`, `developer`, `publisher`, `players`.

**Fields gained**: genre, developer, publisher, players, description (for all 80+ systems).

**LOC estimate**: ~100 (metadata DAT parser) + ~40 (enrichment in LocalDatabaseProvider).

### 1.3 libretro-thumbnails Artwork

**Problem**: `getArtwork()` in LocalDatabaseProvider returns empty. Artwork only comes from authed providers.

**Solution**:

1. After a hash match resolves a canonical game name (from libretro-database), construct the artwork URL:
   ```
   https://thumbnails.libretro.com/{System}/Named_Boxarts/{GameName}.png
   https://thumbnails.libretro.com/{System}/Named_Snaps/{GameName}.png
   https://thumbnails.libretro.com/{System}/Named_Titles/{GameName}.png
   ```
2. System names map to libretro's naming (e.g. `Nintendo - Game Boy Advance`, `Sega - Mega Drive - Genesis`).
3. URL-encode special characters in the game name (`&` → `%26`, etc.).
4. Implement in `LocalDatabaseProvider::getArtwork()`.
5. No API call needed — just URL construction. Download happens at export time or on demand.

**Fields gained**: boxArtUrl, screenshotUrls (gameplay snap + title screen).

**LOC estimate**: ~40 (URL construction + system name mapping).

### 1.4 Wikidata SPARQL Fallback

**Problem**: When a game is too obscure for libretro-database or Hasheous, no free fallback exists for description/genre.

**Solution**:

1. New provider: `WikidataProvider` — queries `https://query.wikidata.org/sparql` by game title.
2. SPARQL query extracts: description (via `schema:description`), genre (`wdt:P136`), developer (`wdt:P178`), publisher (`wdt:P123`), release date (`wdt:P577`).
3. Register at priority 30 (below IGDB) — last resort, CC0 licensed, variable quality.
4. No auth, no key, completely free.

**Fields gained**: description, genre, developer, publisher, releaseDate.

**LOC estimate**: ~150 (new provider + SPARQL query + response parsing).

**Defer?**: Yes — implement after 1.1–1.3 are proven. Wikidata coverage is inconsistent for retro titles.

---

## Tier 2 — Free Registration

### 2.1 ScreenScraper Integration Improvements

**Status**: Already integrated, requires free account.

**Improvements**:
- Add a `--enrich` path that uses ScreenScraper's hash-based lookup (CRC/MD5/SHA1 already computed) for one-call full metadata + artwork.
- Cache aggressively — 20,000 requests/day is generous but the cache should prevent redundant hits.
- Reference Gemba/Skyscraper source (`src/scrapers/ScreenScraper*`) for API v2 endpoint details.

### 2.2 RetroAchievements Hash Identification

**Problem**: Some ROM revisions/regions aren't in No-Intro or libretro-database but are in RetroAchievements.

**Solution**:
- New provider: `RetroAchievementsProvider` — hash-based game identification via RA API.
- Free API key (registration required).
- Register at priority 45 (between IGDB and TheGamesDB).
- Useful for cross-reference IDs (already has an `externalIds` slot: `retroachievements`).

**Defer?**: Yes — lower priority than Tier 1 work. Implement when the free-source baseline is solid.

---

## Tier 3 — Existing Provider Improvements

### 3.1 TheGamesDB Caching

- Current limit: 3,000 requests/month.
- Ensure `MetadataCache` is checked before every TheGamesDB call.
- Add a monthly request counter to warn when approaching the limit.

### 3.2 IGDB Token Auto-Refresh

- Twitch bearer tokens expire every ~60 days.
- Add automatic token refresh before expiry.
- Query `multiplayer_modes.offlinemax` for the `players` field.

### 3.3 GameTDB XML Databases (Nintendo Platforms)

**Problem**: Wii, GameCube, DS, 3DS, WiiU, Switch, PS3 games lack rich free metadata.

**Solution**:
1. Download `wiitdb.zip`, `gamecubetdb.zip`, etc. from `gametdb.com` — no auth needed.
2. Parse XML for title, genre, players, publisher, developer, description.
3. Artwork CDN: `https://art.gametdb.com/{platform}/cover/{region}/{gameID}.jpg` — no auth.
4. New provider: `GameTDBProvider` at priority 60 (above TheGamesDB, below ScreenScraper).

**LOC estimate**: ~200 (XML parser + provider).

**Defer?**: Implement alongside or after Tier 1 — significant effort but high value for Nintendo platforms.

---

## Architecture Changes

### Orchestrator Registration

```
Current chain:    Hasheous(100) → ScreenScraper(90) → TheGamesDB(50) → IGDB(40)

Proposed chain:   LocalDatabase(110) → Hasheous(100) → ScreenScraper(90)
                  → GameTDB(60) → TheGamesDB(50) → RetroAchievements(45)
                  → IGDB(40) → Wikidata(30)
```

The **no-auth path** (no credentials configured):
```
LocalDatabase(110) → Hasheous(100) → TheGamesDB(50) → Wikidata(30)
```

This alone should produce: title, region, CRC32/MD5/SHA1, serial, genre, developer, publisher, players, description, and box art — all without any API key.

### New Files

| File | Purpose |
|------|---------|
| `remus-cli --build-compendium` | Build bundled compendium catalogs (verification + patch metadata) from a manifest |
| `src/metadata/libretro_metadata_parser.h/.cpp` | Parse libretro metadat DAT files (genre, developer, etc.) |
| `src/metadata/wikidata_provider.h/.cpp` | Wikidata SPARQL provider (Tier 1, deferred) |
| `src/metadata/gametdb_provider.h/.cpp` | GameTDB XML provider (Tier 3) |
| `src/metadata/retroachievements_provider.h/.cpp` | RA hash provider (Tier 2, deferred) |
| `data/metadata/genre/*.dat` | libretro genre metadata |
| `data/metadata/developer/*.dat` | libretro developer metadata |
| `data/metadata/publisher/*.dat` | libretro publisher metadata |

### Modified Files

| File | Change |
|------|--------|
| `src/cli/cli_helpers.cpp` | Register LocalDatabaseProvider in `buildOrchestrator()` |
| `src/metadata/local_database_provider.cpp` | Enrich with metadata DATs + libretro-thumbnails artwork URLs |
| `src/metadata/local_database_provider.h` | Add metadata DAT loading methods |
| `src/core/constants/providers.h` | Add constants for new providers |
| `CMakeLists.txt` | Add new source files to `remus-metadata` target |
| `data/databases/` | Bundle top 15–20 system DATs |

---

## Implementation Order

| Phase | Items | Depends on | Auth required |
|-------|-------|-----------|---------------|
| **Phase 1** | 1.1 (expand DATs) + register in orchestrator | None | None |
| **Phase 2** | 1.2 (metadata DAT parser + enrichment) | Phase 1 | None |
| **Phase 3** | 1.3 (libretro-thumbnails artwork URLs) | Phase 1 | None |
| **Phase 4** | 3.3 (GameTDB provider) | None | None |
| **Phase 5** | 2.1 (ScreenScraper improvements) | None | Free account |
| **Phase 6** | 1.4 (Wikidata) + 2.2 (RetroAchievements) | None | None / Free key |
| **Phase 7** | 3.1 (TGDB caching) + 3.2 (IGDB token refresh) | None | Existing keys |

Phases 1–3 are the highest priority — they transform the no-auth experience from nearly empty metadata to rich metadata for every hash-matched ROM.

---

## Success Criteria

After Phases 1–3, a user with **no API keys configured** running `remus --metadata <rom>` should see:
- Title, region, serial (from DAT hash match)
- Genre, developer, publisher, players (from metadata DATs)
- Description (from DAT or Hasheous)
- Box art URL (from libretro-thumbnails CDN)

After all phases, the full export (`--export emulationstation`) fills every EmulationStation field:
`name`, `desc`, `genre`, `players`, `developer`, `publisher`, `releasedate`, `image`, `thumbnail`.
