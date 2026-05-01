#pragma once

#include <QObject>
#include <QVariantMap>

namespace Remus {

class AppController;
class PatchService;
enum class PatchFormat;

class PatchController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool patching READ isPatching NOTIFY patchingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentOperation READ currentOperation NOTIFY currentOperationChanged)
    Q_PROPERTY(QVariantMap toolStatus READ toolStatus NOTIFY toolStatusChanged)

public:
    explicit PatchController(AppController *appController, QObject *parent = nullptr);
    ~PatchController() override;

    bool isPatching() const { return m_patching; }
    int progress() const { return m_progress; }
    QString currentOperation() const { return m_currentOperation; }
    QVariantMap toolStatus() const { return m_toolStatus; }

    Q_INVOKABLE bool applyPatch(const QString &basePath, const QString &patchPath, const QString &outputPath = QString());
    Q_INVOKABLE bool createPatch(const QString &originalPath, const QString &modifiedPath, const QString &patchPath, const QString &format = QStringLiteral("bps"));
    Q_INVOKABLE void checkTools();

signals:
    void patchingChanged();
    void progressChanged();
    void currentOperationChanged();
    void toolStatusChanged();
    void libraryChanged();

private:
    void updateToolStatus();
    void applyToolPaths();
    PatchFormat stringToFormat(const QString &format) const;

    AppController *m_appController;
    PatchService *m_patchService = nullptr;
    bool m_patching = false;
    int m_progress = 0;
    QString m_currentOperation;
    QVariantMap m_toolStatus;
};

} // namespace Remus