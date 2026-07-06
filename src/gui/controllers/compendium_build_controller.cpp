#include "compendium_build_controller.h"

#include "app_controller.h"
#include "settings_controller.h"

#include "../../core/constants/constants.h"
#include "../../services/credential_manager.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

#include <QtGlobal>

namespace Remus {

namespace {

    using namespace Constants::Settings::Providers;

    struct CredField {
        const char *key;
        const char *envVar;
    };

    static const CredField kCredFields[] = {
        { SCREENSCRAPER_USERNAME, "REMUS_SS_USER" },
        { SCREENSCRAPER_PASSWORD, "REMUS_SS_PASS" },
        { SCREENSCRAPER_DEVID, "REMUS_SS_DEVID" },
        { SCREENSCRAPER_DEVPASSWORD, "REMUS_SS_DEVPASS" },
        { THEGAMESDB_API_KEY, "REMUS_TGDB_API_KEY" },
        { IGDB_CLIENT_ID, "REMUS_IGDB_CLIENT_ID" },
        { IGDB_CLIENT_SECRET, "REMUS_IGDB_CLIENT_SECRET" },
        { HASHEOUS_CLIENT_API_KEY, "REMUS_HASHEOUS_API_KEY" },
        { RETROACHIEVEMENTS_USERNAME, "REMUS_RA_USERNAME" },
        { RETROACHIEVEMENTS_API_KEY, "REMUS_RA_API_KEY" },
        { STEAMGRIDDB_API_KEY, "REMUS_SGDB_API_KEY" },
    };

    static const QStringList kEnrichmentSourceKeys = {
        QStringLiteral("igdb"),
        QStringLiteral("screenscraper"),
        QStringLiteral("retroachievements"),
        QStringLiteral("ra"),
        QStringLiteral("launchbox"),
        QStringLiteral("gametdb"),
        QStringLiteral("openvgdb"),
        QStringLiteral("hasheous"),
        QStringLiteral("thegamesdb"),
        QStringLiteral("wikidata"),
        QStringLiteral("libretro"),
        QStringLiteral("mame-catver"),
        QStringLiteral("mame-listxml"),
        QStringLiteral("remus-thumbnails"),
        QStringLiteral("playmatch"),
        QStringLiteral("zxinfo"),
    };

    static const QHash<QString, QString> kProviderVerifyNames = {
        { QStringLiteral("igdb"), QStringLiteral("igdb") },
        { QStringLiteral("screenscraper"), QStringLiteral("screenscraper") },
        { QStringLiteral("retroachievements"), QStringLiteral("retroachievements") },
        { QStringLiteral("hasheous"), QStringLiteral("hasheous") },
        { QStringLiteral("thegamesdb"), QStringLiteral("thegamesdb") },
    };

    bool credentialGroupConfigured(const QString &groupKey) {
        auto nonEmpty
            = [](const char *key) { return !CredentialManager::get(QString::fromLatin1(key)).trimmed().isEmpty(); };

        if (groupKey == QStringLiteral("igdb")) {
            return nonEmpty(IGDB_CLIENT_ID) && nonEmpty(IGDB_CLIENT_SECRET);
        }
        if (groupKey == QStringLiteral("screenscraper")) {
            const bool userPass = nonEmpty(SCREENSCRAPER_USERNAME) && nonEmpty(SCREENSCRAPER_PASSWORD);
            const bool devCreds = nonEmpty(SCREENSCRAPER_DEVID) && nonEmpty(SCREENSCRAPER_DEVPASSWORD);
            return userPass || devCreds;
        }
        if (groupKey == QStringLiteral("retroachievements")) {
            return nonEmpty(RETROACHIEVEMENTS_USERNAME) && nonEmpty(RETROACHIEVEMENTS_API_KEY);
        }
        if (groupKey == QStringLiteral("hasheous")) {
            return nonEmpty(HASHEOUS_CLIENT_API_KEY);
        }
        if (groupKey == QStringLiteral("thegamesdb")) {
            return nonEmpty(THEGAMESDB_API_KEY);
        }
        if (groupKey == QStringLiteral("steamgriddb")) {
            return nonEmpty(STEAMGRIDDB_API_KEY);
        }
        return false;
    }

} // namespace

CompendiumBuildController::CompendiumBuildController(
    AppController *appController, SettingsController *settingsController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_settingsController(settingsController) {
    m_pollTimer.setInterval(2000);
    connect(&m_pollTimer, &QTimer::timeout, this, &CompendiumBuildController::pollProgress);
    if (m_settingsController) {
        connect(m_settingsController, &SettingsController::settingsChanged, this,
            &CompendiumBuildController::refreshCredentialStatus);
    }
    refreshPreflight();
    refreshCredentialStatus();
}

CompendiumBuildController::~CompendiumBuildController() {
    if (m_ownsProcess && m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

QString CompendiumBuildController::resolveRepoRoot() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/remus")),
        cwd,
        QDir(appDir).absoluteFilePath(QStringLiteral("../..")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../..")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../..")),
        QDir(cwd).absoluteFilePath(QStringLiteral("..")),
        QDir(cwd).absoluteFilePath(QStringLiteral("../..")),
    };

    for (const QString &candidate : candidates) {
        const QString cleaned = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleaned + QStringLiteral("/scripts/build_compendium_full.sh"))) {
            return cleaned;
        }
    }
    return QString();
}

