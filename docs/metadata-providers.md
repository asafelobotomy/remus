# Metadata Provider Research & Recommendations

## Overview

This document provides comprehensive research on metadata providers for ROM identification, including hash-based and name-based providers discovered through community research.

## Hash-Based Providers (Best for Accuracy)

### 1. ScreenScraper ⭐ PRIMARY (Implemented in M2)
- **URL**: https://www.screenscraper.fr/
- **Hash Support**: ✅ CRC32, MD5, SHA1
- **Authentication**: Required (devid, devpassword, ssid, sspassword)
- **Rate Limit**: 1 req/2s, 10k req/day (varies by account tier)
- **Cost**: Free with registration, paid tiers for higher limits
- **Coverage**: 125+ systems
- **Data Quality**: Excellent - works with No-Intro/Redump databases
- **Metadata**: Title, description, publisher, developer, release date, genres, players, rating, region
- **Artwork**: Box art (front/back), screenshots, videos, wheels, logos, 3D boxes, cartridge/CD art
- **Special Features**: Multi-language support, manual PDFs
- **API Documentation**: https://www.screenscraper.fr/api2/
- **Best For**: Hash-based matching with verified No-Intro/Redump dumps
- **Implementation Status**: ✅ Complete

### 2. Hasheous ⭐ EXCELLENT FALLBACK (Not Yet Implemented)
- **URL**: https://hasheous.org/
- **Hash Support**: ✅ MD5, SHA1
- **Authentication**: None required
- **Rate Limit**: Standard rate limiting (533ms avg response time)
- **Cost**: FREE
- **Coverage**: Proxies IGDB (135+ systems)
- **Data Quality**: Excellent - community-voted corrections
- **Metadata**: Proxies IGDB data (title, description, genres, release date)
- **Artwork**: Cover art (proxied from IGDB)
- **Special Features**: 
  - No API key required (friction-free)
  - Community voting system for match corrections
  - Works with No-Intro and Redump hash databases
  - RetroAchievements ID linking
  - Open-source
- **API**: Public REST API
- **Stats**: 14M+ requests served, 7k+ unique visitors
- **Best For**: Users who want hash-based matching without API credentials
- **Implementation Priority**: HIGH - Should be added as primary hash fallback
- **Source**: https://github.com/hasheous (open-source)

### 3. RetroAchievements (Not Yet Implemented)
- **URL**: https://retroachievements.org/
- **Hash Support**: ✅ Custom per-system (see Game Identification docs)
- **Authentication**: Required (RETROACHIEVEMENTS_API_KEY)
- **Rate Limit**: Standard tier
- **Cost**: FREE
- **Coverage**: 70+ systems with achievements
- **Data Quality**: Good - compatible with No-Intro hashes
- **Metadata**: Title, achievement data, ROM verification status
- **Special Features**:
  - Custom hash algorithms optimized per system
  - Achievement progress tracking
  - Compatible with No-Intro hashes
  - RAHasher tool for batch verification
- **API Documentation**: https://retroachievements.org/API/
- **Best For**: Users who want achievements + hash verification
- **Implementation Priority**: MEDIUM - Nice-to-have for achievement integration

### 4. PlayMatch (Not Yet Implemented)
- **Hash Support**: ✅ Hash-based matching
- **Authentication**: API endpoint configuration
- **Cost**: FREE (community-hosted)
- **Coverage**: Works with IGDB
- **Special Features**: Community-driven matching for unmatched files
- **Best For**: Filling gaps where ScreenScraper/Hasheous fail
- **Implementation Priority**: LOW - Nice-to-have fallback

### 5. OpenVGDB (Not Yet Implemented)
- **Type**: Offline downloadable database
- **Hash Support**: ✅ ROM hash to game metadata mapping
- **Authentication**: None (local database)
- **Cost**: FREE
- **Coverage**: Multiple systems
- **Data Quality**: Good - community maintained
- **Special Features**:
  - Offline operation (no API calls)
  - SQLite database format
  - Used by various emulators
- **Source**: https://github.com/OpenVGDB/OpenVGDB
- **Best For**: Offline mode, reducing API calls
- **Implementation Priority**: LOW - Future enhancement

## Name-Based Providers (Fallback Only)

### 6. TheGamesDB (Implemented in M2)
- **URL**: https://thegamesdb.net/
- **Hash Support**: ❌ Name-based only
- **Authentication**: Optional API key
- **Rate Limit**: 1 req/s
- **Cost**: FREE
- **Coverage**: Good coverage across systems
- **Metadata**: Title, description, publisher, developer, release date, genres, players
- **Artwork**: Box art (front/back), screenshots, banners
- **API Documentation**: https://api.thegamesdb.net/
- **Best For**: Name-based fallback when hash matching fails
- **Implementation Status**: ✅ Complete

### 7. IGDB (Implemented in M2)
- **URL**: https://www.igdb.com/
- **Hash Support**: ❌ Name-based only
- **Authentication**: Required (Twitch OAuth - client_id, client_secret)
- **Rate Limit**: 4 req/s (250ms interval)
- **Cost**: FREE
- **Coverage**: 200+ systems (most comprehensive)
- **Data Quality**: Excellent - modern database
- **Metadata**: Title, description, publisher, developer, release date, genres, rating, summary, storyline
- **Artwork**: Cover art, screenshots, artworks
- **Special Features**:
  - Richest metadata (related games, franchises, companies)
  - Apicalypse query language
  - Active development
- **API Documentation**: https://api-docs.igdb.com/
- **Best For**: Comprehensive metadata when hash matching unavailable
- **Implementation Status**: ✅ Complete

