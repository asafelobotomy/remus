# Metadata Sources Research: API-Free ROM Identification

**Research Date:** February 2026  
**Objective:** Identify free/no-API-required metadata sources for ROM identification similar to ES-DE and RetroArch

## Executive Summary

Modern emulator frontends (ES-DE, RetroArch, RetroPie) primarily use **offline local databases** combined with **optional online APIs**. The key insight is that **checksum-based matching using local DAT files requires no API keys** and provides the most definitive matches. Online APIs are supplementary for metadata enrichment (descriptions, artwork).

## Key Findings

### 1. Primary Approach: Local DAT Files (No API Required) ✅

RetroArch's core strategy uses **completely offline local databases**:

- **Source:** [libretro-database](https://github.com/libretro/libretro-database)
- **Format:** `.rdb` files (compiled from `.dat` files)
- **Content:** CRC32, MD5, SHA1 checksums + basic metadata (name, year, publisher, developer, genre)
- **Coverage:** 100+ systems from No-Intro, Redump, TOSEC, GameTDB
- **Cost:** Completely free, no API keys, no rate limits
- **Usage:** Download `.rdb` files or compile from `.dat` files locally

#### How It Works
```
1. Calculate ROM hash (CRC32/MD5/SHA1)
2. Search local .rdb database for matching hash
3. Return game metadata (name, year, publisher, etc.)
4. Optional: Query online API for enhanced metadata/artwork
```

#### Database Sources
- **No-Intro** - Cartridge-based systems (NES, SNES, Genesis, Game Boy, etc.)
  - Uses CRC32 checksums
  - Most comprehensive for pre-disc era
  - Available at: http://datomatic.no-intro.org
  
- **Redump** - Disc-based systems (PlayStation, Saturn, Dreamcast, GameCube, etc.)
  - Uses MD5/SHA1 checksums
  - Gold standard for optical media preservation
  - Available at: http://redump.org
  
- **TOSEC** - Broader coverage including computers (Amiga, Commodore, etc.)
  - Fallback for systems not covered by No-Intro/Redump
  - Available at: https://www.tosecdev.org/

### 2. ScreenScraper (Free Tier Available) ✅

**Primary API used by ES-DE, RetroPie, and many ROM managers**

- **URL:** https://www.screenscraper.fr/
- **Authentication:** Free account (username/password) - NO API key required
- **Rate Limits (Free Tier):**
  - ~1 request/second on free tier
  - 30 million API requests handled daily (infrastructure proven)
  - Slow but functional for personal use
- **Paid/Donation Tiers:** Remove rate limits, faster responses
- **Matching Methods:**
  - CRC32/MD5/SHA1 hash matching (primary)
  - Filename matching (fallback)
  - ROM size matching
  - Serial number matching (for disc-based systems)
- **Metadata Provided:**
  - Game description (multiple languages)
  - Release dates, publishers, developers, genres
  - **Extensive artwork:** Box art (front/back), screenshots, wheels, logos, bezels, videos
  - Ratings from multiple sources
  - Region information
- **Why It's Popular:**
  - Free tier actually works (unlike TheGamesDB which now requires API key)
  - Most complete retro game database
  - Hash-based matching means accurate identification
  - Community-driven database (Creative Commons)

**Implementation Notes:**
- Requires user registration (can be done programmatically in app setup)
- API documentation: https://www.screenscraper.fr/webapi.php
- RetroArch scraper implementation: https://github.com/libretro/RetroArch/tree/master/network

### 3. Hasheous (Already Implemented) ✅

**Current implementation status: Already in use**

- **Architecture:** Self-hosted hash database lookup service
- **Advantage:** No external dependencies, privacy-focused
- **Limitation:** Database completeness depends on what's indexed
- **Best Use:** Primary hash lookup, fallback to other sources when not found

### 4. Additional Free/Low-Barrier Sources

#### TheGamesDB (Now Requires API Key) ⚠️
- **Status:** Changed to require API key (previously free)
- **Registration:** Free API key available at https://thegamesdb.net/
- **Limitation:** Name-based matching only (no hash support)
- **Coverage:** Good for metadata/artwork, poor for ROM identification
- **Conclusion:** Not ideal as primary source, better as fallback for name-based enrichment

#### IGDB (Requires Twitch OAuth) ⚠️
- **Authentication:** Twitch Client ID + Secret required
- **Barrier:** Higher than ScreenScraper (OAuth flow vs simple account)
- **Coverage:** Excellent modern games, weaker retro coverage
- **Conclusion:** Keep as optional fallback

#### OpenVGDB (Discontinued) ❌
- **Status:** Project archived/discontinued
- **Reason:** Superseded by libretro-database
- **Conclusion:** Not recommended

### 5. Hybrid Approach: Best Practices from the Industry

**How ES-DE Does It:**
1. **Primary:** ScreenScraper API (free account)
2. **Fallback:** Local filename parsing
3. **User Option:** Disable online scraping, use only local databases

**How RetroArch Does It:**
1. **Primary:** Local `.rdb` database lookup (hash-based, offline)
2. **Secondary:** Optional online scraper (ScreenScraper supported)
3. **Manual:** User can manually input metadata

**How RetroPie Does It:**
1. **Built-in:** ScreenScraper integration (settings menu)
2. **External:** Users can use Skraper tool (desktop app using ScreenScraper)
3. **Fallback:** Manual gamelist.xml editing

## Recommended Implementation Strategy for Remus

### Phase 1: Offline-First (No API Required)
**Goal:** Match and identify ROMs without any external API calls

1. **Download libretro `.rdb` databases** during app installation/first run
   - Source: https://github.com/libretro/libretro-database/tree/master/rdb
   - ~100MB total for all systems
   - Bundle common systems (NES, SNES, Genesis, PS1, etc.) with app
   
2. **Implement local RDB parser**
   - Parse `.rdb` binary format (or use DAT XML files)
   - Index by CRC32 (cartridges), MD5/SHA1 (discs), serial number
   - Store in SQLite for fast lookup
   
3. **Multi-method matching pipeline** (no API needed):
   ```
   Priority 1: Hash match in local database (CRC32/MD5/SHA1)
   Priority 2: Serial number match (for disc-based systems)  
   Priority 3: Exact filename match in local database
   Priority 4: Fuzzy filename match (Levenshtein distance)
   Priority 5: User confirmation
   ```

4. **Update mechanism:**
   - Check for libretro-database updates monthly
   - Download updated `.rdb` files in background
   - No user intervention required

### Phase 2: Optional Online Enhancement
**Goal:** Enrich metadata with descriptions, artwork (user opt-in)

1. **ScreenScraper integration (free tier)**
   - Prompt user to create free account on first run
   - Store credentials locally (encrypted)
   - Use only for:
     - Extended descriptions (not in RDB files)
     - High-quality artwork downloads
     - Missing games not in local database
   - Respect rate limits (1 req/sec)
   - Cache all responses locally

2. **Graceful degradation:**
   - If ScreenScraper fails → use local database results
   - If rate limited → queue requests for later
   - If no account → show "Create free ScreenScraper account for artwork"

3. **User privacy controls:**
   - Setting: "Use only local databases (no internet)"
   - Setting: "Enable online enrichment (ScreenScraper)"
   - Clear indication when online APIs are queried

### Phase 3: Combination Matching
**Goal:** Maximum accuracy using multiple signals

```cpp
struct MatchSignals {
    float hashMatch;      // 100% if exact CRC32/MD5/SHA1 match
    float filenameMatch;  // 80-100% fuzzy score
    float sizeMatch;      // 70-100% if file size matches
    float serialMatch;    // 100% if disc serial matches
    float regionMatch;    // Bonus if region matches user preference
};

MatchConfidence calculateConfidence(MatchSignals signals) {
    // Hash match is definitive
    if (signals.hashMatch == 100) return MatchConfidence::Perfect;
    
    // Multiple weak signals can confirm
    float combined = weighted_average(signals);
    if (combined >= 90) return MatchConfidence::High;
    if (combined >= 70) return MatchConfidence::Medium;
    return MatchConfidence::Low;
}
```

## Implementation Details

### Parsing libretro `.rdb` Files

**Option A: Use DAT files (XML)**
```cpp
// Parse No-Intro/Redump DAT files (ClrMamePro format)
// Example: Sega - Mega Drive - Genesis.dat
/*
game (
    name "Sonic The Hedgehog (USA, Europe)"
    description "Sonic The Hedgehog (USA, Europe)"
    rom ( name "Sonic The Hedgehog (USA, Europe).md" 
          size 524288 
          crc f9394e97 
          md5 1bc674be034e43c96b86487ac69d9293 
          sha1 6ddb7de1e17e7f6cdb88927bd906352030daa194 )
)
*/
```

**Option B: Convert RDB to SQLite**
```bash
# Use libretro's own tools
git clone https://github.com/libretro/RetroArch
cd RetroArch/libretro-db
make
./rdb_tool create output.rdb input.dat
./rdb_tool list output.rdb
```

**Option C: Pre-converted database**
- Ship SQLite database compiled from RDB files
- Update quarterly with new releases

### ScreenScraper API Example

```cpp
// Hash-based lookup (most accurate)
GET https://www.screenscraper.fr/api2/jeuInfos.php
    ?devid=xxx&devpassword=xxx
    &softname=Remus
    &output=json
    &crc=f9394e97
    &md5=1bc674be034e43c96b86487ac69d9293
    &sha1=6ddb7de1e17e7f6cdb88927bd906352030daa194
    &systemeid=1  // Mega Drive

// Response includes:
{
    "response": {
        "jeu": {
            "id": "12345",
            "noms": [{"text": "Sonic The Hedgehog", "region": "wor"}],
            "synopsis": [{"text": "...", "langue": "en"}],
            "dates": [{"text": "1991-06-23", "region": "us"}],
            "editeur": {"text": "Sega"},
            "developpeur": {"text": "Sonic Team"},
            "genres": [{"text": "Platform"}],
            "medias": [
                {"type": "box-2D", "url": "..."},
                {"type": "screenshot", "url": "..."},
                {"type": "video", "url": "..."}
            ]
        }
    }
}
```

### Database Size Estimates

| Database | Systems | File Size | Format |
|----------|---------|-----------|--------|
| **libretro RDB** (all systems) | 100+ | ~100MB | Binary |
| **No-Intro DAT** (common consoles) | 40 | ~50MB | XML |
| **Redump DAT** (disc systems) | 30 | ~80MB | XML |
| **Combined indexed SQLite** | 100+ | ~200MB | SQLite |

## Advantages of This Approach

### For Users
✅ **Works offline** - No API keys required for basic functionality  
✅ **Fast** - Local database lookups are instant  
✅ **Accurate** - Hash-based matching is definitive  
✅ **Privacy** - No data sent to external servers unless opt-in  
✅ **Reliable** - No rate limits or API downtime for core features

### For Development
✅ **No API key management** - libretro databases are public domain  
✅ **Proven approach** - Used by RetroArch (millions of users)  
✅ **Future-proof** - Local databases won't break if APIs change  
✅ **Optional enhancement** - ScreenScraper free tier for artwork/descriptions  
✅ **Legally safe** - All databases are community-driven, freely licensed

### For Accuracy
✅ **Multi-tier matching**:
1. **Hash match (100% confidence)** - CRC32/MD5/SHA1 against known good dumps
2. **Size + filename (90% confidence)** - Multiple signals corroborate
3. **Filename only (70% confidence)** - Pattern matching with known names
4. **Fuzzy matching (50% confidence)** - Levenshtein distance for variations

## Comparison with Current Remus Implementation

| Feature | Current (Remus) | Proposed (Offline-First) |
|---------|-----------------|--------------------------|
| **Primary Method** | Online APIs only | Local database matching |
| **Hasheous** | ✅ Priority 100 | ✅ Keep as-is |
| **ScreenScraper** | ⚠️ Disabled (auth issues) | ✅ Optional free account |
| **TheGamesDB** | ⚠️ Broken (needs API key) | ❌ Remove or make optional |
| **IGDB** | ⚠️ Complex OAuth | ❌ Keep as advanced option |
| **Offline Mode** | ❌ Fails without API | ✅ Fully functional |
| **Accuracy** | Hash-only | Hash + size + filename + serial |
| **Speed** | Network dependent | Instant (local) |
| **Database Size** | ~0 (no local DB) | ~200MB indexed |

## Action Items

### Immediate (M10)
1. ✅ Research complete - this document
2. 📋 Download sample libretro `.rdb`/`.dat` files for testing
3. 📋 Implement RDB/DAT parser (choose XML DAT format for simplicity)
4. 📋 Create `LocalDatabaseProvider` class
5. 📋 Add libretro database to matching pipeline (priority 100, above Hasheous)

### Near-Term (M11)
1. 📋 Bundle common system databases with app (NES, SNES, Genesis, PS1, N64)
2. 📋 Implement database update mechanism
3. 📋 Add ScreenScraper free-tier support (optional, user opt-in)
4. 📋 UI setting: "Enable online metadata enrichment (requires free ScreenScraper account)"

### Long-Term
1. 📋 Database download manager (let users pick which systems to index)
2. 📋 Database statistics/coverage report in UI
3. 📋 Export matched games to libretro playlist format (for RetroArch compatibility)

## References

- **libretro-database:** https://github.com/libretro/libretro-database
- **libretro-database README:** https://github.com/libretro/libretro-database/blob/master/README.md
- **RetroArch Database Docs:** https://docs.libretro.com/guides/databases/
- **ScreenScraper:** https://www.screenscraper.fr/
- **ScreenScraper API Docs:** https://www.screenscraper.fr/webapi.php
- **No-Intro:** http://datomatic.no-intro.org
- **Redump:** http://redump.org
- **TOSEC:** https://www.tosecdev.org/
- **RetroScraper (Python):** https://github.com/zayamatias/retroscraper
- **oxyROMon (Rust):** https://github.com/alucryd/oxyromon

## Conclusion

The key insight is that **Modern ROM managers don't rely primarily on online APIs**. They use:

1. **Local checksum databases** (No-Intro/Redump) - Free, offline, accurate
2. **Optional online enrichment** (ScreenScraper free tier) - For artwork and descriptions
3. **Multi-signal matching** - Hash, filename, size, serial number combined

Remus should adopt this proven approach:
- ✅ **Phase 1:** Local database matching (works offline, no API keys)
- ✅ **Phase 2:** Optional ScreenScraper free account (user opt-in for artwork)
- ✅ **Phase 3:** Multi-method confidence scoring

This provides:
- **Better user experience** (works without setup)
- **Higher accuracy** (hash-based matching)
- **Future-proof architecture** (local databases won't break)
- **Optional enhancement** (online APIs for rich metadata)