QString CompendiumBuildController::resolveBuildScript() const {
    if (m_repoRoot.isEmpty()) {
        return QString();
    }
    return QDir(m_repoRoot).filePath(QStringLiteral("scripts/build_compendium_full.sh"));
}

QString CompendiumBuildController::resolveDetachedScript() const {
    if (m_repoRoot.isEmpty()) {
        return QString();
    }
    return QDir(m_repoRoot).filePath(QStringLiteral("scripts/run_compendium_full_build_detached.sh"));
}

QString CompendiumBuildController::resolveCliBinary() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString sibling = QDir(appDir).filePath(QStringLiteral("remus-cli"));
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    if (!m_repoRoot.isEmpty()) {
        const QString inBuild = QDir(m_repoRoot).filePath(QStringLiteral("build/remus-cli"));
        if (QFileInfo(inBuild).isExecutable()) {
            return inBuild;
        }
    }
    return QString();
}

QString CompendiumBuildController::resolveWritableCompendiumDbPath(const QString &repoRoot) const {
    const QByteArray overridePath = qgetenv("REMUS_COMPENDIUM_DB");
    if (!overridePath.isEmpty()) {
        return QFileInfo(QString::fromLocal8Bit(overridePath)).absoluteFilePath();
    }

    const QString bundledPath = QDir(repoRoot).filePath(QStringLiteral("data/compendium/remus_compendium.db"));
    QFileInfo bundledInfo(bundledPath);
    if (bundledInfo.exists()) {
        QFile probe(bundledPath);
        if (probe.open(QIODevice::Append)) {
            probe.close();
            return bundledInfo.absoluteFilePath();
        }
    } else {
        QDir parentDir = bundledInfo.absoluteDir();
        if (parentDir.exists() || parentDir.mkpath(QStringLiteral("."))) {
            QFile probe(bundledPath);
            if (probe.open(QIODevice::WriteOnly | QIODevice::Append)) {
                probe.close();
                QFile::remove(bundledPath);
                return bundledInfo.absoluteFilePath();
            }
        }
    }

    const QString userData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString userDb = QDir(userData).filePath(QStringLiteral("compendium/remus_compendium.db"));
    QDir().mkpath(QFileInfo(userDb).absolutePath());
    return userDb;
}

QString CompendiumBuildController::lockFilePath() const {
    const QString lockDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString baseName = QFileInfo(m_compendiumDbPath).fileName();
    return QDir(lockDir.isEmpty() ? QStringLiteral("/tmp") : lockDir).filePath(baseName + QStringLiteral(".lock"));
}

QString CompendiumBuildController::progressFilePath() const {
    return m_compendiumDbPath + QStringLiteral(".progress.json");
}

bool CompendiumBuildController::ensureCommandAvailable(const QString &command) const {
    QProcess probe;
    probe.start(QStringLiteral("bash"), { QStringLiteral("-lc"), QStringLiteral("command -v %1").arg(command) });
    if (!probe.waitForFinished(5000)) {
        return false;
    }
    return probe.exitCode() == 0;
}

bool CompendiumBuildController::isProcessAlive(qint64 pid) const {
    if (pid <= 0) {
        return false;
    }
    QProcess probe;
    probe.start(QStringLiteral("bash"), { QStringLiteral("-lc"), QStringLiteral("kill -0 %1").arg(pid) });
    probe.waitForFinished(2000);
    return probe.exitCode() == 0;
}

