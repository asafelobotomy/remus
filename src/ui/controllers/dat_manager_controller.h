#ifndef DAT_MANAGER_CONTROLLER_H
#define DAT_MANAGER_CONTROLLER_H

#include <QObject>
#include <QVariantList>
#include "../../metadata/local_database_provider.h"

namespace Remus {

/**
 * @brief Controller for managing local DAT databases in the UI
 */
class DatManagerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList loadedDats READ loadedDats NOTIFY datsChanged)
    
public:
    explicit DatManagerController(LocalDatabaseProvider *provider, QObject *parent = nullptr);
    
    /**
     * @brief Get list of loaded DAT files as QVariantMap list
     * @return List of DAT metadata (name, version, entryCount, filePath)
     */
    QVariantList loadedDats() const;
    
    /**
     * @brief Load a DAT file from path
     * @param filePath Path to .dat file
     * @return True if loaded successfully
     */
    Q_INVOKABLE bool loadDat(const QString &filePath);
    
    /**
     * @brief Check if a DAT file is newer than the currently loaded version
     * @param filePath Path to .dat file
     * @return True if newer version available
     */
    Q_INVOKABLE bool checkForUpdate(const QString &filePath);
    
    /**
     * @brief Reload a DAT file with newer version
     * @ param filePath Path to .dat file
     * @return True if reloaded successfully
     */
    Q_INVOKABLE bool reloadDat(const QString &filePath);
    
    /**
     * @brief Get update information for a DAT
     * @param filePath Path to potential newer .dat file
     * @return JSON object with currentVersion, newVersion, isNewer
     */
    Q_INVOKABLE QVariantMap getUpdateInfo(const QString &filePath) const;
    
signals:
    void datsChanged();
    void datLoaded(const QString &systemName, int entryCount);
    void updateAvailable(const QString &systemName, const QString &currentVersion, const QString &newVersion);
    void error(const QString &message);
    
private slots:
    void onDatabaseLoaded(const QString &systemName, int entryCount);
    
private:
    LocalDatabaseProvider *m_provider;
};

} // namespace Remus

#endif // DAT_MANAGER_CONTROLLER_H
