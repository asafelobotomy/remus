#include "app_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include "../../core/constants/constants.h"
#include "../../metadata/compendium_provider.h"
#include "../../metadata/gametdb_provider.h"
#include "../../metadata/hasheous_provider.h"
#include "../../metadata/igdb_provider.h"
#include "../../metadata/metadata_cache.h"
#include "../../metadata/provider_orchestrator.h"
#include "../../metadata/retroachievements_provider.h"
#include "../../metadata/screenscraper_provider.h"
#include "../../metadata/thegamesdb_provider.h"
#include "../../metadata/wikidata_provider.h"

namespace Remus {

namespace {

QSettings remusSettings()
{
    return QSettings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                     QString::fromLatin1(Constants::SETTINGS_APPLICATION));
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
        cwd + "/../../" + seg,
    };

    for (const QString &candidate : candidates) {
        if (QDir(candidate).exists()) {
            return QDir::cleanPath(candidate);
        }
    }

    return QString();
}

QString findGameTdbDir()
{
    return findDataSubdir(QStringLiteral("gametdb"));
}

int providerPriorityOrDefault(const QString &providerName, int fallback)
{
    const auto providerInfo = Constants::Providers::getProviderInfo(providerName);
    return providerInfo ? providerInfo->priority : fallback;
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    rebuildOrchestrator();
}

AppController::~AppController()
{
    closeLibrary();
}

bool AppController::openLibrary(const QString &dbPath)
{
    const QString cleanedPath = QDir::cleanPath(dbPath.trimmed());
    if (cleanedPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Select a database path first."));
        return false;
    }

    QFileInfo info(cleanedPath);
    const QString parentDir = info.absolutePath();
    if (!QDir().mkpath(parentDir)) {
        setStatusMessage(QStringLiteral("Cannot create parent directory: %1").arg(parentDir));
        return false;
    }

    m_database.close();
    if (!m_database.initialize(cleanedPath)) {
        setStatusMessage(QStringLiteral("Failed to open library database: %1").arg(cleanedPath));
        return false;
    }

    const bool pathChanged = m_libraryPath != cleanedPath;
    const bool wasClosed = !m_libraryOpen;
    m_libraryPath = cleanedPath;
    m_libraryOpen = true;
    m_selectedFileId = 0;
    m_selectedGameId = 0;
    rebuildOrchestrator();
    setStatusMessage(QStringLiteral("Library ready: %1").arg(QFileInfo(cleanedPath).fileName()));

    if (pathChanged) {
        emit libraryPathChanged();
    }
    if (wasClosed) {
        emit libraryOpenChanged();
    }
    emit selectedFileChanged();
    emit selectedGameChanged();
    emit libraryOpened();
    return true;
}

void AppController::closeLibrary()
{
    const bool wasOpen = m_libraryOpen;
    m_database.close();
    m_cache.reset();
    m_orchestrator.reset();
    m_libraryPath.clear();
    m_libraryOpen = false;
    m_selectedFileId = 0;
    m_selectedGameId = 0;

    if (wasOpen) {
        emit libraryPathChanged();
        emit libraryOpenChanged();
        emit selectedFileChanged();
        emit selectedGameChanged();
        emit orchestratorChanged();
        emit libraryClosed();
    }
}

bool AppController::eraseLibraryDatabase()
{
    if (!m_libraryOpen) {
        setStatusMessage(QStringLiteral("No library is open."));
        return false;
    }

    const QString dbPath = m_libraryPath;
    closeLibrary();

    if (QFile::exists(dbPath) && !QFile::remove(dbPath)) {
        setStatusMessage(QStringLiteral("Failed to erase library database: %1").arg(dbPath));
        return false;
    }

    const bool reopened = openLibrary(dbPath);
    if (reopened) {
        emit libraryDatabaseErased();
        setStatusMessage(QStringLiteral("Library database erased and reset."));
    } else {
        setStatusMessage(QStringLiteral("Library database erased but could not reopen: %1").arg(dbPath));
    }
    return reopened;
}

QString AppController::defaultLibraryPath() const
{
    const QString documentsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documentsDir).filePath(QStringLiteral("remus-library.db"));
}

