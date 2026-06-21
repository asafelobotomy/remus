#pragma once

#include "providers.h"

#include <QMap>
#include <QSet>
#include <QString>

namespace Remus {
namespace Constants {
    namespace ProviderFields {

        // Field name constants used in capability maps and gap computation.
        // These strings map directly to GameMetadata member names for clarity.
        inline constexpr const char *TITLE = "title";
        inline constexpr const char *PUBLISHER = "publisher";
        inline constexpr const char *DEVELOPER = "developer";
        inline constexpr const char *RELEASE_DATE = "releaseDate";
        inline constexpr const char *GENRES = "genres";
        inline constexpr const char *PLAYERS = "players";
        inline constexpr const char *DESCRIPTION = "description";
        inline constexpr const char *BOX_ART_URL = "boxArtUrl";
        inline constexpr const char *RATING = "rating";
        inline constexpr const char *SCREENSHOTS = "screenshotUrls";
        inline constexpr const char *EXTERNAL_IDS = "externalIds";

        // The 8 required fields that define a "fully enriched" game record.
        // Every field in this set must be non-empty/non-zero before a game is
        // considered complete and excluded from further enrichment passes.
        inline const QSet<QString> REQUIRED_FIELDS
            = { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS, DESCRIPTION, BOX_ART_URL };

        // Provider capability map: which GameMetadata fields each provider can supply.
        // LOCAL BAND providers (localdatabase, gametdb) are listed first in the comments
        // to highlight their higher priority — the orchestrator's two-phase waterfall
        // (getSortedLocalProviders / getSortedRemoteProviders) enforces local-first ordering.
        //
        // Key: lowercase provider identifier string (matches Constants::Providers::* values
        //      and the providerId field set by each MetadataProvider implementation).
        //
        // Hasheous bare hash hits only provide title + cross-refs (+ optional logo art).
        // MetadataProxy fields are merged at runtime when a client API key is configured.
        inline const QSet<QString> HASHEOUS_PROXY_FIELDS
            = { PUBLISHER, DEVELOPER, GENRES, RELEASE_DATE, RATING, DESCRIPTION, SCREENSHOTS };

        inline const QSet<QString> HASHEOUS_BARE_FIELDS = { TITLE, EXTERNAL_IDS, BOX_ART_URL };

        inline const QSet<QString> ARTWORK_PROVIDER_IDS = {
            QStringLiteral("compendium"),
            QStringLiteral("localdatabase"),
            QStringLiteral("gametdb"),
            QStringLiteral("screenscraper"),
            QStringLiteral("igdb"),
            QStringLiteral("thegamesdb"),
            QStringLiteral("retroachievements"),
            QStringLiteral("wikidata"),
            QStringLiteral("steamgriddb"),
        };

        inline const QSet<QString> METADATA_EXCLUDED_PROVIDER_IDS = {
            QStringLiteral("steamgriddb"),
        };

        inline bool isArtworkOnlyProvider(const QString &providerId) {
            return providerId.compare(QStringLiteral("steamgriddb"), Qt::CaseInsensitive) == 0;
        }

        inline bool providerSupportsArtworkLookup(const QString &providerId) {
            return ARTWORK_PROVIDER_IDS.contains(providerId.toLower());
        }

        inline bool providerSupportsNameLookup(const QString &providerId) {
            const auto info = Constants::Providers::getProviderInfo(providerId.toLower());
            // Unknown providers (tests/custom stubs) keep legacy name-search behavior.
            return !info || info->supportsNameMatch;
        }

        inline bool providerSupportsSerialLookup(const QString &providerId) {
            return providerId.compare(QStringLiteral("compendium"), Qt::CaseInsensitive) == 0;
        }

        inline const QMap<QString, QSet<QString>> CAPABILITIES = {
            // LOCAL BAND — priority 200 / 150 — always tried before any network call.
            { QStringLiteral("compendium"),
                { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS, BOX_ART_URL, DESCRIPTION } },
            { QStringLiteral("localdatabase"),
                { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS, BOX_ART_URL, DESCRIPTION } },
            { QStringLiteral("gametdb"),
                { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS, BOX_ART_URL, DESCRIPTION } },

            // REMOTE BAND — queried only for fields still missing after local exhaustion.
            { QStringLiteral("screenscraper"),
                { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS, DESCRIPTION, BOX_ART_URL, RATING,
                    SCREENSHOTS, EXTERNAL_IDS } },
            { QStringLiteral("hasheous"), HASHEOUS_BARE_FIELDS },
            { QStringLiteral("playmatch"), { TITLE, RELEASE_DATE, RATING, DESCRIPTION, EXTERNAL_IDS } },
            { QStringLiteral("igdb"),
                { TITLE, PUBLISHER, DEVELOPER, GENRES, RELEASE_DATE, DESCRIPTION, PLAYERS, RATING, SCREENSHOTS,
                    EXTERNAL_IDS } },
            { QStringLiteral("retroachievements"),
                { TITLE, PUBLISHER, DEVELOPER, GENRES, BOX_ART_URL, RATING, EXTERNAL_IDS } },
            { QStringLiteral("thegamesdb"),
                { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, DESCRIPTION, PLAYERS, GENRES, SCREENSHOTS, RATING } },
            { QStringLiteral("wikidata"), { TITLE, PUBLISHER, DEVELOPER, GENRES, RELEASE_DATE, DESCRIPTION } },
        };

        inline bool providerSupportsMetadataLookup(const QString &providerId) {
            const QString key = providerId.toLower();
            if (METADATA_EXCLUDED_PROVIDER_IDS.contains(key))
                return false;
            // Unknown providers (tests/custom stubs) keep legacy metadata waterfall behavior.
            if (!CAPABILITIES.contains(key) && !Constants::Providers::getProviderInfo(key))
                return true;
            return CAPABILITIES.contains(key);
        }

    } // namespace ProviderFields
} // namespace Constants
} // namespace Remus
