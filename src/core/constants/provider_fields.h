#pragma once

#include <QMap>
#include <QSet>
#include <QString>

namespace Remus {
namespace Constants {
namespace ProviderFields {

// Field name constants used in capability maps and gap computation.
// These strings map directly to GameMetadata member names for clarity.
inline constexpr const char* TITLE         = "title";
inline constexpr const char* PUBLISHER     = "publisher";
inline constexpr const char* DEVELOPER     = "developer";
inline constexpr const char* RELEASE_DATE  = "releaseDate";
inline constexpr const char* GENRES        = "genres";
inline constexpr const char* PLAYERS       = "players";
inline constexpr const char* DESCRIPTION   = "description";
inline constexpr const char* BOX_ART_URL   = "boxArtUrl";
inline constexpr const char* RATING        = "rating";
inline constexpr const char* SCREENSHOTS   = "screenshotUrls";

// The 8 required fields that define a "fully enriched" game record.
// Every field in this set must be non-empty/non-zero before a game is
// considered complete and excluded from further enrichment passes.
inline const QSet<QString> REQUIRED_FIELDS = {
    TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE,
    GENRES, PLAYERS, DESCRIPTION, BOX_ART_URL
};

// Provider capability map: which GameMetadata fields each provider can supply.
// LOCAL BAND providers (localdatabase, gametdb) are listed first in the comments
// to highlight their higher priority — the orchestrator's two-phase waterfall
// (getSortedLocalProviders / getSortedRemoteProviders) enforces local-first ordering.
//
// Key: lowercase provider identifier string (matches Constants::Providers::* values
//      and the providerId field set by each MetadataProvider implementation).
inline const QMap<QString, QSet<QString>> CAPABILITIES = {
    // LOCAL BAND — priority 200 / 150 — always tried before any network call.
    { QStringLiteral("compendium"), {
        TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE,
        GENRES, PLAYERS, BOX_ART_URL, DESCRIPTION
    }},
    { QStringLiteral("localdatabase"), {
        TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE,
        GENRES, PLAYERS, BOX_ART_URL, DESCRIPTION
    }},
    { QStringLiteral("gametdb"), {
        TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE,
        GENRES, PLAYERS, BOX_ART_URL, DESCRIPTION
    }},

    // REMOTE BAND — queried only for fields still missing after local exhaustion.
    { QStringLiteral("screenscraper"), {
        TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS,
        DESCRIPTION, BOX_ART_URL, RATING, SCREENSHOTS
    }},
    { QStringLiteral("hasheous"), {
        TITLE, PUBLISHER, DEVELOPER, GENRES, RELEASE_DATE, RATING, DESCRIPTION
    }},
    { QStringLiteral("igdb"), {
        TITLE, PUBLISHER, DEVELOPER, GENRES, RELEASE_DATE,
        DESCRIPTION, PLAYERS, RATING
    }},
    { QStringLiteral("retroachievements"), {
        TITLE, PUBLISHER, DEVELOPER, GENRES, BOX_ART_URL
    }},
    { QStringLiteral("thegamesdb"), {
        TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE,
        DESCRIPTION, PLAYERS, GENRES
    }},
    { QStringLiteral("wikidata"), {
        TITLE, PUBLISHER, DEVELOPER, GENRES, RELEASE_DATE, DESCRIPTION
    }},
};

} // namespace ProviderFields
} // namespace Constants
} // namespace Remus
