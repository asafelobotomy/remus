#include "cli_commands.h"
#include "cli_helpers.h"
#include <memory>
#include <QSettings>
#include "../metadata/metadata_provider.h"
#include "../metadata/provider_orchestrator.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/local_database_provider.h"
#include "../metadata/gametdb_provider.h"
#include "../metadata/retroachievements_provider.h"
#include "../metadata/wikidata_provider.h"
#include "../core/constants/constants.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

namespace {

QSettings remusSettings()
{
    return QSettings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                     QString::fromLatin1(Constants::SETTINGS_APPLICATION));
}

QString parserOrSetting(const QCommandLineParser &parser,
                        const QString &optionName,
                        const char *settingKey)
{
    if (parser.isSet(optionName)) {
        return parser.value(optionName).trimmed();
    }

    QSettings settings = remusSettings();
    return settings.value(QString::fromLatin1(settingKey)).toString().trimmed();
}

}

/// Build a single provider from parser credentials.
/// Returns nullptr when providerName is "auto" or unrecognised.
static std::unique_ptr<MetadataProvider> buildSingleProvider(const QCommandLineParser &parser)
{
    const QString providerName = parser.value("provider");

    if (providerName == Providers::SCREENSCRAPER) {
        auto p = std::make_unique<ScreenScraperProvider>();
        if (parser.isSet("ss-user") && parser.isSet("ss-pass"))
            p->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
        if (parser.isSet("ss-devid") && parser.isSet("ss-devpass"))
            p->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
        return p;
    }
    if (providerName == Providers::THEGAMESDB) {
        auto p = std::make_unique<TheGamesDBProvider>();
        const QString tgdbApiKey = parserOrSetting(parser,
                                                   QStringLiteral("tgdb-api-key"),
                                                   Settings::Providers::THEGAMESDB_API_KEY);
        if (!tgdbApiKey.isEmpty()) {
            p->setApiKey(tgdbApiKey);
        }
        return p;
    }
    if (providerName == Providers::IGDB) {
        auto p = std::make_unique<IGDBProvider>();
        const QString clientId = parserOrSetting(parser,
                                                 QStringLiteral("igdb-client-id"),
                                                 Settings::Providers::IGDB_CLIENT_ID);
        const QString clientSecret = parserOrSetting(parser,
                                                     QStringLiteral("igdb-client-secret"),
                                                     Settings::Providers::IGDB_CLIENT_SECRET);
        if (!clientId.isEmpty() && !clientSecret.isEmpty()) {
            p->setCredentials(clientId, clientSecret);
        }
        return p;
    }
    if (providerName == Providers::HASHEOUS) {
        return std::make_unique<HasheousProvider>();
    }
    if (providerName == Providers::LOCAL_DATABASE) {
        auto p = std::make_unique<LocalDatabaseProvider>();
        const QString dbDir = findDatabaseDir();
        if (!dbDir.isEmpty()) {
            p->loadDatabases(dbDir);
            const QString metaDir = findMetadataDir();
            if (!metaDir.isEmpty())
                p->loadMetadata(metaDir);
        }
        return p;
    }
    if (providerName == Providers::GAMETDB) {
        auto p = std::make_unique<GameTDBProvider>();
        const QString gametdbDir = findGameTDBDir();
        if (!gametdbDir.isEmpty()) {
            p->loadDatabases(gametdbDir);
        }
        return p;
    }
    if (providerName == Providers::RETROACHIEVEMENTS) {
        auto p = std::make_unique<RetroAchievementsProvider>();
        const QString raUser = parserOrSetting(parser,
                                               QStringLiteral("ra-user"),
                                               Settings::Providers::RETROACHIEVEMENTS_USERNAME);
        const QString raKey = parserOrSetting(parser,
                                              QStringLiteral("ra-api-key"),
                                              Settings::Providers::RETROACHIEVEMENTS_API_KEY);
        if (!raUser.isEmpty() && !raKey.isEmpty())
            p->setCredentials(raUser, raKey);
        return p;
    }
    if (providerName == Providers::WIKIDATA) {
        return std::make_unique<WikidataProvider>();
    }
    return nullptr;
}

int handleMetadataCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("metadata")) return 0;

    const QString hash       = ctx.parser.value("metadata");
    const QString system     = ctx.parser.value("system");
    const QString provName   = ctx.parser.value("provider");

    qInfo() << "";
    qInfo() << "Fetching metadata for hash:" << hash;
    qInfo() << "System:" << (system.isEmpty() ? "auto-detect" : system);
    qInfo() << "Provider:" << provName;
    qInfo() << "";

    auto provider = buildSingleProvider(ctx.parser);
    if (!provider) {
        // No explicit provider — use the orchestrator for automatic fallback
        auto orchestrator = buildOrchestrator(ctx.parser, &ctx.db);
        GameMetadata metadata = orchestrator->searchWithFallback(hash, QString(), system);
        if (!metadata.title.isEmpty()) {
            qInfo() << "✓ Match found via" << metadata.providerId << "!";
        } else {
            qInfo() << "✗ No match found for hash:" << hash;
            return 0;
        }
        qInfo() << "─────────────────────────────────────";
        qInfo() << "Title:"        << metadata.title;
        qInfo() << "System:"       << metadata.system;
        qInfo() << "Region:"       << metadata.region;
        qInfo() << "Developer:"    << metadata.developer;
        qInfo() << "Publisher:"    << metadata.publisher;
        qInfo() << "Release Date:" << metadata.releaseDate;
        qInfo() << "Genres:"       << metadata.genres.join(", ");
        return 0;
    }

    GameMetadata metadata = provider->getByHash(hash, system);
    if (!metadata.title.isEmpty()) {
        qInfo() << "✓ Match found!";
        qInfo() << "─────────────────────────────────────";
        qInfo() << "Title:"        << metadata.title;
        qInfo() << "System:"       << metadata.system;
        qInfo() << "Region:"       << metadata.region;
        qInfo() << "Developer:"    << metadata.developer;
        qInfo() << "Publisher:"    << metadata.publisher;
        qInfo() << "Release Date:" << metadata.releaseDate;
        qInfo() << "Genres:"       << metadata.genres.join(", ");
        qInfo() << "Players:"      << metadata.players;
        qInfo() << "Rating:"       << metadata.rating << "/ 10";
        qInfo() << "";
        if (!metadata.boxArtUrl.isEmpty()) {
            qInfo() << "Box Art:"   << metadata.boxArtUrl;
        }
        for (const QString &url : metadata.screenshotUrls) {
            if (url.contains(QStringLiteral("Named_Snaps"))) {
                qInfo() << "Screenshot:" << url;
            } else if (url.contains(QStringLiteral("Named_Titles"))) {
                qInfo() << "Title Screen:" << url;
            } else if (url.contains(QStringLiteral("Named_Boxarts"))) {
                qInfo() << "Box Art (alt):" << url;
            } else {
                qInfo() << "Artwork:" << url;
            }
        }
        qInfo() << "";
        qInfo() << "Description:";
        qInfo().noquote() << metadata.description;
    } else {
        qInfo() << "✗ No match found for hash:" << hash;
    }
    return 0;
}

int handleSearchCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("search")) return 0;

    const QString title    = ctx.parser.value("search");
    const QString system   = ctx.parser.value("system");
    const QString provName = ctx.parser.value("provider");

    qInfo() << "";
    qInfo() << "Searching for:" << title;
    qInfo() << "System:"        << (system.isEmpty() ? "any" : system);
    qInfo() << "Provider:"      << provName;
    qInfo() << "";

    auto provider = buildSingleProvider(ctx.parser);
    if (!provider) {
        // No explicit provider — use the orchestrator for search-by-name
        auto orchestrator = buildOrchestrator(ctx.parser, &ctx.db);
        GameMetadata metadata = orchestrator->searchWithFallback(QString(), title, system);
        if (!metadata.title.isEmpty()) {
            qInfo() << "✓ Found via" << metadata.providerId << ":" << metadata.title;
            qInfo() << "  System:" << metadata.system;
            qInfo() << "  Developer:" << metadata.developer;
            qInfo() << "  Publisher:" << metadata.publisher;
        } else {
            qInfo() << "No results found for:" << title;
        }
        return 0;
    }

    QList<SearchResult> results = provider->searchByName(title, system);
    if (results.isEmpty()) {
        qInfo() << "No results found for:" << title;
        const QString provName = ctx.parser.value("provider");
        if (provName == Providers::THEGAMESDB) {
            const QString key = parserOrSetting(ctx.parser,
                                                QStringLiteral("tgdb-api-key"),
                                                Settings::Providers::THEGAMESDB_API_KEY);
            if (key.isEmpty())
                qInfo() << "Hint: No API key configured for TheGamesDB. Set with --tgdb-api-key or in settings.";
        }
        return 0;
    }

    qInfo() << "Found" << results.size() << "result(s):";
    qInfo() << "─────────────────────────────────────";
    for (int i = 0; i < results.size(); ++i) {
        const SearchResult &r = results[i];
        qInfo().noquote() << QString("%1. %2 (%3)").arg(i + 1).arg(r.title).arg(r.releaseYear);
        qInfo() << "   System:" << r.system;
        qInfo() << "   Match Score:" << QString::number(r.matchScore * 100, 'f', 0) + "%";
        qInfo() << "   Provider ID:" << r.id;
        qInfo() << "";
    }
    return 0;
}

int handleEnrichCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("enrich")) return 0;

    qInfo() << "";
    qInfo() << "=== Metadata Enrichment ===";
    qInfo() << "";

    auto orchestrator = buildOrchestrator(ctx.parser, &ctx.db);
    QMap<int, Database::MatchResult> matches = ctx.db.getAllMatches();
    QList<FileRecord> files = ctx.db.getExistingFiles();

    // Build fileId→FileRecord lookup
    QMap<int, FileRecord> fileMap;
    for (const FileRecord &f : files)
        fileMap[f.id] = f;

    // Find games with sparse metadata
    struct EnrichCandidate {
        int gameId;
        int fileId;
        QString title;
        QString system;
    };
    QList<EnrichCandidate> candidates;
    QSet<int> seenGames;

    for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
        const Database::MatchResult &m = it.value();
        if (seenGames.contains(m.gameId)) continue;
        seenGames.insert(m.gameId);

        const bool sparse = m.description.isEmpty() && m.genre.isEmpty() && m.players.isEmpty();
        if (!sparse) continue;

        const QString system = fileMap.contains(m.fileId)
            ? ctx.db.getSystemDisplayName(fileMap[m.fileId].systemId)
            : QString();
        candidates.append({m.gameId, m.fileId, m.gameTitle, system});
    }

    if (candidates.isEmpty()) {
        qInfo() << "All matched games already have metadata — nothing to enrich.";
        return 0;
    }

    qInfo() << "Found" << candidates.size() << "game(s) with sparse metadata";
    qInfo() << "";

    int enriched = 0, failed = 0;

    for (const auto &c : candidates) {
        qInfo() << "Enriching:" << c.title << "(" << c.system << ")";

        GameMetadata metadata = orchestrator->searchWithFallback(
            QString(), c.title, c.system);

        if (metadata.title.isEmpty()) {
            qInfo() << "  ✗ No metadata found";
            failed++;
            continue;
        }

        const QString genres = metadata.genres.join(", ");
        const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();

        bool updated = ctx.db.updateGame(c.gameId,
                                         metadata.publisher,
                                         metadata.developer,
                                         metadata.releaseDate,
                                         metadata.description,
                                         genres,
                                         players,
                                         metadata.rating);
        if (updated) {
            qInfo() << "  ✓ Enriched from" << metadata.providerId;
            enriched++;
        } else {
            qInfo() << "  ✗ Failed to update database";
            failed++;
        }
    }

    qInfo() << "";
    qInfo() << "=== Enrichment Complete ===";
    qInfo() << "Enriched:" << enriched;
    qInfo() << "Failed:"   << failed;
    return 0;
}
