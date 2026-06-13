#include "mod_controller.h"

#include <QCoreApplication>
#include <QPointer>
#include <QSettings>
#include <QThread>
#include <QUrl>

#include "app_controller.h"
#include "settings_controller.h"
#include "../../core/constants/constants.h"
#include "../models/mod_list_model.h"

namespace Remus {

ModController::ModController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_workflow(std::make_unique<ModWorkflowService>(*appController->database(), m_patchService)) {
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    m_catalogUrl = settings.value(QString::fromLatin1(GuiSettings::MOD_CATALOG_URL)).toString();
    connect(m_appController, &AppController::selectedFileChanged, this, &ModController::loadForSelectedFile);
}

ModController::~ModController() {
    if (m_catalogLoadThread != nullptr) {
        if (m_catalogLoadThread->isRunning()) {
            m_catalogLoadThread->wait();
        }
        if (QCoreApplication::instance() != nullptr) {
            QCoreApplication::processEvents();
        }
        delete m_catalogLoadThread;
        m_catalogLoadThread = nullptr;
    }
}

void ModController::loadCatalog(const QString &url, bool forceRefresh) {
    const QString resolvedUrl = url.trimmed().isEmpty() ? m_catalogUrl : url.trimmed();
    if (resolvedUrl.isEmpty()) {
        setLastError(QStringLiteral("Provide a mod catalog URL or local JSON path."));
        return;
    }

    if (m_catalogLoadThread != nullptr && m_catalogLoadThread->isRunning()) {
        m_catalogLoadThread->wait();
    }

    const bool isRemote = resolvedUrl.startsWith(QStringLiteral("https://"));

    if (!isRemote) {
        m_loadingCatalog = true;
        emit loadingCatalogChanged();

        const bool loaded = m_provider.loadFromFile(resolvedUrl);

        m_loadingCatalog = false;
        emit loadingCatalogChanged();
        if (!loaded) {
            setLastError(m_provider.lastError());
            return;
        }
        setCatalogUrl(resolvedUrl);
        m_workflow->setCatalogIsRemote(false);
        loadForSelectedFile();
        return;
    }

    m_loadingCatalog = true;
    emit loadingCatalogChanged();

    QPointer<ModController> self(this);
    m_catalogLoadThread = QThread::create([self, resolvedUrl, forceRefresh]() {
        if (self.isNull()) {
            return;
        }

        ModCatalogProvider provider;
        const bool loaded = provider.loadFromUrl(QUrl(resolvedUrl), forceRefresh);

        QMetaObject::invokeMethod(
            self.data(),
            [self, loaded, resolvedUrl, provider = std::move(provider)]() mutable {
                if (self.isNull()) {
                    return;
                }

                self->m_loadingCatalog = false;
                emit self->loadingCatalogChanged();
                if (!loaded) {
                    self->setLastError(provider.lastError());
                    return;
                }

                self->m_provider = std::move(provider);
                self->setCatalogUrl(resolvedUrl);
                self->m_workflow->setCatalogIsRemote(true);
                self->loadForSelectedFile();
            },
            Qt::QueuedConnection);
    });
    connect(m_catalogLoadThread, &QThread::finished, this, [this]() {
        if (m_catalogLoadThread != nullptr) {
            m_catalogLoadThread->deleteLater();
            m_catalogLoadThread = nullptr;
        }
    });
    m_catalogLoadThread->start();
}

void ModController::loadForSelectedFile() {
    if (m_loadingCatalog)
        return; // provider data is being written on the worker thread
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        if (m_model != nullptr) {
            m_model->clear();
        }
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        if (m_model != nullptr) {
            m_model->clear();
        }
        m_installedMods.clear();
        emit installedModsChanged();
        return;
    }

    const FileRecord file = m_appController->database()->getFileById(fileId);
    QList<ModEntry> mods = m_provider.findModsForRom(file.crc32, file.md5, file.sha1);
    if (mods.isEmpty()) {
        mods = m_provider.findModsBySystem(m_appController->systemName(file.systemId));
    }

