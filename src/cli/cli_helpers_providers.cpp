#include "cli_helpers.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <memory>

#include "../metadata/compendium_provider.h"
#include "../metadata/gametdb_provider.h"
#include "../core/hasheous_config.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/metadata_cache.h"
#include "../metadata/playmatch_provider.h"
#include "../metadata/retroachievements_provider.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/steamgriddb_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/wikidata_provider.h"
#include "../services/credential_manager.h"

using namespace Remus;
using namespace Remus::Constants;

/// Resolve a provider secret: CLI flag → CredentialManager (env var → OS keychain → legacy QSettings).
/// Emits a security warning when the secret arrives via argv, which is visible
/// in shell history and process listings.
QString resolveSecret(const QCommandLineParser &parser, const QString &optionName, const char *settingKey) {
    if (parser.isSet(optionName)) {
        if (qEnvironmentVariableIsSet("REMUS_DEPRECATE_ARGV_SECRETS")) {
            qCritical().noquote() << QStringLiteral(
                "Security: --%1 is rejected (REMUS_DEPRECATE_ARGV_SECRETS is set). "
                "Use the matching REMUS_* environment variable or the OS keychain instead.")
                                         .arg(optionName);
            return QString();
        }
        qWarning().noquote() << QStringLiteral(
            "Security: --%1 passes a secret via argv (visible in process listings and shell history). "
            "Argv secrets are deprecated and will be removed in a future release — use the matching "
            "REMUS_* environment variable or the OS keychain instead.")
                                    .arg(optionName);
        return parser.value(optionName).trimmed();
    }
    return CredentialManager::get(QString::fromLatin1(settingKey));
}

QString findDataSubdir(const QString &subdir) {
    const QByteArray dataRoot = qgetenv("REMUS_DATA_DIR");
    const bool isolatedEnrichmentRoot = !dataRoot.isEmpty() && subdir != QLatin1String("compendium");
    if (isolatedEnrichmentRoot) {
        const QString path = QDir::cleanPath(QString::fromLocal8Bit(dataRoot) + QStringLiteral("/data/") + subdir);
        return QDir(path).exists() ? path : QString();
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QString seg = QStringLiteral("data/") + subdir;
    QStringList candidates;
    if (!dataRoot.isEmpty()) {
        candidates << QDir::cleanPath(QString::fromLocal8Bit(dataRoot) + QStringLiteral("/data/") + subdir);
    }
    candidates << QStringList { cwd + "/" + seg, appDir + "/" + seg, appDir + "/../" + seg, appDir + "/../../" + seg,
        appDir + "/../../../" + seg, cwd + "/../" + seg, cwd + "/../../" + seg };
    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::cleanPath(dir);
        }
    }
    return QString();
}

