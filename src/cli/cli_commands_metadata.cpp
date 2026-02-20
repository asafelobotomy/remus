#include "cli_commands.h"
#include <memory>
#include "../metadata/metadata_provider.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../core/constants/constants.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

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
    if (providerName == Providers::THEGAMESDB) return std::make_unique<TheGamesDBProvider>();
    if (providerName == Providers::IGDB)       return std::make_unique<IGDBProvider>();
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
    if (!provider) { qInfo() << "No provider selected (use --provider)"; return 0; }

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
    if (!provider) { qInfo() << "No provider selected (use --provider)"; return 0; }

    QList<SearchResult> results = provider->searchByName(title, system);
    if (results.isEmpty()) {
        qInfo() << "No results found for:" << title;
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
