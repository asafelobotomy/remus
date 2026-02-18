# Provider API Research Report

**Date**: 2025  
**Scope**: Comprehensive audit of all metadata provider implementations against actual API documentation  
**Status**: Critical bugs found and fixed

---

## Executive Summary

Research revealed **critical endpoint bugs** in 2 of 4 providers (Hasheous and ScreenScraper) that cause all API lookups to fail with 404 errors. Additionally, analysis of the open-source RomM project revealed the correct API patterns for both services. Fixes are detailed below.

---

## Provider-by-Provider Analysis

### 1. Hasheous (Priority 100 — Hash-only)

**What it does**: Free community hash-matching service. Maps ROM hashes (MD5/SHA1/CRC) to game metadata. Proxies IGDB/TheGamesDB/RetroAchievements data. No API key required.

**Swagger docs**: https://hasheous.org/swagger/index.html

#### Bugs Found (CRITICAL)

| Bug | Current Code | Correct API |
|-----|-------------|-------------|
| Wrong domain in api.h | `https://hasheous.com` | `https://hasheous.org` |
| Wrong endpoint format | `GET /lookup?hash=XXX&hash_type=YYY` (query params) | `POST /Lookup/ByHash` with JSON body |
| Wrong response parsing | Expects `json["data"]` wrapper | Returns flat response with `id`, `name`, `metadata`, `signatures`, `attributes` |
| Missing CRC32 support | Only detects MD5 (32 chars) and SHA1 (40 chars) | CRC32 (8 chars) also supported |

#### Correct API (from Hasheous Swagger + RomM's hasheous_handler.py)

**Hash Lookup** — `POST /api/v1/Lookup/ByHash`
```
URL: https://hasheous.org/api/v1/Lookup/ByHash?returnAllSources=true&returnFields=Signatures,Metadata,Attributes
Method: POST
Content-Type: application/json
Body: {"mD5": "hash_here", "shA1": "hash_here", "crc": "hash_here"}
```

Also available as individual GET endpoints:
- `GET /api/v1/Lookup/ByHash/md5/{md5}`
- `GET /api/v1/Lookup/ByHash/sha1/{sha1}`
- `GET /api/v1/Lookup/ByHash/crc/{crc}`

**Response** returns: `id`, `name`, `metadata[]` (with `source` field: "IGDB"/"TheGamesDB"/"RetroAchievements"), `signatures[]` (with DAT match info: No-Intro/Redump/TOSEC), `attributes[]` (with artwork links).

**MetadataProxy** — enables fetching full IGDB metadata through Hasheous without separate API keys:
- `GET /api/v1/MetadataProxy/IGDB/Game/{igdb_id}?fields=...`
- `GET /api/v1/MetadataProxy/IGDB/Cover/{cover_id}`
- `GET /api/v1/MetadataProxy/TheGamesDB/Game/{tgdb_id}`

#### Key Insight from RomM

RomM's `hasheous_handler.py` shows the correct workflow:
1. POST to `/Lookup/ByHash` with `mD5`/`shA1`/`crc` in body
2. Extract IGDB ID from `metadata[]` where `source == "IGDB"` → get `immutableId`
3. Use MetadataProxy to fetch full IGDB metadata: `GET /MetadataProxy/IGDB/Game/{igdb_id}`
4. Also extract TheGamesDB ID and RetroAchievements ID from response

---

### 2. ScreenScraper (Priority 90 — Hash + Name)

**What it does**: Comprehensive ROM database with hash-based identification AND name search. Requires developer credentials AND user account.

**API docs**: https://www.screenscraper.fr/webapi2.php

#### Bugs Found (CRITICAL)