namespace {

QString compendiumDatabasePath() {
    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (compendiumDir.isEmpty()) {
        return QString();
    }
    const QString path = QDir(compendiumDir).filePath(QStringLiteral("remus_compendium.db"));
    return QFileInfo::exists(path) ? path : QString();
}

void registerRemoteProviders(ProviderOrchestrator &orchestrator, const QCommandLineParser &parser) {
    auto hasheousProvider = std::make_unique<HasheousProvider>();
    {
        const QString hasheousKey
            = resolveSecret(parser, QStringLiteral("hasheous-api-key"), Settings::Providers::HASHEOUS_CLIENT_API_KEY);
        if (!hasheousKey.isEmpty())
            hasheousProvider->setApiKey(hasheousKey);
        if (parser.isSet(QStringLiteral("hasheous-base-url"))) {
            hasheousProvider->setBaseUrl(resolveHasheousBaseUrl(parser.value(QStringLiteral("hasheous-base-url"))));
        }
    }
    const auto hasheousInfo = Providers::getProviderInfo(Providers::HASHEOUS);
    orchestrator.addProvider(Providers::HASHEOUS, hasheousProvider.get(), hasheousInfo ? hasheousInfo->priority : 80);
    hasheousProvider.release();

    auto playmatchProvider = std::make_unique<PlayMatchProvider>();
    const auto playmatchInfo = Providers::getProviderInfo(Providers::PLAYMATCH);
    orchestrator.addProvider(Providers::PLAYMATCH, playmatchProvider.get(),
        playmatchInfo ? playmatchInfo->priority : Constants::Providers::Priority::PLAYMATCH);
    playmatchProvider.release();

    const QString ssUser
        = resolveSecret(parser, QStringLiteral("ss-user"), Settings::Providers::SCREENSCRAPER_USERNAME);
    const QString ssPass
        = resolveSecret(parser, QStringLiteral("ss-pass"), Settings::Providers::SCREENSCRAPER_PASSWORD);
    if (!ssUser.isEmpty() && !ssPass.isEmpty()) {
        auto ssProvider = std::make_unique<ScreenScraperProvider>();
        ssProvider->setCredentials(ssUser, ssPass);
        const QString ssDevId
            = resolveSecret(parser, QStringLiteral("ss-devid"), Settings::Providers::SCREENSCRAPER_DEVID);
        const QString ssDevPass
            = resolveSecret(parser, QStringLiteral("ss-devpass"), Settings::Providers::SCREENSCRAPER_DEVPASSWORD);
        if (!ssDevId.isEmpty() && !ssDevPass.isEmpty()) {
            ssProvider->setDeveloperCredentials(ssDevId, ssDevPass);
        }
        const auto ssInfo = Providers::getProviderInfo(Providers::SCREENSCRAPER);
        orchestrator.addProvider(Providers::SCREENSCRAPER, ssProvider.get(), ssInfo ? ssInfo->priority : 90);
        ssProvider.release();
    }

    const QString gametdbDir = findGameTDBDir();
    if (!gametdbDir.isEmpty()) {
        auto gametdbProvider = std::make_unique<GameTDBProvider>();
        const int gametdbLoaded = gametdbProvider->loadDatabases(gametdbDir);
        if (gametdbLoaded > 0) {
            const auto gametdbInfo = Providers::getProviderInfo(Providers::GAMETDB);
            orchestrator.addProvider(
                Providers::GAMETDB, gametdbProvider.get(), gametdbInfo ? gametdbInfo->priority : 150);
            gametdbProvider.release();
        }
    }

    const QString tgdbApiKey
        = resolveSecret(parser, QStringLiteral("tgdb-api-key"), Settings::Providers::THEGAMESDB_API_KEY);
    if (!tgdbApiKey.isEmpty()) {
        auto tgdbProvider = std::make_unique<TheGamesDBProvider>();
        tgdbProvider->setApiKey(tgdbApiKey);
        const auto tgdbInfo = Providers::getProviderInfo(Providers::THEGAMESDB);
        orchestrator.addProvider(Providers::THEGAMESDB, tgdbProvider.get(), tgdbInfo ? tgdbInfo->priority : 50);
        tgdbProvider.release();
    } else {
        qInfo() << "TheGamesDB: skipped (no API key configured)";
    }

    const QString igdbClientId
        = resolveSecret(parser, QStringLiteral("igdb-client-id"), Settings::Providers::IGDB_CLIENT_ID);
    const QString igdbClientSecret
        = resolveSecret(parser, QStringLiteral("igdb-client-secret"), Settings::Providers::IGDB_CLIENT_SECRET);
    if (!igdbClientId.isEmpty() && !igdbClientSecret.isEmpty()) {
        auto igdbProvider = std::make_unique<IGDBProvider>();
        igdbProvider->setCredentials(igdbClientId, igdbClientSecret);
        const auto igdbInfo = Providers::getProviderInfo(Providers::IGDB);
        orchestrator.addProvider(Providers::IGDB, igdbProvider.get(), igdbInfo ? igdbInfo->priority : 70);
        igdbProvider.release();
    }

    const QString sgdbApiKey
        = resolveSecret(parser, QStringLiteral("sgdb-api-key"), Settings::Providers::STEAMGRIDDB_API_KEY);
    if (!sgdbApiKey.isEmpty()) {
        auto sgdbProvider = std::make_unique<SteamGridDBProvider>();
        sgdbProvider->setApiKey(sgdbApiKey);
        const auto sgdbInfo = Providers::getProviderInfo(Providers::STEAMGRIDDB);
        orchestrator.addProvider(Providers::STEAMGRIDDB, sgdbProvider.get(),
            sgdbInfo ? sgdbInfo->priority : Constants::Providers::Priority::STEAMGRIDDB);
        sgdbProvider.release();
    } else {
        qInfo() << "SteamGridDB: skipped (no API key configured)";
    }

    const QString raUsername
        = resolveSecret(parser, QStringLiteral("ra-user"), Settings::Providers::RETROACHIEVEMENTS_USERNAME);
    const QString raApiKey
        = resolveSecret(parser, QStringLiteral("ra-api-key"), Settings::Providers::RETROACHIEVEMENTS_API_KEY);
    if (!raUsername.isEmpty() && !raApiKey.isEmpty()) {
        auto raProvider = std::make_unique<RetroAchievementsProvider>();
        raProvider->setCredentials(raUsername, raApiKey);
        const auto raInfo = Providers::getProviderInfo(Providers::RETROACHIEVEMENTS);
        orchestrator.addProvider(Providers::RETROACHIEVEMENTS, raProvider.get(), raInfo ? raInfo->priority : 60);
        raProvider.release();
    }

    auto wikidataProvider = std::make_unique<WikidataProvider>();
    const auto wdInfo = Providers::getProviderInfo(Providers::WIKIDATA);
    orchestrator.addProvider(Providers::WIKIDATA, wikidataProvider.get(), wdInfo ? wdInfo->priority : 40);
    wikidataProvider.release();
}

} // namespace

