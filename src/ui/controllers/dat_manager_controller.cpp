#include "dat_manager_controller.h"
#include <QFileInfo>
#include <QVariantMap>
#include <QDebug>

namespace Remus {

DatManagerController::DatManagerController(LocalDatabaseProvider *provider, Database *database, QObject *parent)
    : QObject(parent)
    , m_provider(provider)
    , m_database(database)
    , m_verificationEngine(database ? new VerificationEngine(database, this) : nullptr)
{
    // Connect to provider signals
    if (m_provider) {
        connect(m_provider, &LocalDatabaseProvider::databaseLoaded,
                this, &DatManagerController::onDatabaseLoaded);
        connect(m_provider, &LocalDatabaseProvider::updateAvailable,
                this, &DatManagerController::updateAvailable);
    }
}

QVariantList DatManagerController::loadedDats() const
{
    QVariantList result;
    
    if (!m_provider) {
        return result;
    }
    
    QList<DatMetadata> dats = m_provider->getLoadedDats();
    
    for (const DatMetadata &dat : dats) {
        QVariantMap datMap;
        datMap["name"] = dat.name;
        datMap["version"] = dat.version;
        datMap["description"] = dat.description;
        datMap["filePath"] = dat.filePath;
        datMap["entryCount"] = dat.entryCount;
        datMap["loadedAt"] = dat.loadedAt.toString("yyyy-MM-dd hh:mm:ss");
        result.append(datMap);
    }
    
    return result;
}

QVariantList DatManagerController::loadedPatchDats() const
{
    QVariantList result;

    if (!m_verificationEngine) {
        return result;
    }

    const auto patchDats = m_verificationEngine->getImportedPatchDats();
    for (auto it = patchDats.cbegin(); it != patchDats.cend(); ++it) {
        QVariantMap datMap;
        datMap["system"] = it.key();
        datMap["name"] = it.value().name;
        datMap["version"] = it.value().version;
        datMap["description"] = it.value().description;
        datMap["source"] = it.value().category;
        result.append(datMap);
    }

    return result;
}

bool DatManagerController::loadDat(const QString &filePath)
{
    if (!m_provider) {
        emit error("LocalDatabaseProvider not available");
        return false;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit error("DAT file not found: " + filePath);
        return false;
    }
    
    int loaded = m_provider->loadDatabase(filePath);
    
    if (loaded > 0) {
        emit datsChanged();
        return true;
    } else {
        emit error("Failed to load DAT file: " + filePath);
        return false;
    }
}

bool DatManagerController::checkForUpdate(const QString &filePath)
{
    if (!m_provider) {
        return false;
    }
    
    return m_provider->isDatNewer(filePath);
}

bool DatManagerController::reloadDat(const QString &filePath)
{
    if (!m_provider) {
        emit error("LocalDatabaseProvider not available");
        return false;
    }
    
    int loaded = m_provider->reloadDatabase(filePath);
    
    if (loaded > 0) {
        emit datsChanged();
        return true;
    } else if (loaded == -1) {
        emit error("DAT file is not newer than current version");
        return false;
    } else {
        emit error("Failed to reload DAT file");
        return false;
    }
}

bool DatManagerController::loadPatchDat(const QString &filePath, const QString &systemName)
{
    if (!m_verificationEngine) {
        emit error("Verification engine not available");
        return false;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit error("Patch DAT file not found: " + filePath);
        return false;
    }

    const QString resolvedSystemName = systemName.isEmpty() ? fileInfo.completeBaseName() : systemName;
    const int loaded = m_verificationEngine->importPatchDat(filePath, resolvedSystemName);
    if (loaded <= 0) {
        emit error("Failed to load patch DAT file: " + filePath);
        return false;
    }

    emit patchDatLoaded(resolvedSystemName, loaded);
    emit patchDatsChanged();
    return true;
}

bool DatManagerController::removePatchDat(const QString &systemName)
{
    if (!m_verificationEngine) {
        emit error("Verification engine not available");
        return false;
    }

    if (!m_verificationEngine->removePatchDat(systemName)) {
        emit error("Failed to remove patch DAT: " + systemName);
        return false;
    }

    emit patchDatsChanged();
    return true;
}

bool DatManagerController::hasPatchDat(const QString &systemName) const
{
    return m_verificationEngine && m_verificationEngine->hasPatchDat(systemName);
}

QVariantMap DatManagerController::getUpdateInfo(const QString &filePath) const
{
    QVariantMap result;
    
    if (!m_provider) {
        result["error"] = "Provider not available";
        return result;
    }
    
    QFileInfo fileInfo(filePath);
    QString systemName = fileInfo.baseName();
    
    // Get current version
    QList<DatMetadata> dats = m_provider->getLoadedDats();
    QString currentVersion = "";
    for (const DatMetadata &dat : dats) {
        if (dat.name == systemName || dat.filePath == filePath) {
            currentVersion = dat.version;
            break;
        }
    }
    
    // Get new version from file
    QMap<QString, QString> header = ClrMameProParser::parseHeader(filePath);
    QString newVersion = header.value("version", "unknown");
    
    result["systemName"] = systemName;
    result["currentVersion"] = currentVersion;
    result["newVersion"] = newVersion;
    result["isNewer"] = newVersion > currentVersion;
    result["isLoaded"] = !currentVersion.isEmpty();
    
    return result;
}

void DatManagerController::onDatabaseLoaded(const QString &systemName, int entryCount)
{
    qDebug() << "DatManagerController: Database loaded:" << systemName << "with" << entryCount << "entries";
    emit datLoaded(systemName, entryCount);
    emit datsChanged();
}

} // namespace Remus
