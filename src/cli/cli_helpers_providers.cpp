#include "cli_helpers.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <memory>

#include "../metadata/compendium_provider.h"
#include "../metadata/gametdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/metadata_cache.h"
#include "../metadata/retroachievements_provider.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/wikidata_provider.h"

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

QString findDataSubdir(const QString &subdir)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QString seg = QStringLiteral("data/") + subdir;
    const QStringList candidates = {
        cwd + "/" + seg,
        appDir + "/" + seg,
        appDir + "/../" + seg,
        appDir + "/../../" + seg,
        appDir + "/../../../" + seg,
        cwd + "/../" + seg,
        cwd + "/../../" + seg
    };
    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::cleanPath(dir);
        }
    }
    return QString();
}

std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser,
                                                       Database *db)
{
    auto orchestrator = std::make_unique<ProviderOrchestrator>();

    if (db) {
        auto cache = std::make_unique<MetadataCache>(db->database(), orchestrator.get());
        orchestrator->setCache(cache.get());
        cache.release();
    }

    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (!compendiumDir.isEmpty()) {
        const QString compendiumPath = QDir(compendiumDir).filePath(QStringLiteral("remus_compendium.db"));
        if (QFileInfo::exists(compendiumPath)) {
            auto compendiumProvider = std::make_unique<CompendiumProvider>();
            if (compendiumProvider->openDatabase(compendiumPath)) {
                const auto compendiumInfo = Providers::getProviderInfo(Providers::COMPENDIUM);
                orchestrator->addProvider(Providers::COMPENDIUM, compendiumProvider.get(),
                                          compendiumInfo ? compendiumInfo->priority : 180);
                compendiumProvider.release();
            }
        }
    }

    auto hasheousProvider = std::make_unique<HasheousProvider>();
    const auto hasheousInfo = Providers::getProviderInfo(Providers::HASHEOUS);
    orchestrator->addProvider(Providers::HASHEOUS, hasheousProvider.get(),
                              hasheousInfo ? hasheousInfo->priority : 80);
    hasheousProvider.release();

    if (parser.isSet("ss-user") && parser.isSet("ss-pass")) {
        auto ssProvider = std::make_unique<ScreenScraperProvider>();
        ssProvider->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
        if (parser.isSet("ss-devid") && parser.isSet("ss-devpass")) {
            ssProvider->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
        }
        const auto ssInfo = Providers::getProviderInfo(Providers::SCREENSCRAPER);
        orchestrator->addProvider(Providers::SCREENSCRAPER, ssProvider.get(),
                                  ssInfo ? ssInfo->priority : 90);
        ssProvider.release();
    }

    const QString gametdbDir = findGameTDBDir();
    if (!gametdbDir.isEmpty()) {
        auto gametdbProvider = std::make_unique<GameTDBProvider>();
        const int gametdbLoaded = gametdbProvider->loadDatabases(gametdbDir);
        if (gametdbLoaded > 0) {
            const auto gametdbInfo = Providers::getProviderInfo(Providers::GAMETDB);
            orchestrator->addProvider(Providers::GAMETDB, gametdbProvider.get(),
                                      gametdbInfo ? gametdbInfo->priority : 150);
            gametdbProvider.release();
        }
    }

    const QString tgdbApiKey = parserOrSetting(parser,
                                               QStringLiteral("tgdb-api-key"),
                                               Settings::Providers::THEGAMESDB_API_KEY);
    if (!tgdbApiKey.isEmpty()) {
        auto tgdbProvider = std::make_unique<TheGamesDBProvider>();
        tgdbProvider->setApiKey(tgdbApiKey);
        const auto tgdbInfo = Providers::getProviderInfo(Providers::THEGAMESDB);
        orchestrator->addProvider(Providers::THEGAMESDB, tgdbProvider.get(),
                                  tgdbInfo ? tgdbInfo->priority : 50);
        tgdbProvider.release();
    } else {
        qInfo() << "TheGamesDB: skipped (no API key configured)";
    }

    const QString igdbClientId = parserOrSetting(parser,
                                                 QStringLiteral("igdb-client-id"),
                                                 Settings::Providers::IGDB_CLIENT_ID);
    const QString igdbClientSecret = parserOrSetting(parser,
                                                     QStringLiteral("igdb-client-secret"),
                                                     Settings::Providers::IGDB_CLIENT_SECRET);
    if (!igdbClientId.isEmpty() && !igdbClientSecret.isEmpty()) {
        auto igdbProvider = std::make_unique<IGDBProvider>();
        igdbProvider->setCredentials(igdbClientId, igdbClientSecret);
        const auto igdbInfo = Providers::getProviderInfo(Providers::IGDB);
        orchestrator->addProvider(Providers::IGDB, igdbProvider.get(),
                                  igdbInfo ? igdbInfo->priority : 70);
        igdbProvider.release();
    }

    const QString raUsername = parserOrSetting(parser,
                                               QStringLiteral("ra-user"),
                                               Settings::Providers::RETROACHIEVEMENTS_USERNAME);
    const QString raApiKey = parserOrSetting(parser,
                                             QStringLiteral("ra-api-key"),
                                             Settings::Providers::RETROACHIEVEMENTS_API_KEY);
    if (!raUsername.isEmpty() && !raApiKey.isEmpty()) {
        auto raProvider = std::make_unique<RetroAchievementsProvider>();
        raProvider->setCredentials(raUsername, raApiKey);
        const auto raInfo = Providers::getProviderInfo(Providers::RETROACHIEVEMENTS);
        orchestrator->addProvider(Providers::RETROACHIEVEMENTS, raProvider.get(),
                                  raInfo ? raInfo->priority : 60);
        raProvider.release();
    }

    auto wikidataProvider = std::make_unique<WikidataProvider>();
    const auto wdInfo = Providers::getProviderInfo(Providers::WIKIDATA);
    orchestrator->addProvider(Providers::WIKIDATA, wikidataProvider.get(),
                              wdInfo ? wdInfo->priority : 40);
    wikidataProvider.release();

    return orchestrator;
}