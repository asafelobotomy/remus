#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace Remus {

class AppController;
class HashController;
class MatchController;
class ArtworkController;
class ConversionController;
class OrganizeController;
class ExportController;

class WorkflowController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int identityCount READ identityCount NOTIFY stageCountsChanged)
    Q_PROPERTY(int enrichCount READ enrichCount NOTIFY stageCountsChanged)
    Q_PROPERTY(int doneCount READ doneCount NOTIFY stageCountsChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int queueStage READ queueStage WRITE setQueueStage NOTIFY queueStageChanged)
    Q_PROPERTY(QVariantList queueFiles READ queueFiles NOTIFY queueFilesChanged)
    Q_PROPERTY(int activeStage READ activeStage NOTIFY activeStageChanged)

public:
    // Stage values exposed as integers in QML (0=All, 1=Identity, 2=Enrich, 3=Done)
    // NOTE: These Stage enum values are pipeline queue buckets (0–3), distinct from
    //       openStage in QML (1–6 for the six workflow StageCards). Do not confuse them.
    enum Stage { AllFiles = 0, Identity = 1, Enrich = 2, Done = 3 };
    Q_ENUM(Stage)

    explicit WorkflowController(AppController *app, HashController *hash, MatchController *match,
        ArtworkController *artwork, ConversionController *conversion, OrganizeController *organize,
        ExportController *export_ctl, QObject *parent = nullptr);

    int identityCount() const {
        return m_identityCount;
    }
    int enrichCount() const {
        return m_enrichCount;
    }
    int doneCount() const {
        return m_doneCount;
    }
    bool isRunning() const {
        return m_running;
    }
    int queueStage() const {
        return m_queueStage;
    }
    QVariantList queueFiles() const {
        return m_queueFiles;
    }
    int activeStage() const {
        return m_activeStage;
    }

    void setQueueStage(int stage);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void runAll(const QString &scanDir, const QString &destDir, const QString &namingTemplate = { });
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void hashAndMatchAll();
    Q_INVOKABLE void hashAndMatchSelected();
    Q_INVOKABLE bool artworkExistsForFile(int fileId) const;

signals:
    void stageCountsChanged();
    void runningChanged();
    void queueStageChanged();
    void queueFilesChanged();
    void activeStageChanged();

private:
    void refreshCounts();
    void refreshQueueFiles();
    void advanceRunAll();
    void cancelRunAll();
    void setActiveStage(int stage);
    QString artworkDirPath() const;

    AppController *m_appController;
    HashController *m_hashController;
    MatchController *m_matchController;
    ArtworkController *m_artworkController;
    ConversionController *m_conversionController;
    OrganizeController *m_organizeController;
    ExportController *m_exportController;

    QTimer *m_refreshTimer = nullptr;

    int m_identityCount = 0;
    int m_enrichCount = 0;
    int m_doneCount = 0;
    bool m_running = false;
    int m_queueStage = 0;
    QVariantList m_queueFiles;
    int m_runStep = 0;
    int m_activeStage = 0;
    QString m_scanDir;
    QString m_destDir;
    QString m_namingTemplate;
};

} // namespace Remus
