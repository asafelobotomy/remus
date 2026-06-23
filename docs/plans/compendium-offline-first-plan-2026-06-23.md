# Compendium Offline-First Plan — 2026-06-23

> **Goal**: Build `remus_compendium.db` from **local mirrors only**, so Remus can identify,
> match, and enrich games at runtime without depending on live APIs. Network access is
> confined to a **pre-build acquisition phase** that downloads and refreshes complete data
> stores; the compendium build itself reads only from disk.
>
> **Priority**: Maximum catalogue completeness and match quality. Storage size and download
> time are acceptable costs. Online gap-fill during build remains a **dev fallback**, not
> the production path.
>
> **Related docs**:
>
> - [COMPENDIUM-DATA-SOURCES.md](../reports/COMPENDIUM-DATA-SOURCES.md) — source inventory
> - [COMPENDIUM-BUILD-DEEP-RESEARCH.md](../reports/COMPENDIUM-BUILD-DEEP-RESEARCH.md) — pipeline audit
> - [compendium-completeness-plan-2026-05-26.md](compendium-completeness-plan-2026-05-26.md) — field-gap analysis
> - [metadata-source-improvements.md](metadata-source-improvements.md) — runtime provider notes

---

## 1. Principles

### 1.1 Two-phase pipeline

| Phase | When network is used | Output |
|-------|----------------------|--------|
| **Acquisition** | Yes — download, clone, snapshot | `data/**` mirrors with version markers |
| **Compendium build** | No (production default) | `remus_compendium.db` + build report |

```text
  ┌─────────────────────────────────────────────────────────────┐
  │  ACQUISITION (scripts/update_compendium_offline_sources.sh) │
  │  DATs · metadata · GameTDB · OpenVGDB · MAME · Hasheous     │
  │  LaunchBox · libretro-thumbnails · (future snapshots)       │
  └──────────────────────────┬──────────────────────────────────┘
                             │ local files only
                             ▼
  ┌─────────────────────────────────────────────────────────────┐
  │  BUILD (scripts/build_compendium_full.sh --strict-offline)  │
  │  ingest → merge → offline enrichment → validate             │
  └──────────────────────────┬──────────────────────────────────┘
                             │
                             ▼
                    remus_compendium.db
                             │
                             ▼
  ┌─────────────────────────────────────────────────────────────┐
  │  RUNTIME (Remus app)                                        │
  │  hash match · compendium lookup · local artwork paths       │
  └─────────────────────────────────────────────────────────────┘
```

### 1.2 Design rules

1. **Download once, build many** — acquisition scripts are idempotent (skip unchanged via
   content hash or git HEAD markers).
2. **DAT-name alignment wins** — sources keyed to libretro / No-Intro names (thumbnails,
   libretro metadata) outrank title-fuzzy sources for artwork and identity bridges.
3. **Repo-relative paths in DB** — store `data/thumbnails/...` not absolute `file://` URLs
   so the compendium is portable across machines.
4. **Provenance in `game_facts`** — every enriched field records source id and snapshot id.
5. **Strict offline is the production default** once mirrors exist; online passes are
   opt-in for machines without full mirrors.

---

## 2. Current state (baseline)

### 2.1 Acquisition — implemented

`scripts/update_compendium_offline_sources.sh` orchestrates:

| Source | Script / path | Skip-if-unchanged |
|--------|---------------|-------------------|
| libretro-database DATs | `update_dats.sh --all` | SHA-256 on DAT copies |
| libretro metadata DATs | `update_dats.sh` | SHA-256 |
| GameTDB XML | `update_dats.sh` | Content compare (version attr stripped) |
| OpenVGDB SQLite | `update_dats.sh` | SHA-256 on extracted DB |
| MAME catver + listxml | `update_dats.sh` / `update_mame_listxml.sh` | SHA-256 / size gate |
| Hasheous offline dumps | `update_hasheous_dumps.sh --all-core` | ZIP SHA-256 marker |
| Patch/hack DATs | `update_dats.sh` | SHA-256 |
| LaunchBox | `update_launchbox_metadata.sh --optional` | Manual / stub only |

Wired into `build_compendium_full.sh` unless `--skip-update`.

### 2.2 Compendium enrichment — partial offline

Default build order (`cli_compendium_build_phases.cpp`):

**Offline passes (local files):**

