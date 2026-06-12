# Metadata Providers

## Use the current provider stack

Remus uses a **two-phase metadata waterfall**: all **local** providers run before any
network call. Within each phase, providers are sorted by **priority** (higher first).

Runtime wiring: `buildOrchestrator()` in `src/cli/cli_helpers_providers.cpp` and
`AppController::rebuildOrchestrator()` in the GUI.

### Execution order (when all providers are registered)

**Pass 1 — compendium identity**

1. Compendium — establish identity, then gap-fill from other providers (excluding compendium repeats).

**Pass 2 — legacy waterfall** (local, then remote, until enriched or hash identity resolved)

| Phase | Order | Provider | Priority | Auth |
|-------|-------|----------|----------|------|
| Local | 1 | Compendium | 210 | none |
| Local | 2 | GameTDB | 150 | none (local XML) |
| Remote | 1 | Hasheous | 91 | none (hash); optional MetadataProxy key |
| Remote | 2 | ScreenScraper | 89 | required |
| Remote | 3 | PlayMatch | 88 | none on public instance |
| Remote | 4 | IGDB | 70 | Twitch OAuth |
| Remote | 5 | RetroAchievements | 60 | username + API key |
| Remote | 6 | TheGamesDB | 50 | optional API key |
| Remote | 7 | Wikidata | 40 | none |

Compendium and GameTDB are omitted from pass 2 if pass 1 already resolved identity.
Remote providers are skipped once a definitive hash match (score ≥ 1.0) is found,
unless the caller still requires artwork.

### Hash lookup order (offline catalogs)

Official compendium signatures, patch entries, and metadata hash cascades share one
digest try-order (see [ROM-MATCHING-AUDIT.md §2.1](reports/ROM-MATCHING-AUDIT.md#21-canonical-hash-cascade)):

```text
sha256 → <system preferred> → sha1 → md5 → crc32
```

`selectBestHash()` picks a single digest for **cache keys** only; the orchestrator
still passes all calculated digests into the cascade.

## Know what each provider does

### Compendium

- Matching: multi-hash cascade, serial, patch catalog fallback, multi-signal (`matchROM`), FTS name
- Auth: none
- Best use: first-pass offline matching when `data/compendium/remus_compendium.db` is present

### GameTDB

- Matching: multi-hash cascade for supported systems; name search
- Auth: none
- Best use: Nintendo / PS3 metadata from `data/gametdb/`

### Hasheous

- Matching: multi-hash POST (CRC32 + MD5 + SHA1)
- Auth: none for hash lookup; optional client API key for MetadataProxy IGDB enrichment
- Best use: default online hash fallback without user credentials

### ScreenScraper

- Matching: hash and name
- Auth: required
- Best use: artwork and authenticated hash lookup when credentials are supplied

### PlayMatch

- Matching: hash + filename + file size via RetroRealm identify API; IGDB metadata via proxy
- Auth: none on the public instance (`playmatch.retrorealm.dev`)
- Best use: secondary hash→IGDB bridge after Hasheous / ScreenScraper miss
- Notes: requires `fileName` and `fileSize` from the match path; hash-only CLI queries skip it

### IGDB

- Matching: name-based search
- Auth: required Twitch client credentials
- Best use: rich metadata when hash lookup is unavailable or insufficient

### RetroAchievements

- Matching: MD5 hash lookup via RA API (`dorequest.php?r=gameid`)
- Auth: required username and API key
- Best use: achievement-linked metadata when the ROM's MD5 is in RA's hash list
- Notes: RA uses RAHasher MD5 (stored separately as `ra_md5` on file records). The orchestrator passes `raMd5` to RA, not No-Intro `md5`. When Hasheous/compendium already resolved an RA game ID, the provider uses `getById` instead of hash lookup. Optional external tool: set `REMUS_RAHASHER_PATH` to an RAHasher binary for disc systems (PS1/PS2/NDS/GC/Wii).

### TheGamesDB

- Matching: name-based search
- Auth: optional API key
- Best use: name fallback and artwork

### Wikidata

- Matching: low-priority name fallback
- Auth: none
- Best use: last-resort enrichment

## Verification vs matching

`--verify` uses the same hash cascade for **official** and **patch** catalogs but does
**not** call metadata providers, serial-only paths, or name search. A ROM matched by
serial in `--match` may still show `NotInDat` in verify if no catalog hash matches.

See [ROM-MATCHING-AUDIT.md](reports/ROM-MATCHING-AUDIT.md) for the full dual-pipeline audit.

## Configure providers deliberately

- ScreenScraper: user + developer credentials
- IGDB: Twitch client ID + secret
- RetroAchievements: username + API key
- Hasheous: optional `hasheous/client_api_key` for MetadataProxy enrichment
- TheGamesDB: optional API key

## Prefer local data when possible

- `data/compendium/` — bundled offline compendium
- `data/metadata/` — Libretro metadata enrichment
- `data/gametdb/` — GameTDB XML databases

Legacy `LocalDatabaseProvider` code remains for compatibility tests but is not part of
the default CLI orchestrator (compendium-backed multi-signal matching replaced it).

For compendium build-time enrichment sources (including Hasheous bulk `igdb_id` pass),
see [COMPENDIUM-DATA-SOURCES.md](reports/COMPENDIUM-DATA-SOURCES.md).
