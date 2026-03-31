# Metadata Providers

## Use the current provider stack

Remus ships with a mixed offline and online metadata strategy. The active CLI wiring builds the provider chain in code, then uses priority and capability to decide fallback order.

Default provider order in the CLI:
1. Local DAT databases when local data is present.
2. Hasheous.
3. ScreenScraper when credentials are supplied.
4. GameTDB when local GameTDB data is present.
5. TheGamesDB.
6. IGDB when credentials are supplied.
7. RetroAchievements when credentials are supplied.
8. Wikidata.

## Know what each provider does

### Local DAT databases
- Matching: hash and structured offline metadata
- Auth: none
- Best use: first pass when local DAT files are available
- Notes: highest-priority offline source in the CLI setup

### Hasheous
- Matching: hash-based lookup with CRC32, MD5, and SHA1 support in the current provider
- Auth: none for core hash lookup
- Best use: no-auth online hash matching
- Notes: optional MetadataProxy enrichment needs a client API key; plain hash lookup does not

### ScreenScraper
- Matching: hash and name
- Auth: required
- Best use: high-confidence verified dump matching when the user supplies credentials
- Notes: uses query parameters for the upstream API contract, so treat logs and diagnostics carefully

### GameTDB
- Matching: local database lookup for supported systems
- Auth: none
- Best use: Nintendo and selected console metadata from bundled or downloaded local databases

### TheGamesDB
- Matching: name-based search
- Auth: optional API key
- Best use: general name fallback and artwork retrieval

### IGDB
- Matching: name-based search
- Auth: required Twitch client credentials
- Best use: rich metadata when hash lookup is unavailable or insufficient

### RetroAchievements
- Matching: hash-based lookup for supported ROMs
- Auth: required username and API key
- Best use: achievement-linked metadata and verification for supported titles
- Notes: current provider uses MD5-oriented lookup paths

### Wikidata
- Matching: low-priority name-based fallback
- Auth: none
- Best use: last-resort metadata enrichment from a public source

## Choose a practical fallback strategy

Recommended operating model:
- Prefer local DAT data for repeatable offline matching.
- Use Hasheous for the default online hash fallback because it works without user credentials.
- Add ScreenScraper when you need stronger artwork coverage or authenticated hash lookup.
- Keep TheGamesDB, IGDB, RetroAchievements, and Wikidata as secondary enrichment layers.

## Configure providers deliberately

- ScreenScraper needs user and developer credentials.
- IGDB needs Twitch client credentials.
- RetroAchievements needs username and API key.
- Hasheous hash lookup works without auth, but MetadataProxy enrichment uses a client API key when present.
- TheGamesDB can run without an API key, but an API key is supported.

## Prefer local data when possible

Repository data directories that feed provider behavior:
- `data/databases/` for DAT-backed offline matching
- `data/metadata/` for local metadata enrichment
- `data/gametdb/` for GameTDB databases

If you need the exact runtime wiring, see the provider construction flow in `src/cli/cli_helpers.cpp`.