qint64 CompendiumBuildController::detachedBuildPid() const {
    QFile lock(lockFilePath());
    if (!lock.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    const QString line = QString::fromUtf8(lock.readLine()).trimmed();
    lock.close();
    bool ok = false;
    const qint64 pid = line.toLongLong(&ok);
    return ok ? pid : 0;
}

bool CompendiumBuildController::isBuildLockHeld() const {
    const qint64 pid = detachedBuildPid();
    return isProcessAlive(pid);
}

void CompendiumBuildController::refreshPreflight() {
    const QString previousDb = m_compendiumDbPath;
    m_repoRoot = resolveRepoRoot();
    m_compendiumDbPath = m_repoRoot.isEmpty() ? QString() : resolveWritableCompendiumDbPath(m_repoRoot);
    m_buildLogPath = QString::fromLocal8Bit(qgetenv("REMUS_COMPENDIUM_BUILD_LOG"));
    if (m_buildLogPath.isEmpty()) {
        m_buildLogPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                             .filePath(QStringLiteral("remus_compendium_full_build.log"));
    }

    QStringList blockers;
    m_preflightWarnings.clear();

    if (m_repoRoot.isEmpty()) {
        blockers.append(QStringLiteral("Could not locate the Remus repository (scripts/build_compendium_full.sh). "
                                       "Run from a development tree or use a packaged build with bundled scripts."));
    } else if (!QFileInfo(resolveCliBinary()).isExecutable()) {
        blockers.append(QStringLiteral("remus-cli was not found — run cmake --build build first."));
    }

    for (const QString &tool : { QStringLiteral("bash"), QStringLiteral("sqlite3"), QStringLiteral("flock") }) {
        if (!ensureCommandAvailable(tool)) {
            blockers.append(QStringLiteral("Required tool not found: %1").arg(tool));
        }
    }

    if (!ensureCommandAvailable(QStringLiteral("npm"))) {
        m_preflightWarnings.append(QStringLiteral("npm not found — thumbnail consolidate may fail (cwebp)."));
    }

    if (isBuildLockHeld()) {
        blockers.append(
            QStringLiteral("Another compendium build is already running (pid %1).").arg(detachedBuildPid()));
    }

    if (!m_compendiumDbPath.isEmpty()) {
        const QStorageInfo storage(m_compendiumDbPath);
        if (storage.isValid() && storage.bytesAvailable() > 0
            && storage.bytesAvailable() < qint64(15) * 1024 * 1024 * 1024) {
            m_preflightWarnings.append(QStringLiteral("Low disk space on output volume (< 15 GB free)."));
        }
    }

    bool networkLikely = false;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp) && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            networkLikely = true;
            break;
        }
    }
    if (!networkLikely) {
        m_preflightWarnings.append(QStringLiteral("No active network interface detected — DAT refresh may fail."));
    }

    if (!m_compendiumDbPath.isEmpty() && !QFileInfo::exists(m_compendiumDbPath)) {
        m_preflightWarnings.append(
            QStringLiteral("Compendium database does not exist yet — bootstrap schema will run first."));
    }

    m_preflightReady = blockers.isEmpty();
    if (m_preflightReady) {
        m_preflightMessage = QStringLiteral("Ready to build at %1").arg(m_compendiumDbPath);
        if (!m_preflightWarnings.isEmpty()) {
            m_preflightMessage += QStringLiteral("\nWarnings:\n") + m_preflightWarnings.join(QStringLiteral("\n"));
        }
    } else {
        m_preflightMessage = blockers.join(QStringLiteral("\n"));
    }

    if (previousDb != m_compendiumDbPath) {
        emit pathsChanged();
    }
    emit preflightChanged();
}

void CompendiumBuildController::refreshCredentialStatus() {
    QVariantList rows;
    const auto addGroup = [&](const QString &groupKey, const QString &label, bool runtimeOnly) {
        QVariantMap row;
        row.insert(QStringLiteral("groupKey"), groupKey);
        row.insert(QStringLiteral("label"), label);
        row.insert(QStringLiteral("configured"), credentialGroupConfigured(groupKey));
        row.insert(QStringLiteral("runtimeOnly"), runtimeOnly);
        rows.append(row);
    };

    addGroup(QStringLiteral("igdb"), QStringLiteral("IGDB (recommended)"), false);
    addGroup(QStringLiteral("screenscraper"), QStringLiteral("ScreenScraper (recommended)"), false);
    addGroup(QStringLiteral("retroachievements"), QStringLiteral("RetroAchievements (recommended)"), false);
    addGroup(QStringLiteral("hasheous"), QStringLiteral("Hasheous (optional)"), false);
    addGroup(QStringLiteral("thegamesdb"), QStringLiteral("TheGamesDB (optional)"), false);
    addGroup(QStringLiteral("steamgriddb"), QStringLiteral("SteamGridDB (runtime artwork only)"), true);

    m_credentialStatusModel = rows;
    emit credentialStatusChanged();
}

QVariantList CompendiumBuildController::credentialStatus() const {
    return m_credentialStatusModel;
}

