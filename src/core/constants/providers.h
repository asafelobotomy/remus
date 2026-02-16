#pragma once

#include <QString>
#include <QMap>
#include <QList>
#include <algorithm>
#include "settings.h"

namespace Remus {
namespace Constants {
namespace Providers {

// ============================================================================
// Provider Identifiers (Internal use)
// ============================================================================

/// Metadata provider: Hasheous (free, hash-only)
inline constexpr const char* HASHEOUS = "hasheous";

/// Metadata provider: ScreenScraper (requires auth)
inline constexpr const char* SCREENSCRAPER = "screenscraper";

/// Metadata provider: TheGamesDB (free)
inline constexpr const char* THEGAMESDB = "thegamesdb";

/// Metadata provider: IGDB (requires API key)
inline constexpr const char* IGDB = "igdb";

// ============================================================================
// Provider Display Names (User-facing)
// ============================================================================

/// Human-readable name for Hasheous
inline const QString DISPLAY_HASHEOUS = QStringLiteral("Hasheous");

/// Human-readable name for ScreenScraper
inline const QString DISPLAY_SCREENSCRAPER = QStringLiteral("ScreenScraper");

/// Human-readable name for TheGamesDB
inline const QString DISPLAY_THEGAMESDB = QStringLiteral("TheGamesDB");

/// Human-readable name for IGDB
inline const QString DISPLAY_IGDB = QStringLiteral("IGDB");

// ============================================================================
// Provider Metadata
// ============================================================================

/**
 * @brief Information about a metadata provider
 */
struct ProviderInfo {
    QString id;              ///< Internal identifier (SCREENSCRAPER, IGDB, etc.)
    QString displayName;     ///< User-facing display name
    QString description;     ///< Long description for UI tooltips
    bool supportsHashMatch;  ///< Can search by file hash
    bool supportsNameMatch;  ///< Can search by game name
    bool requiresAuth;       ///< Requires credentials
    QString authHelpUrl;     ///< URL for obtaining credentials
    int priority;            ///< Fallback priority (higher = tried first)
    bool isFreeService;      ///< Does not require payment
};

/**
 * @brief Registry of all available metadata providers
 * 
 * Ordered by priority for fallback chain:
 * 1. Hash matches always preferred (100% accuracy)
 * 2. Name matches with fallback chain
 * 3. Fuzzy matches as last resort
 */
inline const QMap<QString, ProviderInfo> PROVIDER_REGISTRY = {
    // Priority 100: Hash-first provider (best for hash-based matching)
    {HASHEOUS, {
        HASHEOUS,
        DISPLAY_HASHEOUS,
        QStringLiteral("Free hash database (no auth required)"),
        true,   // Hash matching
        false,  // No name search (hash only)
        false,  // No auth required
        QStringLiteral(""),
        100,    // Highest priority (hash matching preferred)
        true    // Free service
    }},
    
    // Priority 90: Primary authenticated provider (comprehensive database)
    {SCREENSCRAPER, {
        SCREENSCRAPER,
        DISPLAY_SCREENSCRAPER,
        QStringLiteral("Comprehensive ROM metadata with artwork (requires free account)"),
        true,   // Hash matching
        true,   // Name search
        true,   // Requires authentication
        QStringLiteral("https://www.screenscraper.fr"),
        90,     // Fallback after hash-only
        true    // Free service available
    }},
    
    // Priority 50: Fallback provider
    {THEGAMESDB, {
        THEGAMESDB,
        DISPLAY_THEGAMESDB,
        QStringLiteral("Game metadata and artwork (no auth required)"),
        false,  // No hash matching
        true,   // Name search
        false,  // No auth required
        QStringLiteral("https://thegamesdb.net"),
        50,     // Medium fallback
        true    // Free service
    }},
    
    // Priority 40: Commercial provider
    {IGDB, {
        IGDB,
        DISPLAY_IGDB,
        QStringLiteral("Commercial game database (requires API key)"),
        false,  // No hash matching
        true,   // Name search only
        true,   // Requires API key
        QStringLiteral("https://api.igdb.com"),
        40,     // Last fallback
        false   // Requires subscription
    }},
};

// ============================================================================
// Provider Settings Keys (aliases)
// ============================================================================

namespace SettingsKeys = Settings::Providers;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get provider information by ID
 * @param providerId Provider identifier (SCREENSCRAPER, IGDB, etc.)
 * @return Pointer to ProviderInfo, or nullptr if not found
 */
inline const ProviderInfo* getProviderInfo(const QString &providerId) {
    auto it = PROVIDER_REGISTRY.find(providerId);
    if (it != PROVIDER_REGISTRY.end()) {
        return &it.value();
    }
    return nullptr;
}

/**
 * @brief Get all metadata providers sorted by priority
 * @return List of provider IDs in priority order (highest first)
 */
inline QStringList getProvidersByPriority() {
    QStringList providers;
    for (auto it = PROVIDER_REGISTRY.begin(); it != PROVIDER_REGISTRY.end(); ++it) {
        providers << it.key();
    }
    // Sort by priority (descending)
    std::sort(providers.begin(), providers.end(),
        [](const QString &a, const QString &b) {
            return PROVIDER_REGISTRY[a].priority > PROVIDER_REGISTRY[b].priority;
        });
    return providers;
}

/**
 * @brief Get provider's display name
 * @param providerId Internal provider ID
 * @return User-facing display name
 */
inline QString getProviderDisplayName(const QString &providerId) {
    auto info = getProviderInfo(providerId);
    return info ? info->displayName : QStringLiteral("Unknown");
}

/**
 * @brief Get all providers that support hash matching
 * @return List of provider IDs that can match by hash
 */
inline QStringList getHashSupportingProviders() {
    QStringList providers;
    for (auto it = PROVIDER_REGISTRY.begin(); it != PROVIDER_REGISTRY.end(); ++it) {
        if (it.value().supportsHashMatch) {
            providers << it.key();
        }
    }
    return providers;
}

/**
 * @brief Get all providers that support name matching
 * @return List of provider IDs that can search by name
 */
inline QStringList getNameSupportingProviders() {
    QStringList providers;
    for (auto it = PROVIDER_REGISTRY.begin(); it != PROVIDER_REGISTRY.end(); ++it) {
        if (it.value().supportsNameMatch) {
            providers << it.key();
        }
    }
    return providers;
}

} // Providers
} // Constants
} // Remus