QVariantMap AppController::selectedFile()
{
    QVariantMap result;
    if (!m_libraryOpen || m_selectedFileId <= 0) {
        return result;
    }

    const FileRecord file = m_database.getFileById(m_selectedFileId);
    if (file.id <= 0) {
        return result;
    }

    result.insert(QStringLiteral("id"), file.id);
    result.insert(QStringLiteral("libraryId"), file.libraryId);
    result.insert(QStringLiteral("path"), file.currentPath);
    result.insert(QStringLiteral("filename"), file.filename);
    result.insert(QStringLiteral("extension"), file.extension);
    result.insert(QStringLiteral("systemId"), file.systemId);
    result.insert(QStringLiteral("systemName"), systemName(file.systemId));
    result.insert(QStringLiteral("crc32"), file.crc32);
    result.insert(QStringLiteral("md5"), file.md5);
    result.insert(QStringLiteral("sha1"), file.sha1);
    result.insert(QStringLiteral("fileSize"), file.fileSize);
    result.insert(QStringLiteral("baseTitle"), file.baseTitle);
    result.insert(QStringLiteral("archivePath"), file.archivePath);
    result.insert(QStringLiteral("isCompressed"), file.isCompressed);
    return result;
}

QVariantMap AppController::selectedMatch()
{
    QVariantMap result;
    if (!m_libraryOpen || m_selectedFileId <= 0) {
        return result;
    }

    const Database::MatchResult match = m_database.getMatchForFile(m_selectedFileId);
    if (match.matchId <= 0) {
        return result;
    }

    result.insert(QStringLiteral("matchId"), match.matchId);
    result.insert(QStringLiteral("fileId"), match.fileId);
    result.insert(QStringLiteral("gameId"), match.gameId);
    result.insert(QStringLiteral("systemId"), match.systemId);
    result.insert(QStringLiteral("systemName"), systemName(match.systemId));
    result.insert(QStringLiteral("title"), match.gameTitle);
    result.insert(QStringLiteral("confidence"), match.confidence);
    result.insert(QStringLiteral("method"), match.matchMethod);
    result.insert(QStringLiteral("publisher"), match.publisher);
    result.insert(QStringLiteral("developer"), match.developer);
    result.insert(QStringLiteral("description"), match.description);
    result.insert(QStringLiteral("genre"), match.genre);
    result.insert(QStringLiteral("players"), match.players);
    result.insert(QStringLiteral("region"), match.region);
    result.insert(QStringLiteral("releaseYear"), match.releaseYear);
    result.insert(QStringLiteral("rating"), match.rating);
    result.insert(QStringLiteral("confirmed"), match.isConfirmed);
    result.insert(QStringLiteral("rejected"), match.isRejected);
    return result;
}

QString AppController::systemName(int systemId)
{
    if (!m_libraryOpen || systemId <= 0) {
        return QString();
    }

    return m_database.getSystemDisplayName(systemId);
}

void AppController::setCurrentView(int view)
{
    // Clamp to the valid 4-view range; guards against stale persisted values.
    const int clamped = qBound(0, view, 3);
    if (m_currentView == clamped) {
        return;
    }

    m_currentView = clamped;
    emit currentViewChanged();
}

void AppController::setSelectedFileId(int fileId)
{
    if (m_selectedFileId == fileId) {
        refreshSelectedMatch();
        return;
    }

    m_selectedFileId = fileId;
    emit selectedFileChanged();
    refreshSelectedMatch();
}

void AppController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged();
}