bool CompendiumBuildController::writeEnrichmentCredentialsJson(const QString &path) const {
    QJsonObject payload;
    auto section = [&](const QString &name, const QJsonObject &fields) { payload.insert(name, fields); };

    section(QStringLiteral("igdb"),
        {
            { QStringLiteral("client_id"), CredentialManager::get(QString::fromLatin1(IGDB_CLIENT_ID)) },
            { QStringLiteral("client_secret"), CredentialManager::get(QString::fromLatin1(IGDB_CLIENT_SECRET)) },
        });
    section(QStringLiteral("retroachievements"),
        {
            { QStringLiteral("username"), CredentialManager::get(QString::fromLatin1(RETROACHIEVEMENTS_USERNAME)) },
            { QStringLiteral("api_key"), CredentialManager::get(QString::fromLatin1(RETROACHIEVEMENTS_API_KEY)) },
        });
    section(QStringLiteral("screenscraper"),
        {
            { QStringLiteral("username"), CredentialManager::get(QString::fromLatin1(SCREENSCRAPER_USERNAME)) },
            { QStringLiteral("password"), CredentialManager::get(QString::fromLatin1(SCREENSCRAPER_PASSWORD)) },
            { QStringLiteral("devid"), CredentialManager::get(QString::fromLatin1(SCREENSCRAPER_DEVID)) },
            { QStringLiteral("devpassword"), CredentialManager::get(QString::fromLatin1(SCREENSCRAPER_DEVPASSWORD)) },
        });
    section(QStringLiteral("hasheous"),
        {
            { QStringLiteral("client_api_key"), CredentialManager::get(QString::fromLatin1(HASHEOUS_CLIENT_API_KEY)) },
        });
    section(QStringLiteral("thegamesdb"),
        {
            { QStringLiteral("api_key"), CredentialManager::get(QString::fromLatin1(THEGAMESDB_API_KEY)) },
        });

    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    file.write("\n");
    return true;
}

bool CompendiumBuildController::writeCredentialEnvFile(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    for (const CredField &field : kCredFields) {
        const QString value = CredentialManager::get(QString::fromLatin1(field.key));
        if (!value.isEmpty()) {
            file.write(QString::fromLatin1(field.envVar).toUtf8());
            file.write("='");
            file.write(value.toUtf8().replace('\'', "'\\''"));
            file.write("'\n");
        }
    }
    const QString raUser = CredentialManager::get(QString::fromLatin1(RETROACHIEVEMENTS_USERNAME));
    if (!raUser.isEmpty()) {
        file.write("REMUS_RA_USER='");
        file.write(raUser.toUtf8().replace('\'', "'\\''"));
        file.write("'\n");
    }
    return true;
}

bool CompendiumBuildController::syncEnrichmentCredentials() {
    if (m_compendiumDbPath.isEmpty()) {
        setLastError(QStringLiteral("Compendium output path is unknown."));
        return false;
    }
    const QString credPath
        = QFileInfo(m_compendiumDbPath).absolutePath() + QStringLiteral("/enrichment-credentials.json");
    if (!writeEnrichmentCredentialsJson(credPath)) {
        setLastError(QStringLiteral("Failed to write enrichment-credentials.json"));
        return false;
    }
    return true;
}

QVariantMap CompendiumBuildController::verifyCredentials(const QString &groupKey) {
    QVariantMap result;
    const QString provider = kProviderVerifyNames.value(groupKey);
    if (provider.isEmpty()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"),
            groupKey == QStringLiteral("steamgriddb")
                ? QStringLiteral("SteamGridDB is runtime-only and not probed for compendium builds.")
                : QStringLiteral("No live probe available for this provider."));
        return result;
    }

    refreshPreflight();
    if (m_repoRoot.isEmpty()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("Repository root not found."));
        return result;
    }

    const QString verifyScript = QDir(m_repoRoot).filePath(QStringLiteral("scripts/verify_credentials.sh"));
    if (!QFileInfo::exists(verifyScript)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("verify_credentials.sh not found."));
        return result;
    }

    const QString envPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                .filePath(QStringLiteral("remus_verify_credentials.env"));
    if (!writeCredentialEnvFile(envPath)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("Failed to write temporary credential env file."));
        return result;
    }

    QProcess probe;
    probe.setWorkingDirectory(m_repoRoot);
    probe.start(QStringLiteral("bash"),
        { verifyScript, QStringLiteral("--env-file"), envPath, QStringLiteral("--provider"), provider,
            QStringLiteral("--quiet") });
    if (!probe.waitForFinished(60000)) {
        probe.kill();
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("Credential probe timed out."));
        return result;
    }

    const bool ok = probe.exitCode() == 0;
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("message"),
        ok ? QStringLiteral("Credentials verified successfully.")
           : QStringLiteral("Credential probe failed (exit %1). Check keys and network.").arg(probe.exitCode()));
    return result;
}

QStringList CompendiumBuildController::enrichmentSourceKeys() const {
    return kEnrichmentSourceKeys;
}

void CompendiumBuildController::applyBuildPreset(int preset) {
    m_lastFullBuildOptions = Remus::applyBuildPreset(static_cast<CompendiumBuildPreset>(preset));
}

