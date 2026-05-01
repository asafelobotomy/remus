#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace Remus {

class AppController;
class HashController;
class MatchController;
class ArtworkController;
class OrganizeController;

class WorkflowController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int identityCount  READ identityCount  NOTIFY stageCountsChanged)
    Q_PROPERTY(int enrichCount    READ enrichCount    NOTIFY stageCountsChanged)
    Q_PROPERTY(int doneCount      READ doneCount      NOTIFY stageCountsChanged)
    Q_PROPERTY(bool running       READ isRunning      NOTIFY runningChanged)
    Q_PROPERTY(QString hint       READ hint           NOTIFY hintChanged)
    Q_PROPERTY(int queueStage     READ queueStage     WRITE setQueueStage NOTIFY queueStageChanged)
    Q_PROPERTY(QVariantList queueFiles READ queueFiles NOTIFY queueFilesChanged)

public:
    // Stage values exposed as integers in QML (0=All, 1=Identity, 2=Enrich, 3=Done)
    enum Stage { AllFiles = 0, Identity = 1, Enrich = 2, Done = 3 };
    Q_ENUM(Stage)

    explicit WorkflowController(AppController   *app,
                                HashController  *hash,
                                MatchController *match,
                                ArtworkController   *artwork,
                                OrganizeController  *organize,
                                QObject *parent = nullptr);

    int          identityCount() const { return m_identityCount; }
    int          enrichCount()   const { return m_enrichCount;   }
    int          doneCount()     const { return m_doneCount;     }
    bool         isRunning()     const { return m_running;       }
    QString      hint()          const { return m_hint;          }
    int          queueStage()    const { return m_queueStage;    }
    QVariantList queueFiles()    const { return m_queueFiles;    }

    void setQueueStage(int stage);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void runAll();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool artworkExistsForFile(int fileId) const;

signals:
    void stageCountsChanged();
    void runningChanged();
    void hintChanged();
    void queueStageChanged();
    void queueFilesChanged();

private slots:
    void onSelectedFileChanged();

private:
    void refreshCounts();
    void refreshQueueFiles();
    void refreshHint();
    void advanceRunAll();
    void cancelRunAll();
    QString artworkDirPath() const;

    AppController      *m_appController;
    HashController     *m_hashController;
    MatchController    *m_matchController;
    ArtworkController  *m_artworkController;
    OrganizeController *m_organizeController;

    int          m_identityCount = 0;
    int          m_enrichCount   = 0;
    int          m_doneCount     = 0;
    bool         m_running       = false;
    QString      m_hint;
    int          m_queueStage    = 0;
    QVariantList m_queueFiles;
    int          m_runStep       = 0;
};

} // namespace Remus
