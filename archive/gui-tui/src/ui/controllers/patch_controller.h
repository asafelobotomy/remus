#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include "../../core/database.h"
#include "../../core/patch_engine.h"

namespace Remus {

class PatchService;

/**
 * @brief Controller for ROM patching operations
 * 
 * Handles patch detection, application, and creation.
 * Delegates to PatchService for business logic.
 * Exposed to QML as a context property.
 */
class PatchController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool patching READ isPatching NOTIFY patchingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentOperation READ currentOperation NOTIFY currentOperationChanged)
    Q_PROPERTY(QVariantMap toolStatus READ toolStatus NOTIFY toolStatusChanged)
    
public:
    explicit PatchController(Database *db, QObject *parent = nullptr);
    ~PatchController() override;
    
    bool isPatching() const { return m_patching; }
    int progress() const { return m_progress; }
    QString currentOperation() const { return m_currentOperation; }
    QVariantMap toolStatus() const { return m_toolStatus; }
    
    // Patch detection
    Q_INVOKABLE QVariantMap detectPatchFormat(const QString &patchPath);
    Q_INVOKABLE bool isFormatSupported(const QString &format);
    Q_INVOKABLE QStringList getSupportedFormats();
    
    // Patch application
    Q_INVOKABLE bool applyPatch(const QString &basePath, const QString &patchPath,
                                const QString &outputPath = QString());
    Q_INVOKABLE void cancelPatching();
    
    // Patch creation
    Q_INVOKABLE bool createPatch(const QString &originalPath, const QString &modifiedPath,
                                  const QString &patchPath, const QString &format = "bps");
    
    // Tool configuration
    Q_INVOKABLE void checkTools();
    Q_INVOKABLE void setFlipsPath(const QString &path);
    Q_INVOKABLE void setXdeltaPath(const QString &path);
    Q_INVOKABLE QString getFlipsPath();
    Q_INVOKABLE QString getXdeltaPath();
    
    // Utility
    Q_INVOKABLE QString generateOutputPath(const QString &basePath, const QString &patchPath);
    
signals:
    void patchingChanged();
    void progressChanged();
    void currentOperationChanged();
    void toolStatusChanged();
    
    void patchStarted();
    void patchCompleted(const QString &outputPath);
    void patchError(const QString &error);
    
    void createPatchStarted();
    void createPatchCompleted(const QString &patchPath);
    void createPatchError(const QString &error);
    
private:
    Database *m_db;
    PatchService *m_patchService = nullptr;
    
    bool m_patching = false;
    int m_progress = 0;
    QString m_currentOperation;
    QVariantMap m_toolStatus;
    bool m_cancelRequested = false;
    
    void updateToolStatus();
    PatchFormat stringToFormat(const QString &format);
};

} // namespace Remus