QVariantMap CompendiumBuildController::currentFullBuildOptions() const {
    const CompendiumFullBuildOptions &o = m_lastFullBuildOptions;
    QVariantMap map;
    map.insert(QStringLiteral("skipDatUpdate"), o.skipDatUpdate);
    map.insert(QStringLiteral("offlineOnly"), o.offlineOnly);
    map.insert(QStringLiteral("strictOffline"), o.strictOffline);
    map.insert(QStringLiteral("forceFullRebuild"), o.forceFullRebuild);
    map.insert(QStringLiteral("onlineEnrichmentAll"), o.onlineEnrichmentAll);
    map.insert(QStringLiteral("recover"), o.recover);
    map.insert(QStringLiteral("forceEnrichment"), o.forceEnrichment);
    map.insert(QStringLiteral("allowUnresolvedConflicts"), o.allowUnresolvedConflicts);
    map.insert(QStringLiteral("skipValidation"), o.skipValidation);
    map.insert(QStringLiteral("pruneAcquisition"), o.pruneAcquisition);
    map.insert(QStringLiteral("thumbnailSnapLossless"), o.thumbnailSnapLossless);
    map.insert(QStringLiteral("skipConsolidate"), o.skipConsolidate);
    map.insert(QStringLiteral("detached"), o.detached);
    map.insert(QStringLiteral("outputDbPath"), o.outputDbPath);
    return map;
}

CompendiumFullBuildOptions CompendiumBuildController::fullBuildOptionsFromMap(const QVariantMap &map) const {
    CompendiumFullBuildOptions options = m_lastFullBuildOptions;
    auto flag = [&](const char *key, bool &target) {
        if (map.contains(QString::fromLatin1(key))) {
            target = map.value(QString::fromLatin1(key)).toBool();
        }
    };
    flag("skipDatUpdate", options.skipDatUpdate);
    flag("offlineOnly", options.offlineOnly);
    flag("strictOffline", options.strictOffline);
    flag("forceFullRebuild", options.forceFullRebuild);
    flag("onlineEnrichmentAll", options.onlineEnrichmentAll);
    flag("recover", options.recover);
    flag("forceEnrichment", options.forceEnrichment);
    flag("allowUnresolvedConflicts", options.allowUnresolvedConflicts);
    flag("skipValidation", options.skipValidation);
    flag("pruneAcquisition", options.pruneAcquisition);
    flag("thumbnailSnapLossless", options.thumbnailSnapLossless);
    flag("skipConsolidate", options.skipConsolidate);
    flag("detached", options.detached);
    if (map.contains(QStringLiteral("outputDbPath"))) {
        const QString custom = map.value(QStringLiteral("outputDbPath")).toString().trimmed();
        if (!custom.isEmpty()) {
            options.outputDbPath = custom;
        }
    }
    normalizeFullBuildOptions(options);
    return options;
}

CompendiumExtendBuildOptions CompendiumBuildController::extendBuildOptionsFromMap(const QVariantMap &map) const {
    CompendiumExtendBuildOptions options;
    options.enrichSources = map.value(QStringLiteral("enrichSources")).toStringList();
    options.forceEnrichment = map.value(QStringLiteral("forceEnrichment")).toBool();
    options.offlineOnly = map.value(QStringLiteral("offlineOnly")).toBool();
    options.onlineEnrichmentAll = map.value(QStringLiteral("onlineEnrichmentAll")).toBool();
    options.consolidateArtwork = map.value(QStringLiteral("consolidateArtwork")).toBool();
    options.detached = map.value(QStringLiteral("detached"), true).toBool();
    const QString custom = map.value(QStringLiteral("outputDbPath")).toString().trimmed();
    if (!custom.isEmpty()) {
        options.outputDbPath = custom;
    }
    normalizeExtendBuildOptions(options);
    return options;
}

void CompendiumBuildController::applyCredentialEnvironment(QProcessEnvironment &env) const {
    for (const CredField &field : kCredFields) {
        const QString value = CredentialManager::get(QString::fromLatin1(field.key));
        if (!value.isEmpty()) {
            env.insert(QString::fromLatin1(field.envVar), value);
        }
    }
    const QString raUser = CredentialManager::get(QString::fromLatin1(RETROACHIEVEMENTS_USERNAME));
    if (!raUser.isEmpty()) {
        env.insert(QStringLiteral("REMUS_RA_USER"), raUser);
    }
    if (!m_compendiumDbPath.isEmpty()) {
        env.insert(QStringLiteral("REMUS_COMPENDIUM_DB"), m_compendiumDbPath);
    }
    const QString cliBinary = resolveCliBinary();
    if (!cliBinary.isEmpty()) {
        env.insert(QStringLiteral("PATH"),
            QFileInfo(cliBinary).absolutePath() + QDir::listSeparator() + env.value(QStringLiteral("PATH")));
    }
}

bool CompendiumBuildController::bootstrapDatabaseIfNeeded() {
    if (m_compendiumDbPath.isEmpty() || QFileInfo::exists(m_compendiumDbPath)) {
        return true;
    }
    const QString setupScript = QDir(m_repoRoot).filePath(QStringLiteral("scripts/setup_compendium_db.sh"));
    if (!QFileInfo::exists(setupScript)) {
        setLastError(QStringLiteral("setup_compendium_db.sh not found."));
        return false;
    }

    QProcess bootstrap;
    bootstrap.setWorkingDirectory(m_repoRoot);
    bootstrap.setProgram(QStringLiteral("bash"));
    bootstrap.setArguments({ setupScript, m_compendiumDbPath });
    bootstrap.start();
    if (!bootstrap.waitForFinished(-1) || bootstrap.exitCode() != 0) {
        setLastError(QStringLiteral("Failed to bootstrap compendium database schema."));
        return false;
    }
    return true;
}

