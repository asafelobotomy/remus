#pragma once

/**
 * @file api.h
 * @brief API endpoints and service URLs for metadata providers
 * 
 * Centralizes all external API endpoints and service URLs used by
 * metadata providers, avoiding scattered hardcoded URLs throughout the codebase.
 */

namespace Remus {
namespace Constants {
namespace API {

// ============================================================================
// ScreenScraper.fr API
// ============================================================================

/// Base URL for ScreenScraper API
inline constexpr const char* SCREENSCRAPER_BASE_URL = "https://api.screenscraper.fr/api2";

/// ScreenScraper platform endpoint (for system info)
inline constexpr const char* SCREENSCRAPER_PLATFORM_ENDPOINT = "/platform.php";

/// ScreenScraper ROM identification endpoint (hash-based lookup)
inline constexpr const char* SCREENSCRAPER_JEUINFOS_ENDPOINT = "/jeuInfos.php";

/// ScreenScraper game name search endpoint
inline constexpr const char* SCREENSCRAPER_JEURECHERCHE_ENDPOINT = "/jeuRecherche.php";

/// ScreenScraper game details endpoint (by game ID)
inline constexpr const char* SCREENSCRAPER_GETGAME_ENDPOINT = "/jeuInfos.php";

/// ScreenScraper screenshot endpoint
inline constexpr const char* SCREENSCRAPER_SCREENSHOT_ENDPOINT = "/media/screenshots";

/// ScreenScraper wheel art endpoint
inline constexpr const char* SCREENSCRAPER_WHEEL_ENDPOINT = "/media/wheels";

/// ScreenScraper marquee endpoint
inline constexpr const char* SCREENSCRAPER_MARQUEE_ENDPOINT = "/media/marquees";

/// ScreenScraper video endpoint
inline constexpr const char* SCREENSCRAPER_VIDEO_ENDPOINT = "/media/videos";

// ============================================================================
// TheGamesDB API
// ============================================================================

/// Base URL for TheGamesDB API v1
inline constexpr const char* THEGAMESDB_BASE_URL = "https://api.thegamesdb.net/v1";

/// TheGamesDB games endpoint (search)
inline constexpr const char* THEGAMESDB_GAMES_ENDPOINT = "/Games/ByGameName";

/// TheGamesDB platforms endpoint (system info)
inline constexpr const char* THEGAMESDB_PLATFORMS_ENDPOINT = "/Platforms";

/// TheGamesDB game details endpoint
inline constexpr const char* THEGAMESDB_GAMEINFO_ENDPOINT = "/Games";

/// TheGamesDB image endpoint (artwork base)
inline constexpr const char* THEGAMESDB_IMAGES_BASE = "https://cdn.thegamesdb.net/images";

// ============================================================================
// IGDB API (Internet Game Database)
// ============================================================================

/// Base URL for IGDB API v4
inline constexpr const char* IGDB_BASE_URL = "https://api.igdb.com/v4";

/// IGDB games endpoint (search/query)
inline constexpr const char* IGDB_GAMES_ENDPOINT = "/games";

/// IGDB platforms endpoint (system info)
inline constexpr const char* IGDB_PLATFORMS_ENDPOINT = "/platforms";

/// IGDB covers endpoint (artwork)
inline constexpr const char* IGDB_COVERS_ENDPOINT = "/covers";

/// IGDB screenshots endpoint
inline constexpr const char* IGDB_SCREENSHOTS_ENDPOINT = "/screenshots";

/// IGDB image CDN base URL
inline constexpr const char* IGDB_CDN_BASE = "https://images.igdb.com/igdb/image/upload";

// ============================================================================
// Hasheous API (Free hash matching)
// ============================================================================

/// Base URL for Hasheous service
inline constexpr const char* HASHEOUS_BASE_URL = "https://hasheous.org";

/// Hasheous hash lookup endpoint (POST with JSON body)
inline constexpr const char* HASHEOUS_LOOKUP_ENDPOINT = "/api/v1/Lookup/ByHash";

/// Hasheous IGDB metadata proxy endpoint (GET with game ID)
inline constexpr const char* HASHEOUS_PROXY_IGDB_GAME = "/api/v1/MetadataProxy/IGDB/Game";

/// Hasheous TheGamesDB metadata proxy endpoint
inline constexpr const char* HASHEOUS_PROXY_TGDB_GAME = "/api/v1/MetadataProxy/TheGamesDB/Game";

/// Hasheous health check endpoint
inline constexpr const char* HASHEOUS_HEALTHCHECK_ENDPOINT = "/api/v1/HealthCheck";

// ============================================================================
// Known ROM Database Services
// ============================================================================

/// URL pattern for downloading DAT files from No-Intro
inline constexpr const char* NO_INTRO_DAT_BASE = "https://datomatic.no-intro.org";

/// URL pattern for downloading DAT files from Redump
inline constexpr const char* REDUMP_DAT_BASE = "https://redump.org/datfiles";

// ============================================================================
// API Query Parameters
// ============================================================================

/// HTTP User-Agent string for API requests
inline const QString USER_AGENT = QStringLiteral("Remus/0.10.1 (ROM Library Manager)");

/// IGDB API Client User-Agent header name
inline constexpr const char* IGDB_CLIENT_ID_HEADER = "Client-ID";

/// Maximum number of items per API request (pagination limit)
inline constexpr int MAX_RESULTS_PER_PAGE = 50;

/// Default number of results per API request
inline constexpr int DEFAULT_RESULTS_PER_PAGE = 20;

}  // namespace API
}  // namespace Constants
}  // namespace Remus