    const QList<ModInstallationRecord> installed = m_appController->database()->getModInstallations(fileId);
    QList<ModListEntry> entries;
    for (const ModEntry &mod : mods) {
        ModListEntry entry;
        entry.id = mod.id;
        entry.title = mod.title;
        entry.author = mod.author;
        entry.version = mod.version;
        entry.type = mod.type;
        entry.format = mod.format;
        entry.system = mod.system;
        entry.description = mod.description;
        entry.rating = mod.rating;
        entry.downloads = mod.downloads;
        for (const ModInstallationRecord &installation : installed) {
            if (installation.catalogModId == mod.id) {
                entry.installed = true;
                entry.installationId = installation.id;
                break;
            }
        }
        entries.append(entry);
    }

    if (m_model != nullptr) {
        m_model->setEntries(entries);
    }

    refreshInstalledMods();
}

bool ModController::installMod(const QString &modId, const QString &outputDir) {
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastError(QStringLiteral("Open a library before installing mods."));
        return false;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastError(QStringLiteral("Select a base ROM first."));
        return false;
    }

    const auto mod = m_provider.getModById(modId);
    if (!mod.has_value()) {
        setLastError(QStringLiteral("Selected mod no longer exists in the loaded catalog."));
        return false;
    }

    applyToolPaths();

    m_installing = true;
    emit installingChanged();
    const ModInstallResult result
        = m_workflow->install(m_appController->database()->getFileById(fileId), mod.value(), outputDir, nullptr);
    m_installing = false;
    emit installingChanged();

    if (!result.success) {
        setLastError(result.error);
        return false;
    }

    loadForSelectedFile();
    emit libraryChanged();
    return true;
}

bool ModController::uninstallInstallation(int installationId) {
    if (!m_workflow->uninstall(installationId)) {
        setLastError(QStringLiteral("Failed to uninstall the selected mod."));
        return false;
    }

    loadForSelectedFile();
    emit libraryChanged();
    return true;
}

void ModController::setCatalogUrl(const QString &value) {
    if (m_catalogUrl == value) {
        return;
    }

    m_catalogUrl = value;
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    settings.setValue(QString::fromLatin1(GuiSettings::MOD_CATALOG_URL), value);
    emit catalogUrlChanged();
}

void ModController::applyToolPaths() {
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    const QString flipsPath = settings.value(QString::fromLatin1(GuiSettings::FLIPS_PATH)).toString().trimmed();
    const QString xdeltaPath = settings.value(QString::fromLatin1(GuiSettings::XDELTA3_PATH)).toString().trimmed();
    const QString ppfPath = settings.value(QString::fromLatin1(GuiSettings::PPF_PATH)).toString().trimmed();
    if (!flipsPath.isEmpty()) {
        m_patchService.setFlipsPath(flipsPath);
    }
    if (!xdeltaPath.isEmpty()) {
        m_patchService.setXdelta3Path(xdeltaPath);
    }
    if (!ppfPath.isEmpty()) {
        m_patchService.setPpfPath(ppfPath);
    }
}

void ModController::refreshInstalledMods() {
    QVariantList installedMods;
    if (m_appController != nullptr && m_appController->isLibraryOpen() && m_appController->selectedFileId() > 0) {
        const QList<ModInstallationRecord> records
            = m_appController->database()->getModInstallations(m_appController->selectedFileId());
        for (const ModInstallationRecord &record : records) {
            QVariantMap item;
            item.insert(QStringLiteral("installationId"), record.id);
            item.insert(QStringLiteral("modId"), record.catalogModId);
            item.insert(QStringLiteral("title"), record.modTitle);
            item.insert(QStringLiteral("author"), record.modAuthor);
            item.insert(QStringLiteral("version"), record.modVersion);
            item.insert(QStringLiteral("type"), record.modType);
            installedMods.append(item);
        }
    }

    m_installedMods = installedMods;
    emit installedModsChanged();
}

void ModController::setLastError(const QString &message) {
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

} // namespace Remus