void CompendiumBuildController::setBuilding(bool building, bool monitoringDetached) {
    const bool changed = m_building != building || m_monitoringDetached != monitoringDetached;
    m_building = building;
    m_monitoringDetached = monitoringDetached;
    if (changed) {
        emit buildingChanged();
    }
}

void CompendiumBuildController::setLastError(const QString &message) {
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    emit lastErrorChanged();
}

void CompendiumBuildController::startMonitoring(qint64 pid, bool ownsProcess) {
    m_monitoredPid = pid;
    m_ownsProcess = ownsProcess;
    setBuilding(true, !ownsProcess);
    m_pollTimer.start(2000);
    pollProgress();
}

void CompendiumBuildController::startFullBuild(const QVariantMap &optionsMap) {
    if (m_building) {
        return;
    }
    refreshPreflight();
    if (!m_preflightReady) {
        setLastError(m_preflightMessage);
        return;
    }

    m_lastFullBuildOptions = fullBuildOptionsFromMap(optionsMap);
    if (!m_lastFullBuildOptions.outputDbPath.isEmpty()) {
        m_compendiumDbPath = QFileInfo(m_lastFullBuildOptions.outputDbPath).absoluteFilePath();
        emit pathsChanged();
    }

    if (!syncEnrichmentCredentials()) {
        return;
    }
    if (!bootstrapDatabaseIfNeeded()) {
        return;
    }

    const QString script = m_lastFullBuildOptions.detached ? resolveDetachedScript() : resolveBuildScript();
    if (script.isEmpty()) {
        setLastError(QStringLiteral("Build script not found."));
        return;
    }

    const QStringList flagArgs = fullBuildFlagArgs(m_compendiumDbPath, m_lastFullBuildOptions);

    setLastError(QString());
    m_hadMergeConflicts = false;
    m_buildSummary.clear();
    emit buildSummaryChanged();
    m_progressPercent = 0;
    m_progressValue = 0;
    m_progressTotal = 0;
    m_progressMessage = QStringLiteral("Starting compendium build…");
    m_buildPhase = QStringLiteral("starting");
    emit progressChanged();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    applyCredentialEnvironment(env);
    env.insert(QStringLiteral("REMUS_COMPENDIUM_BUILD_LOG"), m_buildLogPath);

    if (m_lastFullBuildOptions.detached) {
        const QString envPath = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                    .filePath(QStringLiteral("remus_compendium_build.env"));
        writeCredentialEnvFile(envPath);

        QStringList args;
        args << QStringLiteral("-c");
        QString cmd = QStringLiteral("set -a; source '%1'; set +a; export REMUS_COMPENDIUM_BUILD_LOG='%2'; "
                                     "export REMUS_COMPENDIUM_DB='%3'; exec '%4'")
                          .arg(envPath, m_buildLogPath, m_compendiumDbPath, script);
        for (const QString &flag : flagArgs) {
            QString escaped = flag;
            escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
            cmd += QStringLiteral(" '") + escaped + QLatin1Char('\'');
        }
        args << cmd;

        qint64 pid = 0;
        if (!QProcess::startDetached(QStringLiteral("bash"), args, m_repoRoot, &pid)) {
            setLastError(QStringLiteral("Failed to start detached compendium build."));
            return;
        }
        Q_UNUSED(pid);
        QTimer::singleShot(500, this, [this]() {
            const qint64 lockPid = detachedBuildPid();
            if (lockPid > 0 && isProcessAlive(lockPid)) {
                startMonitoring(lockPid, false);
            } else {
                setLastError(QStringLiteral("Detached build failed to start — see log for details."));
                readLogTail();
                setBuilding(false);
            }
        });
        return;
    }

    m_process = std::make_unique<QProcess>(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(m_repoRoot);
    m_process->setProgram(QStringLiteral("bash"));
    QStringList args;
    args << script;
    args << flagArgs;
    m_process->setArguments(args);

    connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &CompendiumBuildController::onProcessFinished);
    connect(m_process.get(), &QProcess::errorOccurred, this, &CompendiumBuildController::onProcessError);

    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        setLastError(QStringLiteral("Failed to start build script: %1").arg(m_process->errorString()));
        m_process.reset();
        return;
    }

    startMonitoring(m_process->processId(), true);
}

