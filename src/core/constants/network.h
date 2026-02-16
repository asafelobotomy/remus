#pragma once

/**
 * @file network.h
 * @brief Network configuration, timeouts, and rate limiting constants
 * 
 * Centralizes all network-related configuration including HTTP timeouts,
 * rate limits, retry policies, and connection parameters.
 */

namespace Remus {
namespace Constants {
namespace Network {

// ============================================================================
// HTTP Timeouts (milliseconds)
// ============================================================================

/// Default HTTP request timeout (5 seconds)
inline constexpr int HTTP_TIMEOUT_MS = 5000;

/// Metadata API request timeout (10 seconds, longer for complex queries)
inline constexpr int METADATA_TIMEOUT_MS = 10000;

/// Artwork download timeout (60 seconds, for large files)
inline constexpr int ARTWORK_TIMEOUT_MS = 60000;

/// ScreenScraper API timeout (20 seconds, often slower)
inline constexpr int SCREENSCRAPER_TIMEOUT_MS = 20000;

/// IGDB API timeout (15 seconds)
inline constexpr int IGDB_TIMEOUT_MS = 15000;

/// TheGamesDB timeout (10 seconds)
inline constexpr int THEGAMESDB_TIMEOUT_MS = 10000;

/// Hasheous API timeout (5 seconds, fast service)
inline constexpr int HASHEOUS_TIMEOUT_MS = 5000;

/// Connection establishment timeout (3 seconds)
inline constexpr int CONNECT_TIMEOUT_MS = 3000;

// ============================================================================
// Rate Limiting
// ============================================================================

/// ScreenScraper minimum interval between requests (milliseconds)
/// Documentation recommends 200ms minimum
inline constexpr int SCREENSCRAPER_RATE_LIMIT_MS = 250;

/// IGDB minimum interval between requests (milliseconds)
/// API allows 4 requests/second = 250ms minimum
inline constexpr int IGDB_RATE_LIMIT_MS = 250;

/// TheGamesDB minimum interval between requests (milliseconds)
inline constexpr int THEGAMESDB_RATE_LIMIT_MS = 100;

/// Hasheous minimum interval between requests (milliseconds)
/// Fast service, low rate limit needed
inline constexpr int HASHEOUS_RATE_LIMIT_MS = 50;

/// Default rate limit for generic requests (milliseconds)
inline constexpr int DEFAULT_RATE_LIMIT_MS = 1000;

// ============================================================================
// Retry Policy
// ============================================================================

/// Maximum number of automatic retries for failed requests
inline constexpr int MAX_RETRIES = 3;

/// Backoff multiplier for retry attempts (exponential)
/// Next retry = delay * (backoff_multiplier ^ retry_count)
inline constexpr float RETRY_BACKOFF_MULTIPLIER = 1.5f;

/// Initial retry delay (milliseconds)
inline constexpr int INITIAL_RETRY_DELAY_MS = 500;

/// Maximum retry delay before giving up (milliseconds)
inline constexpr int MAX_RETRY_DELAY_MS = 10000;

// ============================================================================
// Connection Parameters
// ============================================================================

/// HTTP Keep-Alive timeout (seconds)
inline constexpr int KEEP_ALIVE_TIMEOUT_S = 15;

/// Maximum concurrent API requests
inline constexpr int MAX_CONCURRENT_REQUESTS = 5;

/// Thread pool size for network operations
inline constexpr int NETWORK_THREAD_POOL_SIZE = 4;

// ============================================================================
// Cache Control
// ============================================================================

/// Metadata cache expiration (24 hours in seconds)
inline constexpr int METADATA_CACHE_TTL_S = 86400;

/// Cache check interval (5 minutes in seconds)
inline constexpr int CACHE_CHECK_INTERVAL_S = 300;

/// Artwork cache expiration (7 days in seconds)
inline constexpr int ARTWORK_CACHE_TTL_S = 604800;

// ============================================================================
// Buffer Sizes
// ============================================================================

/// Default network buffer size (bytes)
inline constexpr int DEFAULT_BUFFER_SIZE = 8192;

/// Large file buffer size for artwork downloads (bytes)
inline constexpr int LARGE_FILE_BUFFER_SIZE = 65536;

/// API response buffer size (bytes)
inline constexpr int API_RESPONSE_BUFFER_SIZE = 1048576;  // 1MB

// ============================================================================
// Network Error Handling
// ============================================================================

/// HTTP status code: Unauthorized (retry with new credentials)
inline constexpr int HTTP_UNAUTHORIZED = 401;

/// HTTP status code: Forbidden (don't retry, skip provider)
inline constexpr int HTTP_FORBIDDEN = 403;

/// HTTP status code: Not Found (don't retry)
inline constexpr int HTTP_NOT_FOUND = 404;

/// HTTP status code: Server Error (retry with backoff)
inline constexpr int HTTP_SERVER_ERROR = 500;

/// HTTP status code: Service Unavailable (retry with backoff)
inline constexpr int HTTP_SERVICE_UNAVAILABLE = 503;

}  // namespace Network
}  // namespace Constants
}  // namespace Remus