### 8. MobyGames (Not Implemented)
- **URL**: https://www.mobygames.com/
- **Hash Support**: ❌ Name-based only
- **Authentication**: Required (API key)
- **Rate Limit**: Standard tier
- **Cost**: PAID (API access is paid feature)
- **Coverage**: Comprehensive retro + modern games
- **Metadata**: Title, description, cover art, screenshots
- **Note**: Not recommended due to paid API
- **Implementation Priority**: NONE - Use ScreenScraper instead

### 9. LaunchBox Games Database (Not Implemented)
- **Type**: Downloadable database
- **Hash Support**: ❌ Filename-based matching
- **Authentication**: None for download
- **Rate Limit**: N/A (local database)
- **Cost**: FREE
- **Coverage**: Community-driven
- **Special Features**:
  - Downloads entire database locally
  - Exact filename matching
  - Used by LaunchBox desktop app
- **Best For**: Offline filename matching
- **Implementation Priority**: LOW - Filename matching less reliable than hash

### 10. SteamGridDB (Not Implemented)
- **URL**: https://www.steamgriddb.com/
- **Hash Support**: ❌ Name-based only
- **Authentication**: Required (API key, Steam login)
- **Purpose**: Custom cover art (not primary metadata)
- **Best For**: Alternative artwork after game is identified
- **Implementation Priority**: LOW - UI enhancement only

## Recommended Implementation Strategy

### Phase 1: M2 (Current - Complete)
✅ ScreenScraper (hash + name)
✅ TheGamesDB (name fallback)
✅ IGDB (name fallback with rich metadata)

### Phase 2: M3 Enhancement (Recommended)
🔲 Add Hasheous as hash-based fallback
  - No authentication required
  - Reduces ScreenScraper API load
  - Free alternative for users without ScreenScraper accounts
  
**Provider Fallback Logic**:
1. Try ScreenScraper (hash) - if authenticated
2. Try Hasheous (hash) - if ScreenScraper fails or no auth
3. Try ScreenScraper (name) - if hash fails
4. Try TheGamesDB (name)
5. Try IGDB (name)
6. Manual search

### Phase 3: Future Enhancements (Optional)
🔲 RetroAchievements (achievements + hash verification)
🔲 PlayMatch (community matching)
🔲 OpenVGDB (offline mode)
🔲 SteamGridDB (alternative artwork)

## Provider Comparison Matrix

| Provider | Hash Support | Auth Required | Cost | Coverage | Best For |
|----------|-------------|---------------|------|----------|----------|
| ScreenScraper | ✅ CRC32/MD5/SHA1 | ✅ Yes | Free* | 125+ | Verified dumps |
| Hasheous | ✅ MD5/SHA1 | ❌ No | Free | 135+ | No-auth hash |
| RetroAchievements | ✅ Custom | ✅ Yes | Free | 70+ | Achievements |
| PlayMatch | ✅ Yes | ✅ Yes | Free | IGDB | Gap filling |
| OpenVGDB | ✅ Yes | ❌ No | Free | Many | Offline |
| TheGamesDB | ❌ No | ⚠️ Optional | Free | Good | Name fallback |
| IGDB | ❌ No | ✅ Yes | Free | 200+ | Rich metadata |
| MobyGames | ❌ No | ✅ Yes | Paid | Many | ❌ Not recommended |
| LaunchBox | ❌ Filename | ❌ No | Free | Good | Offline filename |

*ScreenScraper has paid tiers for higher rate limits

## Technical Implementation Notes

### Hasheous Integration Example
```cpp
class HasheousProvider : public MetadataProvider {
public:
    // No authentication needed!
    GameMetadata getByHash(const QString &hash, const QString &system) override {
        // Simple SHA1 or MD5 lookup
        QUrl url("https://hasheous.org/api/v1/lookup");
        QUrlQuery query;
        query.addQueryItem("hash", hash);
        query.addQueryItem("hash_type", detectHashType(hash)); // "sha1" or "md5"
        url.setQuery(query);
        
        // Returns IGDB metadata + RetroAchievements ID
        // No API key in headers!
    }
};
```

### RetroAchievements Integration Example
```cpp
class RetroAchievementsProvider : public MetadataProvider {
    QString apiKey;
    
    GameMetadata getByHash(const QString &hash, const QString &system) override {
        // Custom hash algorithm per system
        QUrl url("https://retroachievements.org/API/API_GetGameInfoByHash.php");
        QUrlQuery query;
        query.addQueryItem("z", apiKey); // username
        query.addQueryItem("y", apiKey); // API key
        query.addQueryItem("m", hash);
        
        // Returns: Title, achievements, hash verification
    }
};
```

## Community Resources

### Hash Database Sources
- **Emulation General Wiki**: https://emulation.gametechwiki.com/index.php/File_hashes
  - Comprehensive list of hash databases
  - BIOS hash lists
  - Verification tool recommendations
  
- **No-Intro DAT-o-MATIC**: https://datomatic.no-intro.org/
  - Download DAT files for verification
  
- **Redump**: http://redump.org/downloads/
  - Disc system DAT files

### ROM Management Inspiration
- **RomM Project**: https://github.com/rommapp/romm
  - v4.0.0 introduced hash-based matching
  - Uses Hasheous + ScreenScraper + IGDB combo
  - Excellent reference implementation
  
- **Article**: https://gardinerbryant.com/romm-4-0-0-overview/
  - Detailed explanation of hash matching benefits
  - Community perspective on metadata provider challenges

## Conclusion

**Immediate Action (M3)**: Add Hasheous as hash-based fallback. It's free, requires no authentication, and provides excellent coverage by proxying IGDB. This will:
- Reduce ScreenScraper API load
- Provide hash matching for users without ScreenScraper accounts
- Maintain high confidence matching throughout fallback chain

**Long-term**: Consider RetroAchievements for achievement integration and OpenVGDB for offline mode.