1. libretro metadata (`data/metadata/`)
2. GameTDB (`data/gametdb/`)
3. OpenVGDB (`data/openvgdb/openvgdb.sqlite`)
4. LaunchBox (`data/launchbox/Metadata.xml`)
5. MAME catver (`data/mame/catver.ini`)
6. MAME listxml (`data/mame/listxml.xml`)
7. Hasheous offline dumps (`data/hasheous/dumps/`)

**Online gap-fill (unless `--offline-only-enrichment`):**

1. ScreenScraper (per-game API)
2. IGDB (bulk API)
3. RetroAchievements
4. TheGamesDB
5. Wikidata
6. PlayMatch / ZXInfo (only with `--online-enrichment-all`)

### 2.3 Runtime artwork — CDN only today

- `ThumbnailUrlHelper` builds `https://thumbnails.libretro.com/{System}/{Type}/{Name}.png`
- `LocalDatabaseProvider` and `CompendiumProvider::getArtwork()` use CDN URLs
- `games.cover_url` populated mainly by **IGDB online** enrichment (migration 0010)
- No `data/thumbnails/` mirror or offline artwork enricher

### 2.4 Gaps vs goal

| Gap | Impact |
|-----|--------|
| No libretro-thumbnails acquisition | Box/snap/title art requires CDN at runtime |
| LaunchBox not auto-downloaded | ~530 MB XML missing unless manual |
| IGDB / ScreenScraper / RA only online during build | Build needs credentials + network |
| No `--strict-offline` build mode | Cannot enforce zero-network compendium builds |
| No acquisition manifest / coverage gates | Cannot verify mirrors complete before build |
| `cover_url` only (no snap/title columns) | Limited artwork persistence in schema |

---

## 3. Target offline source catalog

### 3.1 Tier A — Full mirror, production required

Sources with **official bulk distribution** suitable for complete local copies.

| ID | Source | Target path | Est. size | Acquisition method | Enricher / consumer |
|----|--------|-------------|-----------|-------------------|---------------------|
| A1 | libretro-database | `data/databases/`, `data/metadata/` | ~300 MB | Git shallow clone | ingest + libretro metadata |
| A2 | No-Intro / Redump DATs | `data/databases/no-intro/`, `redump/` | ~200 MB | HTTP cache in `update_dats.sh` | ingest |
| A3 | GameTDB | `data/gametdb/*.xml` | ~60 MB | ZIP download | GameTDB enricher |
| A4 | OpenVGDB | `data/openvgdb/openvgdb.sqlite` | ~41 MB | Release ZIP | OpenVGDB enricher |
| A5 | MAME catver + listxml | `data/mame/` | ~350 MB | HTTP / local MAME | MAME enrichers |
| A6 | Hasheous platform dumps | `data/hasheous/dumps/` | ~1–2 GB compressed | API ZIP (`--all-core` or `--all`) | Hasheous offline enricher |
| A7 | **libretro-thumbnails** | `data/acquisition/libretro-thumbnails/` (input) | **~96 GB** (124 systems) | Git submodules | acquisition only → consolidate to **remus-thumbnails** |
| A7b | **remus-thumbnails** | `data/remus-thumbnails/` | **~15–60 GB** (tiered WebP, compendium subset) | `consolidate_thumbnails` pass | **new** enricher + runtime; canonical artwork store |
| A8 | **LaunchBox Games DB** | `data/launchbox/Metadata.xml` | ~100 MB zip / ~530 MB XML | `Metadata.zip` from gamesdb.launchbox-app.com | LaunchBox enricher |

**libretro-thumbnails notes:**

