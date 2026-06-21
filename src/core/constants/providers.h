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

        // Provider priority values (higher = tried first).
        // Ordered by the number of GameMetadata fields each provider populates on a
        // successful match ("field-coverage score", out of 10 core fields).
        //
        // LOCAL BAND (200-100): offline providers — always exhausted before any
        //   network call is made (see two-phase waterfall in ProviderOrchestrator).
        //
        //   localdatabase 200 — 9/10: title pub dev genres date desc players art shots; +hashes+serial
        //   gametdb       150 — 8/10: title pub dev genres date desc players art; Nintendo/PS3 offline XML
        //
        // REMOTE BAND (90-40): online providers — queried only for fields still
        //   missing after all local providers have been exhausted.
        //
        //   hasheous       91 —  1/10 bare / 9/10 with MetadataProxy; multi-hash POST before SS
        //   screenscraper  89 — 10/10: all fields including rating & screenshots; requires auth
        //   playmatch      88 —  6/10: title/desc/date/rating via IGDB proxy; hash + filename+size
        //   igdb           70 —  8/10: title pub dev genres date desc players rating; requires OAuth
        //   retroachievements 60 — 6/10: title sys pub dev genres date art; MD5 hash-only, free key
        //   thegamesdb     50 —  6/10: title sys pub dev date desc players; simplified genres, no auth
        //   wikidata       40 —  6/10: title sys pub dev genres date desc; community-sourced, no auth
        namespace Priority {
            inline constexpr int COMPENDIUM = 210;
            inline constexpr int LOCAL_DATABASE = 200;
            inline constexpr int GAMETDB = 150;
            inline constexpr int HASHEOUS = 91;
            inline constexpr int SCREENSCRAPER = 89;
            inline constexpr int PLAYMATCH = 88;
            inline constexpr int IGDB = 70;
            inline constexpr int STEAMGRIDDB = 75;
            inline constexpr int RETROACHIEVEMENTS = 60;
            inline constexpr int THEGAMESDB = 50;
            inline constexpr int WIKIDATA = 40;
        }

        /// Metadata provider: Hasheous (free, hash-only)
        inline constexpr const char *HASHEOUS = "hasheous";

        /// Metadata provider: PlayMatch (free public API, hash/filename matching)
        inline constexpr const char *PLAYMATCH = "playmatch";

        /// Metadata provider: ScreenScraper (requires auth)
        inline constexpr const char *SCREENSCRAPER = "screenscraper";

        /// Metadata provider: TheGamesDB (free)
        inline constexpr const char *THEGAMESDB = "thegamesdb";

        /// Metadata provider: IGDB (requires API key)
        inline constexpr const char *IGDB = "igdb";

        /// Metadata provider: SteamGridDB (artwork only, requires API key)
        inline constexpr const char *STEAMGRIDDB = "steamgriddb";

        /// Metadata provider: Local DAT database (offline, no auth)
        inline constexpr const char *LOCAL_DATABASE = "localdatabase";

        /// Metadata provider: Canonical compendium database (offline, no auth)
        inline constexpr const char *COMPENDIUM = "compendium";

        /// Metadata provider: GameTDB (offline, no auth, Nintendo/PS3)
        inline constexpr const char *GAMETDB = "gametdb";

        /// Metadata provider: RetroAchievements (hash-based, requires free API key)
        inline constexpr const char *RETROACHIEVEMENTS = "retroachievements";

        /// Metadata provider: Wikidata (SPARQL, no auth, CC0)
        inline constexpr const char *WIKIDATA = "wikidata";

        // ============================================================================
        // Provider Display Names (User-facing)
        // ============================================================================

        /// Human-readable name for Hasheous
        inline const QString DISPLAY_HASHEOUS = QStringLiteral("Hasheous");

        /// Human-readable name for PlayMatch
        inline const QString DISPLAY_PLAYMATCH = QStringLiteral("PlayMatch");

        /// Human-readable name for ScreenScraper
        inline const QString DISPLAY_SCREENSCRAPER = QStringLiteral("ScreenScraper");

        /// Human-readable name for TheGamesDB
        inline const QString DISPLAY_THEGAMESDB = QStringLiteral("TheGamesDB");

        /// Human-readable name for IGDB
        inline const QString DISPLAY_IGDB = QStringLiteral("IGDB");

        /// Human-readable name for SteamGridDB
        inline const QString DISPLAY_STEAMGRIDDB = QStringLiteral("SteamGridDB");

        /// Human-readable name for Local Database
        inline const QString DISPLAY_LOCAL_DATABASE = QStringLiteral("Local Database");

        /// Human-readable name for Compendium
        inline const QString DISPLAY_COMPENDIUM = QStringLiteral("Compendium");

        /// Human-readable name for GameTDB
        inline const QString DISPLAY_GAMETDB = QStringLiteral("GameTDB");

        /// Human-readable name for RetroAchievements
        inline const QString DISPLAY_RETROACHIEVEMENTS = QStringLiteral("RetroAchievements");

        /// Human-readable name for Wikidata
        inline const QString DISPLAY_WIKIDATA = QStringLiteral("Wikidata");

        // ============================================================================
        // External ID keys (used as keys in GameMetadata::externalIds QMap)
        // ============================================================================

        namespace ExternalId {
            /// Key for IGDB game IDs in the externalIds map
            inline constexpr const char *IGDB = "igdb";

            /// Key for TheGamesDB game IDs in the externalIds map
            inline constexpr const char *THEGAMESDB = "thegamesdb";

            /// Key for RetroAchievements game IDs in the externalIds map
            inline constexpr const char *RETROACHIEVEMENTS = "retroachievements";

            /// Key for DAT-file source list (comma-joined names) in the externalIds map
            inline constexpr const char *DAT_SOURCES = "dat_sources";

            /// Key for GameTDB game IDs in the externalIds map
            inline constexpr const char *GAMETDB = "gametdb";

            /// Key for Wikidata entity IDs in the externalIds map
            inline constexpr const char *WIKIDATA = "wikidata";

            /// Key for Steam app IDs in the externalIds map
            inline constexpr const char *STEAM = "steam";

            /// Key for SteamGridDB game IDs in the externalIds map
            inline constexpr const char *STEAMGRIDDB = "steamgriddb";
        } // ExternalId

        // ============================================================================
        // Provider Metadata
        // ============================================================================

        /**
         * @brief Information about a metadata provider
         */
        struct ProviderInfo {
            QString id; ///< Internal identifier (SCREENSCRAPER, IGDB, etc.)
            QString displayName; ///< User-facing display name
            QString description; ///< Long description for UI tooltips
            bool supportsHashMatch; ///< Can search by file hash
            bool supportsNameMatch; ///< Can search by game name
            bool requiresAuth; ///< Requires credentials
            QString authHelpUrl; ///< URL for obtaining credentials
            int priority; ///< Fallback priority (higher = tried first)
            bool isFreeService; ///< Does not require payment
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
            // Priority 200: Local DAT database (offline, instant) — 9/10 fields
            { LOCAL_DATABASE,
                {
                    LOCAL_DATABASE, DISPLAY_LOCAL_DATABASE,
                    QStringLiteral("Offline ROM identification via local DAT files (no auth required)"),
                    true, // Hash matching
                    true, // Name search (substring)
                    false, // No auth required
                    QStringLiteral(""), Priority::LOCAL_DATABASE,
                    true // Free service
                } },

            // Priority 180: Canonical compendium database (offline, precompiled) — 8/10 fields
            { COMPENDIUM,
                {
                    COMPENDIUM, DISPLAY_COMPENDIUM,
                    QStringLiteral("Offline metadata from the canonical Remus compendium (no auth required)"),
                    true, // Hash matching
                    true, // Name search
                    false, // No auth required
                    QStringLiteral(""), Priority::COMPENDIUM,
                    true // Free service
                } },

            // Priority 80: Hash identification + cross-references; 1/10 bare, 9/10 with MetadataProxy
            { HASHEOUS,
                {
                    HASHEOUS, DISPLAY_HASHEOUS, QStringLiteral("Free hash database (no auth required)"),
                    true, // Hash matching
                    false, // No name search (hash only)
                    false, // No auth required
                    QStringLiteral(""), Priority::HASHEOUS,
                    true // Free service
                } },

            // Priority 88: Hash/filename bridge with IGDB proxy — 6/10 fields; no auth on public instance
            { PLAYMATCH,
                {
                    PLAYMATCH, DISPLAY_PLAYMATCH,
                    QStringLiteral("Hash and filename matching with IGDB metadata via PlayMatch (no auth required)"),
                    true, // Hash matching (via identify API)
                    false, // No standalone name search
                    false, // No auth required on public instance
                    QStringLiteral("https://playmatch.retrorealm.dev"), Priority::PLAYMATCH,
                    true // Free service
                } },

            // Priority 90: Most complete remote provider — 10/10 fields; requires auth
            { SCREENSCRAPER,
                {
                    SCREENSCRAPER, DISPLAY_SCREENSCRAPER,
                    QStringLiteral("Comprehensive ROM metadata with artwork (requires free account)"),
                    true, // Hash matching
                    true, // Name search
                    true, // Requires authentication
                    QStringLiteral("https://www.screenscraper.fr"), Priority::SCREENSCRAPER,
                    true // Free service available
                } },

            // Priority 150: Second local provider — 8/10 fields; Nintendo/PS3 offline XML
            { GAMETDB,
                {
                    GAMETDB, DISPLAY_GAMETDB,
                    QStringLiteral("Nintendo and PS3 metadata from GameTDB XML databases (no auth required)"),
                    true, // Hash matching (via ROM CRC/MD5/SHA1)
                    true, // Name search
                    false, // No auth required
                    QStringLiteral("https://www.gametdb.com"), Priority::GAMETDB,
                    true // Free service
                } },

            // Priority 50: Decent remote fallback — 6/10 fields, simplified genres, no auth
            { THEGAMESDB,
                {
                    THEGAMESDB, DISPLAY_THEGAMESDB, QStringLiteral("Game metadata and artwork (no auth required)"),
                    false, // No hash matching
                    true, // Name search
                    false, // No auth required
                    QStringLiteral("https://thegamesdb.net"), Priority::THEGAMESDB,
                    true // Free service
                } },

            // Priority 60: Hash-based identification — 6/10 fields; MD5-only, free API key
            { RETROACHIEVEMENTS,
                {
                    RETROACHIEVEMENTS, DISPLAY_RETROACHIEVEMENTS,
                    QStringLiteral("Hash-based game identification via RetroAchievements (requires free API key)"),
                    true, // Hash matching (MD5)
                    false, // No name search
                    true, // Requires API key
                    QStringLiteral("https://retroachievements.org/controlpanel.php"), Priority::RETROACHIEVEMENTS,
                    true // Free service
                } },

            // Priority 70: Rich metadata — 8/10 fields; requires Twitch OAuth, name-only
            { IGDB,
                {
                    IGDB, DISPLAY_IGDB, QStringLiteral("Commercial game database (requires API key)"),
                    false, // No hash matching
                    true, // Name search only
                    true, // Requires API key
                    QStringLiteral("https://api.igdb.com"), Priority::IGDB,
                    false // Requires subscription
                } },

            // Priority 75: Artwork-only fallback — grids/heroes/logos; requires free API key
            { STEAMGRIDDB,
                {
                    STEAMGRIDDB, DISPLAY_STEAMGRIDDB,
                    QStringLiteral("Community game artwork (grids, heroes, logos; requires free API key)"),
                    false, // No hash matching
                    false, // No text metadata search
                    true, // Requires API key
                    QStringLiteral("https://www.steamgriddb.com"), Priority::STEAMGRIDDB,
                    true // Free service
                } },

            // Priority 40: Community-sourced supplementary data — 6/10 fields, no auth
            { WIKIDATA,
                {
                    WIKIDATA, DISPLAY_WIKIDATA,
                    QStringLiteral("Wikidata SPARQL endpoint for game metadata (no auth, CC0 licensed)"),
                    false, // No hash matching
                    true, // Name search
                    false, // No auth required
                    QStringLiteral(""), Priority::WIKIDATA,
                    true // Free service
                } },
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
        inline const ProviderInfo *getProviderInfo(const QString &providerId) {
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
            std::sort(providers.begin(), providers.end(), [](const QString &a, const QString &b) {
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