void CompendiumBuildController::startExtendBuild(const QVariantMap &optionsMap) {
    if (m_building) {
        return;
    }
    refreshPreflight();
    if (!m_preflightReady) {
        setLastError(m_preflightMessage);
        return;
    }

    const CompendiumExtendBuildOptions options = extendBuildOptionsFromMap(optionsMap);
    if (options.enrichSources.isEmpty()) {
        setLastError(QStringLiteral("Select at least one enrichment source."));
        return;
    }

    if (!options.outputDbPath.isEmpty()) {
        m_compendiumDbPath = QFileInfo(options.outputDbPath).absoluteFilePath();
        emit pathsChanged();
    }
    if (!QFileInfo::exists(m_compendiumDbPath)) {
        setLastError(QStringLiteral("Compendium database not found at %1").arg(m_compendiumDbPath));
        return;
    }
    if (!syncEnrichmentCredentials()) {
        return;
    }

    const QString cliBinary = resolveCliBinary();
    if (cliBinary.isEmpty()) {
        setLastError(QStringLiteral("remus-cli not found."));
        return;
    }

    const QString jobScript = QDir(m_repoRoot).filePath(QStringLiteral("scripts/run_compendium_job.sh"));

    if (options.consolidateArtwork) {
        QStringList consolidateArgs;
        consolidateArgs << jobScript << QStringLiteral("--no-lock") << QStringLiteral("--db") << m_compendiumDbPath
                        << QStringLiteral("--") << cliBinary << QStringLiteral("--consolidate-thumbnails")
                        << QStringLiteral("--compendium-output") << m_compendiumDbPath
                        << QStringLiteral("--ingest-remote-artwork");
        QProcess consolidate;
        consolidate.setWorkingDirectory(m_repoRoot);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        applyCredentialEnvironment(env);
        consolidate.setProcessEnvironment(env);
        consolidate.start(QStringLiteral("bash"), consolidateArgs);
        if (!consolidate.waitForFinished(-1) || consolidate.exitCode() != 0) {
            setLastError(QStringLiteral("Artwork consolidate failed before extend enrich."));
            return;
        }
    }

    const QStringList cliArgs = extendBuildCommandArgs(m_repoRoot, cliBinary, m_compendiumDbPath, options);
    QStringList args;
    args << jobScript << QStringLiteral("--no-lock") << QStringLiteral("--db") << m_compendiumDbPath
         << QStringLiteral("--");
    args << cliArgs;

    m_process = std::make_unique<QProcess>(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    applyCredentialEnvironment(env);
    env.insert(QStringLiteral("REMUS_COMPENDIUM_BUILD_LOG"), m_buildLogPath);
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(m_repoRoot);
    m_process->setProgram(QStringLiteral("bash"));
    m_process->setArguments(args);

    connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        &CompendiumBuildController::onProcessFinished);
    connect(m_process.get(), &QProcess::errorOccurred, this, &CompendiumBuildController::onProcessError);

    setLastError(QString());
    m_hadMergeConflicts = false;
    m_progressMessage = QStringLiteral("Starting extend build…");
    m_buildPhase = QStringLiteral("enriching");
    emit progressChanged();

    m_process->start();
    if (!m_process->waitForStarted(5000)) {
        setLastError(QStringLiteral("Failed to start extend build: %1").arg(m_process->errorString()));
        m_process.reset();
        return;
    }

    startMonitoring(m_process->processId(), true);
}

void CompendiumBuildController::reattachToRunningBuild() {
    refreshPreflight();
    const qint64 pid = detachedBuildPid();
    if (pid > 0 && isProcessAlive(pid)) {
        m_process.reset();
        m_progressMessage = QStringLiteral("Reattached to running build (pid %1)").arg(pid);
        emit progressChanged();
        startMonitoring(pid, false);
    }
}

void CompendiumBuildController::cancelBuild() {
    if (m_ownsProcess && m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        return;
    }
    if (m_monitoringDetached && m_monitoredPid > 0) {
        QProcess::execute(
            QStringLiteral("bash"), { QStringLiteral("-lc"), QStringLiteral("kill %1").arg(m_monitoredPid) });
    }
}

void CompendiumBuildController::openLogFile() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_buildLogPath));
}

void CompendiumBuildController::openOutputFolder() {
    if (m_compendiumDbPath.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_compendiumDbPath).absolutePath()));
}