- Use **active** org: [github.com/libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
  — **not** archived `libretro/libretro-thumbnails` (frozen 2018).
- Meta-repo + 124 per-system submodule repos; official update: `make` or
  `git submodule update --recursive --remote --init --force`.
- Layout matches RetroArch and Remus `systems.libretro_name`:
  `{System}/Named_Boxarts|Named_Snaps|Named_Titles|Named_Logos/{Game}.png`.
- CDN `thumbnails.libretro.com` is a mirror (~2-day sync); Git is source of truth.

### 3.2 Tier B — Snapshot once, enrich offline

No official full dump; **one-time bulk crawl** during acquisition, then local SQLite/JSONL
consumed by enrichers.

| ID | Source | Target path | Est. effort | Notes |
|----|--------|-------------|-------------|-------|
| B1 | IGDB | `data/igdb/igdb.sqlite` (proposed) | 1–2 days crawl | ~360k games; 4 req/s; Twitch OAuth; no official dump |
| B2 | ScreenScraper | `data/screenscraper/ss.sqlite` (proposed) | Multi-day | ~200k games; dev credentials; no official dump; see [sscraper](https://github.com/zayamatias/sscraper) |
| B3 | RetroAchievements | `data/ra/games.json` (proposed) | Hours | Game list + hash maps; complement RA enricher |
| B4 | Wikidata | `data/wikidata/games.sparql.json` (proposed) | Hours | SPARQL export for retro titles; variable quality |

### 3.3 Tier C — Optional / manual

| ID | Source | Notes |
|----|--------|-------|
| C1 | TOSEC supplemental | Manual drop-in at `data/databases/supplemental/tosec/` |
| C2 | SteamGridDB | Art-only; runtime provider; optional local cache |
| C3 | MobyGames | No bulk API; runtime only |

### 3.4 Proposed acquisition manifest

Versioned JSON checked into repo (paths + requirements, not blobs):

```json
{
  "version": 1,
  "sources": [
    {
      "id": "libretro-thumbnails",
      "path": "data/thumbnails",
      "required": true,
      "marker": "data/thumbnails/.sync-head",
      "update_script": "scripts/update_libretro_thumbnails.sh"
    }
  ]
}
```

`update_compendium_offline_sources.sh` validates required sources before returning success;
`build_compendium_full.sh --strict-offline` fails fast if any required mirror is missing.

---

## 4. libretro-thumbnails — detailed plan

### 4.1 Why this source

- **Same naming as DAT ingest** — thumbnails keyed to playlist/DAT game names, not fuzzy titles.
- Remus already has `systems.libretro_name` (migration 0003) and `ThumbnailUrlHelper`
  (sanitize, language-tag stripping, CDN URL build).
- Highest-quality box/snap/title alignment for No-Intro / libretro catalogue entries.
- Complements IGDB/ScreenScraper (which use different title spaces).

### 4.2 Acquisition script

**New:** `scripts/update_libretro_thumbnails.sh`

```text
Usage:
  scripts/update_libretro_thumbnails.sh [--all] [--core] [--system NAME] [--force]

Options:
  --all     All 124 submodule repos (~96 GB)
  --core    CORE_SYSTEMS from update_dats.sh (default for CI smoke)
  --system  Single libretro_name directory (repeatable)
  --force   Re-fetch even when HEAD marker matches
```

**Implementation approach:**

1. Clone or update meta-repo at `$CACHE/libretro-thumbnails` OR clone per-system repos
   directly into `data/thumbnails/{System Name}/`.
2. Submodule slug rule: `Nintendo - Game Boy Advance` → `Nintendo_-_Game_Boy_Advance`.
3. Store per-system marker: `data/thumbnails/{System}/.git_head` with commit SHA.
4. Skip pull when SHA unchanged and `Named_Boxarts/` non-empty.
5. Inventory: PNG counts per type; total size; systems failed.

**Reference upstream commands** (from libretro-thumbnails README):

```bash
git clone --recursive --depth=1 https://github.com/libretro-thumbnails/libretro-thumbnails.git
cd libretro-thumbnails && make
```

### 4.3 Name matching (enrichment + runtime)

Reuse `ThumbnailUrlHelper::generateThumbnailCandidates()`:

1. Exact `canonical_title` (DAT name)
2. Language-tag-stripped variant `(En)`, `(En,Ja)`, etc.

Invalid filename chars `&*/:\<>?|"` → `_` per libretro spec.

**Edge cases to handle:**

| Case | Handling |
|------|----------|
| MAME | `systems.libretro_name = 'MAME'`; large set; verify submodule layout |
| Neo Geo / Xbox / PS4 | Some repos use nested `Named_Boxarts/` only — path resolver fallback |
| Missing thumbnail | Leave `cover_url` empty; do not invent CDN URL in strict-offline mode |

### 4.4 Compendium enrichment pass

**New:** `enrichFromRemusThumbnails` (after `consolidate_thumbnails` — see
[artwork storage feasibility §13](compendium-artwork-storage-feasibility-2026-06-23.md))

- Source id: `remus-thumbnails` (consolidated blobs; upstream `libretro-thumbnails`)
- Writes `game_assets` rows + `games.cover_url` (repo-relative blob path)
- Gap predicate: no `game_assets` row for `asset_type = 'box'`
- Position: **after libretro metadata, before LaunchBox**

Legacy note: a pass that only records paths into the libretro acquisition tree without
consolidation is **not sufficient** for `--prune-acquisition-sources`.

### 4.5 Runtime resolution

Extend `ThumbnailUrlHelper`:

```cpp
// Pseudocode
QString resolveThumbnailUrl(system, game, type, const QString &dataRoot);
// 1. For each candidate name → check dataRoot/thumbnails/{system}/{type}/{sanitized}.png
// 2. If found → return file URL or relative path
// 3. Else → buildThumbnailUrl() CDN fallback (non-strict mode only)
```

Consumers: `LocalDatabaseProvider`, `CompendiumProvider::getArtwork()`, bundle artwork step.

### 4.6 Merge policy

Add / adjust `merge_policy` for `cover_url`:

| Priority | Source | Rationale |
|----------|--------|-----------|
| 1 | libretro-thumbnails (local path) | DAT-name aligned |
| 2 | LaunchBox | Filename/title aligned |
| 3 | Hasheous offline | Hash bridge |
| 4 | IGDB snapshot (local) | Broad coverage |
| 5 | ScreenScraper snapshot (local) | Deep media |
| 6 | CDN URL | Legacy / fallback |

---

## 5. LaunchBox auto-download

RomM pattern: download [Metadata.zip](https://gamesdb.launchbox-app.com/Metadata.zip)
(~100 MB compressed, ~530 MB `Metadata.xml` uncompressed).

**Extend** `scripts/update_launchbox_metadata.sh`:

```bash
# New default when file missing:
curl -fL -o "$CACHE/Metadata.zip" "https://gamesdb.launchbox-app.com/Metadata.zip"
unzip -p Metadata.zip Metadata.xml > "$DEST_FILE"
# Skip when ZIP SHA-256 unchanged
```

Also extract `Platforms.xml`, `Mame.xml` if enricher can use them later.

**Note:** LaunchBox zip is **metadata XML only** — box art images are URL references in XML.
A follow-on **LaunchBox media acquisition** job may be needed for full offline artwork
(separate phase).

---

## 6. Tier B snapshot jobs (future phases)

### 6.1 IGDB snapshot

**New:** `scripts/snapshot_igdb.sh` + `data/igdb/README.md`

- Endpoints: `/games`, `/platforms`, `/covers` (paginated Apicalypse queries)
- Rate: 4 req/s; store progress checkpoint
- Output: SQLite with tables `igdb_games`, `igdb_covers`, `igdb_platforms`
- Enricher reads local DB instead of live API when `data/igdb/igdb.sqlite` present

### 6.2 ScreenScraper snapshot

**New:** `scripts/import_screenscraper_bulk.sh`

- Import game list + metadata into local SQLite (pattern from sscraper / scrapegoat)
- Media URLs cached under `data/screenscraper/media/` (large; optional sub-job)
- Enricher: hash lookup against local DB, no per-game HTTP during build

### 6.3 RetroAchievements / Wikidata

Lower priority once IGDB + Hasheous offline cover gaps. SPARQL export and RA public
game lists are straightforward compared to ScreenScraper media volume.

---

## 7. Build pipeline changes

### 7.1 New flags

| Flag | Behavior |
|------|----------|
| `--strict-offline` | Require all Tier A mirrors; skip all online enrichment passes; fail if mirror missing |
| `--offline-only-enrichment` | (existing) Skip online passes — does not validate mirrors |
| `--skip-update` | (existing) Skip acquisition phase |

**Production default (once Tier A complete):**

```bash
bash scripts/update_compendium_offline_sources.sh --all
bash scripts/build_compendium_full.sh --strict-offline
```

### 7.2 Enrichment pass order (target)

```text
OFFLINE (always):
  1. libretro metadata
  2. libretro-thumbnails      ← NEW
  3. GameTDB
  4. OpenVGDB
  5. LaunchBox
  6. MAME catver
  7. MAME listxml
  8. Hasheous offline dumps
  9. IGDB snapshot            ← NEW (when present)
 10. ScreenScraper snapshot   ← NEW (when present)
 11. RA snapshot / Wikidata    ← NEW (when present)

ONLINE (dev fallback only, NOT --strict-offline):
  screenscraper, igdb, ra, thegamesdb, wikidata, playmatch, zxinfo
```

### 7.3 Build report extensions

Add to `build_report.json`:

- `offline_sources_present` — map of source id → bool
- `offline_sources_version` — marker SHAs / file hashes
- `cover_url_coverage_pct`
- `thumbnail_match_count` / `thumbnail_miss_count`
- `strict_offline: true`

---

## 8. Schema considerations

### 8.1 Existing (migration 0010)

- `games.cover_url TEXT` — use for repo-relative box art path

### 8.2 Proposed migration (optional, Phase 3)

```sql
ALTER TABLE games ADD COLUMN snap_url TEXT;
ALTER TABLE games ADD COLUMN title_screen_url TEXT;
-- merge_policy rows for snap_url, title_screen_url
```

Until then, store snap/title in `game_facts` or rely on runtime `ThumbnailUrlHelper` local
resolution without DB persistence.

### 8.3 Path convention

Store paths relative to repo root:

```text
data/thumbnails/Nintendo - Game Boy Advance/Named_Boxarts/Super Mario Land (USA).png
```

Runtime resolves via `REMUS_DATA_ROOT` or application dir + relative path.

---

## 9. Validation and coverage gates

### 9.1 Acquisition validation

`scripts/update_compendium_offline_sources.sh` exit non-zero when:

- Required Tier A source missing or empty
- Marker write failed after successful update

### 9.2 Post-build SQL checks (new validation file)

`data/compendium/validation/0012_offline_completeness.sql` (proposed):

- `% games with at least one signature` (ingest health)
- `% games with genre OR description` (metadata floor)
- `% games with cover_url` (artwork floor) — warn threshold e.g. 40%, target 70%+
- Per-system coverage TSV export (extend existing coverage report)

### 9.3 CI strategy

- **Smoke:** `--core` thumbnails + `--all-core` Hasheous + quick validate
- **Nightly / manual:** full `--all` acquisition + `--strict-offline` build

---

## 10. Implementation phases

### Phase 0 — Documentation and manifest (this plan)

- [x] Formal plan doc
- [ ] Add `data/compendium/offline-sources.json` manifest
- [ ] Update `COMPENDIUM-DATA-SOURCES.md` art section
- [ ] Update `data/compendium/README.md` acquisition section

### Phase 1 — Tier A artwork (highest ROI)

See [compendium-artwork-storage-feasibility-2026-06-23.md](compendium-artwork-storage-feasibility-2026-06-23.md) for
layout (hybrid CAS-lite), schema (`0012_game_assets`), and consolidation algorithm.

| Task | Files | Est. |
|------|-------|------|
| P1.1 `update_libretro_thumbnails.sh` → `data/acquisition/` | new script | 1–2 days |
| P1.2 Migration `0012_game_assets.sql` | migrations | 0.5 day |
| P1.3 `consolidate_thumbnails.sh` (transcode → `data/remus-thumbnails/blobs/`) | new script | 1–2 days |
| P1.4 `enrichFromRemusThumbnails` | new enricher | 1 day |
| P1.5 `ThumbnailUrlHelper` blob resolution | C++ + tests | 1 day |
| P1.6 Wire consolidate + enrich into `build_compendium_full.sh` | build script | 0.5 day |

**Exit criteria:** One system end-to-end; `game_assets` + `cover_url` point at local blobs;
GUI shows box art offline; optional `--prune-acquisition-sources` for that system.

### Phase 2 — Strict offline build mode + prune at scale

| Task | Files | Est. |
|------|-------|------|
| P2.1 All CORE_SYSTEMS consolidate | scripts | 1–2 days |
| P2.2 `--prune-acquisition-sources` + per-system mode | build script | 0.5 day |
| P2.3 `--strict-offline` flag + manifest validation | build script, CLI | 1 day |
| P2.4 Blob GC + validation SQL 0012/0013 | scripts, validation | 0.5 day |
| P2.5 LaunchBox `Metadata.zip` auto-download | `update_launchbox_metadata.sh` | 0.5 day |

**Exit criteria:** `build_compendium_full.sh --strict-offline` completes with zero HTTP from
enrichers on a machine with full Tier A mirrors.

### Phase 3 — Tier B snapshots

| Task | Est. |
|------|------|
| P3.1 IGDB snapshot script + offline enricher | 3–5 days |
| P3.2 ScreenScraper bulk import + offline enricher | 5–10 days |
| P3.3 RA / Wikidata snapshots | 2–3 days |
| P3.4 Optional snap/title schema migration | 1 day |

**Exit criteria:** Production build achieves completeness-plan field targets without online
passes.

### Phase 4 — LaunchBox media + polish

- LaunchBox image URL bulk download
- Per-system thumbnail path exceptions (MAME, Neo Geo)
- Incremental thumbnail sync (only systems whose DATs changed)

---

## 11. Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Thumbnail licensing unclear | User-hosted cache; not committed to git; document credits in README |
| Git LFS migration ([issue #23](https://github.com/libretro-thumbnails/libretro-thumbnails/issues/23)) | Monitor; pin acquisition script to plain-git workflow until LFS documented |
| ~96 GB disk for full thumbnails | Accept per project priority; `--core` for dev |
| LaunchBox XML without images | Phase 4 media job; libretro-thumbnails fills DAT-aligned art |
| IGDB ToS / rate limits | Snapshot once; local enricher; respect 4 req/s |
| ScreenScraper quota on bulk import | Run import over days; cache locally; never re-hit API in build |
| Name mismatches (thumbnail vs DAT) | Candidate list + RetroArch flexible matching rules; report miss rate |
| DB portability | Repo-relative paths only |
| Large thumbnail storage (~30–96 GB) | See [compendium-artwork-storage-feasibility-2026-06-23.md](compendium-artwork-storage-feasibility-2026-06-23.md) — remus-thumbnails + WebP consolidation + `--prune-acquisition-sources` |

---

## 12. Success criteria

### Minimum viable offline compendium (Phase 1 complete)

- [ ] All Tier A sources acquired by single `update_compendium_offline_sources.sh --all`
- [ ] `cover_url` populated for majority of cartridge/handheld titles (libretro-thumbnails)
- [ ] LaunchBox `Metadata.xml` auto-downloaded
- [ ] Runtime artwork resolves local PNG without network for matched games
- [ ] Build report documents artwork coverage %

### Full offline compendium (Phase 3 complete)

- [ ] `--strict-offline` build produces DB with no enricher HTTP calls
- [ ] Field coverage meets or exceeds completeness-plan targets for genre, description,
      developer, publisher, year on core systems
- [ ] `cover_url` coverage ≥ 70% overall (stretch: 85%+ on core systems)
- [ ] Remus CLI/GUI match + enrich works on air-gapped machine with only `data/` + DB

---

## 13. Open decisions

| # | Question | Recommendation |
|---|----------|----------------|
| D1 | Default thumbnail scope: `--all` vs `--core`? | `--all` for production; `--core` for CI |
| D2 | Store `cover_url` as relative path or `file://`? | **Relative path** |
| D3 | Persist snap/title in schema now or later? | **Later** (game_facts or Phase 3 migration) |
| D4 | IGDB snapshot before or after ScreenScraper? | **IGDB first** (metadata); SS for art gaps |
| D5 | Commit `offline-sources.json` markers to repo? | No — markers gitignored; manifest only |

---

## 14. References

- [libretro-thumbnails (active)](https://github.com/libretro-thumbnails/libretro-thumbnails)
- [libretro-thumbnails (archived — do not use)](https://github.com/libretro/libretro-thumbnails)
- [RetroArch thumbnails guide](https://docs.libretro.com/guides/roms-playlists-thumbnails/)
- [thumbnails.libretro.com CDN](https://thumbnails.libretro.com/)
- [LaunchBox Metadata.zip](https://gamesdb.launchbox-app.com/Metadata.zip)
- [RomM metadata providers](https://docs.romm.app/latest/Getting-Started/Metadata-Providers/)
- [Hasheous dumps API](https://hasheous.org/)
- [IGDB API docs](https://api-docs.igdb.com/)
- [ScreenScraper API](https://www.screenscraper.fr/webapi2.php)

---

*Last updated: 2026-06-23. Status: **Planning** — implementation not started.*