| Bug | Current Code | Correct API |
|-----|-------------|-------------|
| Wrong base URL | `https://www.screenscraper.fr/api2` | `https://api.screenscraper.fr/api2` |
| Wrong hash endpoint | `/getsearchres.php` (doesn't exist) | `/jeuInfos.php` |
| Wrong name search endpoint | `/getsearchres.php` (doesn't exist) | `/jeuRecherche.php` |
| Missing ROM metadata params | Only sends hash + systemeid | Should also send `romnom` (filename) and `romtaille` (file size in bytes) |

#### Correct API

**Hash Lookup** — `GET /jeuInfos.php`
```
URL: https://api.screenscraper.fr/api2/jeuInfos.php
Params:
  devid=XXX            (required: developer ID)
  devpassword=YYY      (required: developer password)
  softname=Remus       (required: application name)
  output=json          (required: response format)
  ssid=USER            (required: user account)
  sspassword=PASS      (required: user password)
  crc=XXXXXXXX         (CRC32 hash)
  md5=XXXXXXXX...      (MD5 hash)
  sha1=XXXXXXXX...     (SHA1 hash)
  systemeid=1          (optional: system ID for disambiguation)
  romtype=rom          (optional: "rom" or "iso")
  romnom=filename.zip  (optional: original filename)
  romtaille=749652     (optional: file size in bytes)
```

**Name Search** — `GET /jeuRecherche.php`
```
URL: https://api.screenscraper.fr/api2/jeuRecherche.php
Params:
  devid, devpassword, softname, output, ssid, sspassword (same as above)
  recherche=SearchTerm (required: search query)
  systemeid=1          (optional: narrow by system)
```
Returns up to 30 results sorted by match probability.

**Error Codes**: 400 (missing fields/bad hash), 401 (API closed), 403 (bad dev creds), 404 (not found), 429 (rate limit/quota exceeded)

#### Key Insight from RomM

RomM's `ss_handler.py` shows:
1. Hash lookup uses `get_game_info()` sending all three hashes (md5, sha1, crc) plus file size
2. Selects the **largest file** for hash lookups (most likely main ROM)
3. Name search uses `search_games()` with URL-encoded unidecoded term
4. Applies fuzzy matching on returned results to find best match

---

### 3. TheGamesDB (Priority 50 — Name-only)

**What it does**: Game database with name + platform search. No hash matching.

**API docs**: https://api.thegamesdb.net (Swagger UI)

#### Status: Implementation mostly correct but needs API key

| Issue | Details |
|-------|---------|
| API key required | Register at https://api.thegamesdb.net/key.php (free after login) |
| Endpoint version | Current: `/v1/Games/ByGameName` — v1.1 also available for same endpoint |
| No hash matching | Correctly returns error for hash lookups |

The `thegamesdb_provider.cpp` implementation is functionally correct. The main issue is that **without an API key, requests will fail**. Users need to register for a free key.

---

### 4. IGDB (Priority 40 — Name-only)

**What it does**: Twitch-backed game database. Name search using Apicalypse query language.

**API docs**: https://api-docs.igdb.com

#### Status: Implementation correct

| Aspect | Status |
|--------|--------|
| Twitch OAuth | Correctly implements `client_credentials` flow |
| Apicalypse queries | Correctly formats `search "title"; fields ...;` |
| No hash matching | Correctly returns error |

The `igdb_provider.cpp` implementation is correct. Requires Twitch developer account (needs 2FA/phone number).

---

### 5. MobyGames (Not Implemented)

**What it does**: Oldest and largest video game database. Name search only, no hash matching.

**API**: https://www.mobygames.com/info/api/

| Aspect | Details |
|--------|---------|
| Auth | Paid subscription required (non-commercial only) |
| Hash matching | Not supported |
| Recommendation | **Do not implement** — paid, non-commercial restriction, no hash matching, lower value than existing providers |

---

## Open-Source Project Analysis

### RomM (rommapp/romm)

The most relevant reference project. Key architecture patterns:

1. **Multi-handler metadata system**: Separate handlers for IGDB, ScreenScraper, Hasheous, TheGamesDB, MobyGames, RetroAchievements, LaunchBox, SGDB, Flashpoint, HLTB
2. **Hash-first identification**: Uses Hasheous as the hash→game ID bridge, then fetches rich metadata from IGDB/TheGamesDB via Hasheous MetadataProxy
3. **File selection**: For multi-file games, selects the largest file for hashing (most accurate)
4. **DATfile matching**: v4.0.0 added hash-based matching against No-Intro/Redump/TOSEC DAT databases for verification
5. **Platform slug system**: Universal platform slugs for cross-provider platform mapping

### Key Pattern: Hasheous as Hub

RomM's approach is to use Hasheous as a **metadata hub**:
1. Hash ROM → POST to Hasheous `/Lookup/ByHash`
2. Get back IGDB ID, TheGamesDB ID, RetroAchievements ID
3. Use Hasheous MetadataProxy to get full IGDB metadata without needing separate Twitch OAuth
4. This means you can get rich IGDB metadata **for free, without any API key**

**Recommendation for Remus**: Implement the same pattern. After Hasheous hash lookup, use the returned IGDB ID with MetadataProxy to get full metadata. This eliminates the need for separate IGDB/TheGamesDB credentials for hash-matched ROMs.

---

## Fixes Applied

### 1. api.h Constants

Fixed all wrong URLs:
- `SCREENSCRAPER_BASE_URL`: `www.screenscraper.fr` → `api.screenscraper.fr`
- `SCREENSCRAPER_GETSEARCHRES_ENDPOINT` → Split into `SCREENSCRAPER_JEUINFOS_ENDPOINT` (`/jeuInfos.php`) and `SCREENSCRAPER_JEURECHERCHE_ENDPOINT` (`/jeuRecherche.php`)
- `HASHEOUS_BASE_URL`: `hasheous.com` → `hasheous.org`
- `HASHEOUS_LOOKUP_ENDPOINT`: `/api/v1/lookup` → `/api/v1/Lookup/ByHash`
- Added new Hasheous MetadataProxy endpoint constants

### 2. hasheous_provider.cpp

- Rewrote `getByHash()` to use POST with JSON body (`mD5`/`shA1`/`crc` fields)
- Added `makePostRequest()` for JSON POST requests
- Added CRC32 (8 char) hash detection
- Updated `parseGameJson()` to match actual Hasheous response format
- Added MetadataProxy support: after hash match, fetches full IGDB metadata
- Added `fetchIgdbMetadata()` for MetadataProxy calls

### 3. screenscraper_provider.cpp

- `getByHash()`: Changed endpoint from `/getsearchres.php` to `/jeuInfos.php`
- `searchByName()`: Changed endpoint from `/getsearchres.php` to `/jeuRecherche.php`
- Updated base URL references to use `api.screenscraper.fr`
- Added `romtaille` (file size) parameter support to `getByHash()` signature

---

## Future Improvements

1. **Hasheous MetadataProxy integration**: Use returned IGDB/TheGamesDB IDs to fetch rich metadata through Hasheous without separate API keys
2. **Multi-hash submissions**: Send all available hashes (CRC+MD5+SHA1) in single Hasheous POST
3. **File size parameter**: Pass ROM file size to ScreenScraper for better hash disambiguation
4. **ROM filename parameter**: Pass original ROM filename to ScreenScraper (improves matching)
5. **DAT file matching**: Consider local No-Intro/Redump DAT file support (like RomM v4.0.0)
6. **RetroAchievements ID**: Store RA IDs returned by Hasheous for future RA integration