void AppController::rebuildOrchestrator()
{
    m_cache.reset();
    m_orchestrator = std::make_unique<ProviderOrchestrator>(this);
    if (m_libraryOpen) {
        // MetadataCache is owned by m_cache; parenting it to the orchestrator
        // would make a second rebuild double-delete it during replacement.
        m_cache = std::make_unique<MetadataCache>(m_database.database());
        m_orchestrator->setCache(m_cache.get());
    }

    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (!compendiumDir.isEmpty()) {
        const QString compendiumPath = QDir(compendiumDir).filePath(QStringLiteral("remus_compendium.db"));
        if (QFileInfo::exists(compendiumPath)) {
            auto *compendiumProvider = new CompendiumProvider(m_orchestrator.get());
            if (compendiumProvider->openDatabase(compendiumPath)) {
                m_orchestrator->addProvider(
                    Constants::Providers::COMPENDIUM,
                    compendiumProvider,
                    providerPriorityOrDefault(Constants::Providers::COMPENDIUM, 180));
            } else {
                compendiumProvider->deleteLater();
            }
        }
    }

    m_orchestrator->addProvider(
        Constants::Providers::HASHEOUS,
        new HasheousProvider(m_orchestrator.get()),
        providerPriorityOrDefault(Constants::Providers::HASHEOUS, 80));

    QSettings settings = remusSettings();

    const QString ssUser = settings.value(QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_USERNAME)).toString().trimmed();
    const QString ssPass = settings.value(QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_PASSWORD)).toString().trimmed();
    if (!ssUser.isEmpty() && !ssPass.isEmpty()) {
        auto *provider = new ScreenScraperProvider(m_orchestrator.get());
        provider->setCredentials(ssUser, ssPass);

        const QString ssDevId = settings.value(QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_DEVID)).toString().trimmed();
        const QString ssDevPass = settings.value(QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_DEVPASSWORD)).toString().trimmed();
        if (!ssDevId.isEmpty() && !ssDevPass.isEmpty()) {
            provider->setDeveloperCredentials(ssDevId, ssDevPass);
        }

        m_orchestrator->addProvider(
            Constants::Providers::SCREENSCRAPER,
            provider,
            providerPriorityOrDefault(Constants::Providers::SCREENSCRAPER, 90));
    }

    const QString gametdbDir = findGameTdbDir();
    if (!gametdbDir.isEmpty()) {
        auto *provider = new GameTDBProvider(m_orchestrator.get());
        if (provider->loadDatabases(gametdbDir) > 0) {
            m_orchestrator->addProvider(
                Constants::Providers::GAMETDB,
                provider,
                providerPriorityOrDefault(Constants::Providers::GAMETDB, 150));
        } else {
            provider->deleteLater();
        }
    }

    auto *tgdbProvider = new TheGamesDBProvider(m_orchestrator.get());
    const QString tgdbApiKey = settings.value(QString::fromLatin1(Constants::Settings::Providers::THEGAMESDB_API_KEY)).toString().trimmed();
    if (!tgdbApiKey.isEmpty()) {
        tgdbProvider->setApiKey(tgdbApiKey);
    }
    m_orchestrator->addProvider(
        Constants::Providers::THEGAMESDB,
        tgdbProvider,
        providerPriorityOrDefault(Constants::Providers::THEGAMESDB, 50));

    const QString igdbClientId = settings.value(QString::fromLatin1(Constants::Settings::Providers::IGDB_CLIENT_ID)).toString().trimmed();
    const QString igdbClientSecret = settings.value(QString::fromLatin1(Constants::Settings::Providers::IGDB_CLIENT_SECRET)).toString().trimmed();
    if (!igdbClientId.isEmpty() && !igdbClientSecret.isEmpty()) {
        auto *igdbProvider = new IGDBProvider(m_orchestrator.get());
        igdbProvider->setCredentials(igdbClientId, igdbClientSecret);
        m_orchestrator->addProvider(
            Constants::Providers::IGDB,
            igdbProvider,
            providerPriorityOrDefault(Constants::Providers::IGDB, 70));
    }

    const QString raUser = settings.value(QString::fromLatin1(Constants::Settings::Providers::RETROACHIEVEMENTS_USERNAME)).toString().trimmed();
    const QString raApiKey = settings.value(QString::fromLatin1(Constants::Settings::Providers::RETROACHIEVEMENTS_API_KEY)).toString().trimmed();
    if (!raUser.isEmpty() && !raApiKey.isEmpty()) {
        auto *provider = new RetroAchievementsProvider(m_orchestrator.get());
        provider->setCredentials(raUser, raApiKey);
        m_orchestrator->addProvider(
            Constants::Providers::RETROACHIEVEMENTS,
            provider,
            providerPriorityOrDefault(Constants::Providers::RETROACHIEVEMENTS, 60));
    }

    m_orchestrator->addProvider(
        Constants::Providers::WIKIDATA,
        new WikidataProvider(m_orchestrator.get()),
        providerPriorityOrDefault(Constants::Providers::WIKIDATA, 40));

    emit orchestratorChanged();
}

void AppController::refreshSelectedMatch()
{
    int nextGameId = 0;
    if (m_libraryOpen && m_selectedFileId > 0) {
        nextGameId = m_database.getMatchForFile(m_selectedFileId).gameId;
    }

    if (m_selectedGameId != nextGameId) {
        m_selectedGameId = nextGameId;
        emit selectedGameChanged();
    }

    // Always notify QML so metadata fields re-evaluate after any match DB change.
    emit selectedMatchDataChanged();
}

} // namespace Remus