void CompendiumBuildController::updateProgressFromJson(const QJsonObject &obj) {
    const QString status = obj.value(QStringLiteral("status")).toString();
    if (!status.isEmpty() || obj.contains(QStringLiteral("build_phase"))) {
        m_buildPhase = obj.value(QStringLiteral("build_phase")).toString(status);
    }

    if (obj.contains(QStringLiteral("overall_pct"))) {
        m_progressPercent = obj.value(QStringLiteral("overall_pct")).toInt(m_progressPercent);
        m_progressValue = m_progressPercent;
        m_progressTotal = 100;
        m_pollTimer.setInterval(2000);
    } else if (obj.contains(QStringLiteral("current")) && obj.contains(QStringLiteral("total"))) {
        m_progressValue = obj.value(QStringLiteral("current")).toInt();
        m_progressTotal = std::max(1, obj.value(QStringLiteral("total")).toInt());
        m_progressPercent = (m_progressValue * 100) / m_progressTotal;
        m_pollTimer.setInterval(2000);
    } else if (m_buildPhase == QStringLiteral("dat_update")) {
        m_pollTimer.setInterval(10000);
    }

    QStringList parts;
    if (!obj.value(QStringLiteral("description")).toString().isEmpty()) {
        parts.append(obj.value(QStringLiteral("description")).toString());
    }
    if (!obj.value(QStringLiteral("enrichment_pass_name")).toString().isEmpty()) {
        parts.append(QStringLiteral("Pass %1/%2: %3")
                .arg(obj.value(QStringLiteral("enrichment_pass_current")).toInt())
                .arg(obj.value(QStringLiteral("enrichment_pass_total")).toInt())
                .arg(obj.value(QStringLiteral("enrichment_pass_name")).toString()));
    } else if (!obj.value(QStringLiteral("current_source")).toString().isEmpty()) {
        parts.append(obj.value(QStringLiteral("current_source")).toString());
    }
    if (!obj.value(QStringLiteral("enrichment_detail")).toString().isEmpty()) {
        parts.append(obj.value(QStringLiteral("enrichment_detail")).toString());
    } else if (!status.isEmpty()) {
        parts.append(status);
    }
    if (!parts.isEmpty()) {
        m_progressMessage = parts.join(QStringLiteral(" — "));
    }
    emit progressChanged();
}

void CompendiumBuildController::readLogTail() {
    QFile log(m_buildLogPath);
    if (!log.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QByteArray data = log.readAll();
    log.close();
    const QString text = QString::fromUtf8(data);
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QString tail = lines.mid(qMax(0, lines.size() - 20)).join(QLatin1Char('\n'));
    if (tail != m_logTail) {
        m_logTail = tail;
        emit logTailChanged();
    }
}

void CompendiumBuildController::pollProgress() {
    if (m_monitoringDetached && m_monitoredPid > 0 && !isProcessAlive(m_monitoredPid)) {
        finishBuild(0, QProcess::NormalExit);
        return;
    }

    readLogTail();
    QFile file(progressFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        updateProgressFromJson(doc.object());
    }
}

qint64 CompendiumBuildController::countSignaturesInDb() const {
    if (m_compendiumDbPath.isEmpty() || !QFileInfo::exists(m_compendiumDbPath)) {
        return -1;
    }
    const QString connectionName = QStringLiteral("gui_compendium_sig_count");
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(m_compendiumDbPath);
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

void CompendiumBuildController::updateBuildSummary() {
    m_buildSummary.clear();
    const qint64 signatures = countSignaturesInDb();
    if (signatures >= 0) {
        m_buildSummary.insert(QStringLiteral("signatureCount"), signatures);
    }
    m_buildSummary.insert(QStringLiteral("dbPath"), m_compendiumDbPath);
    m_buildSummary.insert(QStringLiteral("coveragePath"),
        QFileInfo(m_compendiumDbPath).absolutePath() + QStringLiteral("/remus_compendium.coverage.tsv"));
    m_buildSummary.insert(QStringLiteral("logPath"), m_buildLogPath);
    m_buildSummary.insert(QStringLiteral("hadMergeConflicts"), m_hadMergeConflicts);
    emit buildSummaryChanged();
}

void CompendiumBuildController::finishBuild(int exitCode, QProcess::ExitStatus status) {
    m_pollTimer.stop();
    pollProgress();
    readLogTail();
    setBuilding(false);

    const bool success = status == QProcess::NormalExit && (exitCode == 0 || exitCode == 2);
    m_hadMergeConflicts = exitCode == 2;
    if (!success) {
        setLastError(QStringLiteral("Compendium build failed (exit %1). See log for details.").arg(exitCode));
    } else if (exitCode == 2) {
        m_progressMessage
            = QStringLiteral("Build finished with unresolved merge conflicts (exit 2). Review log and DB.");
        emit progressChanged();
    } else {
        m_progressPercent = 100;
        m_progressValue = 100;
        m_progressTotal = 100;
        m_progressMessage = QStringLiteral("Compendium build complete.");
        emit progressChanged();
        if (m_appController) {
            m_appController->reloadCompendiumOrchestrator();
        }
    }

    updateBuildSummary();
    if (m_ownsProcess) {
        m_process.reset();
    }
    m_monitoredPid = 0;
    m_ownsProcess = false;
    emit buildFinished(success, exitCode);
}

void CompendiumBuildController::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    finishBuild(exitCode, status);
}

void CompendiumBuildController::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        setLastError(QStringLiteral("Failed to start compendium build process."));
        setBuilding(false);
        m_pollTimer.stop();
    }
}

} // namespace Remus
