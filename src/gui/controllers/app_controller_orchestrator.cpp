#include "app_controller.h"

#include "services/secret_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include "../../core/constants/constants.h"
#include "../../core/compendium_disc_bridge.h"
#include "../../metadata/compendium_provider.h"
#include "../../metadata/gametdb_provider.h"
#include "../../metadata/hasheous_provider.h"
#include "../../metadata/igdb_provider.h"
#include "../../metadata/metadata_cache.h"
#include "../../metadata/playmatch_provider.h"
#include "../../metadata/provider_orchestrator.h"
#include "../../metadata/retroachievements_provider.h"
#include "../../metadata/screenscraper_provider.h"
#include "../../metadata/steamgriddb_provider.h"
#include "../../metadata/thegamesdb_provider.h"
#include "../../metadata/wikidata_provider.h"

namespace Remus {

namespace {

    QSettings remusSettings() {
        return QSettings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
            QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    }

    QString findDataSubdir(const QString &subdir) {
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
            cwd + "/../../" + seg,
        };

        for (const QString &candidate : candidates) {
            if (QDir(candidate).exists()) {
                return QDir::cleanPath(candidate);
            }
        }

        return QString();
    }

    QString findGameTdbDir() {
        return findDataSubdir(QStringLiteral("gametdb"));
    }

    int providerPriorityOrDefault(const QString &providerName, int fallback) {
        const auto providerInfo = Constants::Providers::getProviderInfo(providerName);
        return providerInfo ? providerInfo->priority : fallback;
    }

    ProviderOrchestrator::OrchestratorMode resolveGuiOrchestratorMode(bool compendiumAvailable) {
        const QString mode = remusSettings()
                                 .value(QStringLiteral("metadata/runtime_mode"), QStringLiteral("compendium-only"))
                                 .toString()
                                 .trimmed()
                                 .toLower();
        if (mode == QLatin1String("online-fallback") || mode == QLatin1String("compendium-preferred")) {
            return ProviderOrchestrator::OrchestratorMode::CompendiumPreferred;
        }
        if (compendiumAvailable) {
            return ProviderOrchestrator::OrchestratorMode::CompendiumOnly;
        }
        return ProviderOrchestrator::OrchestratorMode::CompendiumPreferred;
    }