ProviderOrchestrator::OrchestratorMode resolveOrchestratorMode(
    const QCommandLineParser &parser, bool compendiumAvailable) {
    if (parser.isSet(QStringLiteral("online-fallback"))) {
        return ProviderOrchestrator::OrchestratorMode::CompendiumPreferred;
    }
    if (parser.isSet(QStringLiteral("compendium-only")) || parser.isSet(QStringLiteral("offline"))) {
        return ProviderOrchestrator::OrchestratorMode::CompendiumOnly;
    }
    if (compendiumAvailable) {
        return ProviderOrchestrator::OrchestratorMode::CompendiumOnly;
    }
    return ProviderOrchestrator::OrchestratorMode::CompendiumPreferred;
}

std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser, Database *db) {
    auto orchestrator = std::make_unique<ProviderOrchestrator>();

    if (db) {
        auto cache = std::make_unique<MetadataCache>(db->database(), orchestrator.get());
        orchestrator->setCache(cache.get());
        cache.release();
    }

    const QString compendiumPath = compendiumDatabasePath();
    const bool compendiumAvailable = !compendiumPath.isEmpty();
    const auto mode = resolveOrchestratorMode(parser, compendiumAvailable);
    orchestrator->setMode(mode);

    if (compendiumAvailable) {
        auto compendiumProvider = std::make_unique<CompendiumProvider>();
        if (compendiumProvider->openDatabase(compendiumPath)) {
            compendiumProvider->setStrictOffline(mode == ProviderOrchestrator::OrchestratorMode::CompendiumOnly);
            const auto compendiumInfo = Providers::getProviderInfo(Providers::COMPENDIUM);
            orchestrator->addProvider(
                Providers::COMPENDIUM, compendiumProvider.get(), compendiumInfo ? compendiumInfo->priority : 180);
            compendiumProvider.release();
        }
    }

    if (mode == ProviderOrchestrator::OrchestratorMode::CompendiumOnly) {
        if (!compendiumAvailable) {
            qWarning() << "Compendium-only mode requested but remus_compendium.db was not found — "
                          "run scripts/init_compendium.sh or pass --online-fallback";
        } else {
            qInfo() << "Runtime mode: compendium-only (no remote providers)";
        }
        return orchestrator;
    }

    registerRemoteProviders(*orchestrator, parser);
    qInfo() << "Runtime mode: compendium-preferred (online fallback enabled)";
    return orchestrator;
}

qint64 countCompendiumSignatures(const QString &dbPath) {
    if (dbPath.isEmpty() || !QFileInfo::exists(dbPath)) {
        return -1;
    }

    const QString connectionName
        = QStringLiteral("compendium_sig_count_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        QSqlDatabase::removeDatabase(connectionName);
        return -1;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM game_signatures")) || !query.next()) {
        db.close();
        QSqlDatabase::removeDatabase(connectionName);
        return -1;
    }

    const qint64 count = query.value(0).toLongLong();
    db.close();
    QSqlDatabase::removeDatabase(connectionName);
    return count;
}