    void registerGuiRemoteProviders(AppController *parent, ProviderOrchestrator *orchestrator) {
        auto secretValue = [](const char *key) -> QString {
            const SecretStore::ReadResult kr = SecretStore::readWithStatus(QString::fromLatin1(key));
            if (kr.status == SecretStore::ReadResult::Status::Found)
                return kr.value;
            if (kr.status == SecretStore::ReadResult::Status::BackendError)
                return { };
            return remusSettings().value(QString::fromLatin1(key)).toString().trimmed();
        };

        {
            auto *hasheousProvider = new HasheousProvider(orchestrator);
            const QString hasheousKey = secretValue(Constants::Settings::Providers::HASHEOUS_CLIENT_API_KEY);
            if (!hasheousKey.isEmpty())
                hasheousProvider->setApiKey(hasheousKey);
            orchestrator->addProvider(Constants::Providers::HASHEOUS, hasheousProvider,
                providerPriorityOrDefault(Constants::Providers::HASHEOUS, Constants::Providers::Priority::HASHEOUS));
        }

        {
            auto *playmatchProvider = new PlayMatchProvider(orchestrator);
            orchestrator->addProvider(Constants::Providers::PLAYMATCH, playmatchProvider,
                providerPriorityOrDefault(Constants::Providers::PLAYMATCH, Constants::Providers::Priority::PLAYMATCH));
        }

        const QString ssUser = secretValue(Constants::Settings::Providers::SCREENSCRAPER_USERNAME);
        const QString ssPass = secretValue(Constants::Settings::Providers::SCREENSCRAPER_PASSWORD);
        if (!ssUser.isEmpty() && !ssPass.isEmpty()) {
            auto *provider = new ScreenScraperProvider(orchestrator);
            provider->setCredentials(ssUser, ssPass);

            const QString ssDevId = secretValue(Constants::Settings::Providers::SCREENSCRAPER_DEVID);
            const QString ssDevPass = secretValue(Constants::Settings::Providers::SCREENSCRAPER_DEVPASSWORD);
            if (!ssDevId.isEmpty() && !ssDevPass.isEmpty()) {
                provider->setDeveloperCredentials(ssDevId, ssDevPass);
            }

            orchestrator->addProvider(Constants::Providers::SCREENSCRAPER, provider,
                providerPriorityOrDefault(
                    Constants::Providers::SCREENSCRAPER, Constants::Providers::Priority::SCREENSCRAPER));
        }

        const QString gametdbDir = findGameTdbDir();
        if (!gametdbDir.isEmpty()) {
            auto *provider = new GameTDBProvider(orchestrator);
            if (provider->loadDatabases(gametdbDir) > 0) {
                orchestrator->addProvider(Constants::Providers::GAMETDB, provider,
                    providerPriorityOrDefault(Constants::Providers::GAMETDB, Constants::Providers::Priority::GAMETDB));
            } else {
                provider->deleteLater();
            }
        }

        const QString tgdbApiKey = secretValue(Constants::Settings::Providers::THEGAMESDB_API_KEY);
        if (!tgdbApiKey.isEmpty()) {
            auto *tgdbProvider = new TheGamesDBProvider(orchestrator);
            tgdbProvider->setApiKey(tgdbApiKey);
            orchestrator->addProvider(Constants::Providers::THEGAMESDB, tgdbProvider,
                providerPriorityOrDefault(
                    Constants::Providers::THEGAMESDB, Constants::Providers::Priority::THEGAMESDB));
        }

        const QString igdbClientId = secretValue(Constants::Settings::Providers::IGDB_CLIENT_ID);
        const QString igdbClientSecret = secretValue(Constants::Settings::Providers::IGDB_CLIENT_SECRET);
        if (!igdbClientId.isEmpty() && !igdbClientSecret.isEmpty()) {
            auto *igdbProvider = new IGDBProvider(orchestrator);
            igdbProvider->setCredentials(igdbClientId, igdbClientSecret);
            orchestrator->addProvider(Constants::Providers::IGDB, igdbProvider,
                providerPriorityOrDefault(Constants::Providers::IGDB, Constants::Providers::Priority::IGDB));
        }

        const QString sgdbApiKey = secretValue(Constants::Settings::Providers::STEAMGRIDDB_API_KEY);
        if (!sgdbApiKey.isEmpty()) {
            auto *sgdbProvider = new SteamGridDBProvider(orchestrator);
            sgdbProvider->setApiKey(sgdbApiKey);
            orchestrator->addProvider(Constants::Providers::STEAMGRIDDB, sgdbProvider,
                providerPriorityOrDefault(
                    Constants::Providers::STEAMGRIDDB, Constants::Providers::Priority::STEAMGRIDDB));
        }

        const QString raUser = secretValue(Constants::Settings::Providers::RETROACHIEVEMENTS_USERNAME);
        const QString raApiKey = secretValue(Constants::Settings::Providers::RETROACHIEVEMENTS_API_KEY);
        if (!raUser.isEmpty() && !raApiKey.isEmpty()) {
            auto *provider = new RetroAchievementsProvider(orchestrator);
            provider->setCredentials(raUser, raApiKey);
            orchestrator->addProvider(Constants::Providers::RETROACHIEVEMENTS, provider,
                providerPriorityOrDefault(
                    Constants::Providers::RETROACHIEVEMENTS, Constants::Providers::Priority::RETROACHIEVEMENTS));
        }

        orchestrator->addProvider(Constants::Providers::WIKIDATA, new WikidataProvider(orchestrator),
            providerPriorityOrDefault(Constants::Providers::WIKIDATA, Constants::Providers::Priority::WIKIDATA));

        Q_UNUSED(parent);
    }

} // namespace

void AppController::rebuildOrchestrator() {
    m_cache.reset();
    m_orchestrator = std::make_unique<ProviderOrchestrator>(this);
    if (m_libraryOpen) {
        m_cache = std::make_unique<MetadataCache>(m_database.database());
        m_orchestrator->setCache(m_cache.get());
    }

    bool compendiumAvailable = false;
    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (!compendiumDir.isEmpty()) {
        const QString compendiumPath = QDir(compendiumDir).filePath(QStringLiteral("remus_compendium.db"));
        if (QFileInfo::exists(compendiumPath)) {
            compendiumAvailable = true;
            m_database.setCompendiumDbPath(compendiumPath);
            auto *compendiumProvider = new CompendiumProvider(m_orchestrator.get());
            if (compendiumProvider->openDatabase(compendiumPath)) {
                const auto mode = resolveGuiOrchestratorMode(true);
                m_orchestrator->setMode(mode);
                compendiumProvider->setStrictOffline(mode == ProviderOrchestrator::OrchestratorMode::CompendiumOnly);
                m_orchestrator->addProvider(Constants::Providers::COMPENDIUM, compendiumProvider,
                    providerPriorityOrDefault(
                        Constants::Providers::COMPENDIUM, Constants::Providers::Priority::COMPENDIUM));
            } else {
                compendiumProvider->deleteLater();
                compendiumAvailable = false;
            }
        }
    }

    const auto mode = resolveGuiOrchestratorMode(compendiumAvailable);
    m_orchestrator->setMode(mode);

    if (mode == ProviderOrchestrator::OrchestratorMode::CompendiumOnly) {
        emit orchestratorChanged();
        return;
    }

    registerGuiRemoteProviders(this, m_orchestrator.get());
    emit orchestratorChanged();
}

} // namespace Remus
