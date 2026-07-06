#pragma once

#include "compendium_build_options.h"

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace Remus {

class AppController;
class SettingsController;

class CompendiumBuildController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool building READ isBuilding NOTIFY buildingChanged)
    Q_PROPERTY(bool monitoringDetached READ isMonitoringDetached NOTIFY buildingChanged)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY progressChanged)
    Q_PROPERTY(int progressValue READ progressValue NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressChanged)
    Q_PROPERTY(QString buildPhase READ buildPhase NOTIFY progressChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString logTail READ logTail NOTIFY logTailChanged)
    Q_PROPERTY(bool preflightReady READ preflightReady NOTIFY preflightChanged)
    Q_PROPERTY(QString preflightMessage READ preflightMessage NOTIFY preflightChanged)
    Q_PROPERTY(QStringList preflightWarnings READ preflightWarnings NOTIFY preflightChanged)
    Q_PROPERTY(QString compendiumDbPath READ compendiumDbPath NOTIFY pathsChanged)
    Q_PROPERTY(QString repoRoot READ repoRoot NOTIFY preflightChanged)
    Q_PROPERTY(QVariantList credentialStatusModel READ credentialStatusModel NOTIFY credentialStatusChanged)
    Q_PROPERTY(QVariantMap buildSummary READ buildSummary NOTIFY buildSummaryChanged)
    Q_PROPERTY(bool hadMergeConflicts READ hadMergeConflicts NOTIFY buildFinished)

public:
    explicit CompendiumBuildController(
        AppController *appController, SettingsController *settingsController, QObject *parent = nullptr);
    ~CompendiumBuildController() override;

    bool isBuilding() const {
        return m_building;
    }
    bool isMonitoringDetached() const {
        return m_monitoringDetached;
    }
    int progressPercent() const {
        return m_progressPercent;
    }
    int progressValue() const {
        return m_progressValue;
    }
    int progressTotal() const {
        return m_progressTotal;
    }
    QString progressMessage() const {
        return m_progressMessage;
    }
    QString buildPhase() const {
        return m_buildPhase;
    }
    QString lastError() const {
        return m_lastError;
    }
    QString logTail() const {
        return m_logTail;
    }
    bool preflightReady() const {
        return m_preflightReady;
    }
    QString preflightMessage() const {
        return m_preflightMessage;
    }
    QStringList preflightWarnings() const {
        return m_preflightWarnings;
    }
    QString compendiumDbPath() const {
        return m_compendiumDbPath;
    }
    QString repoRoot() const {
        return m_repoRoot;
    }
    QVariantList credentialStatusModel() const {
        return m_credentialStatusModel;
    }
    QVariantMap buildSummary() const {
        return m_buildSummary;
    }
    bool hadMergeConflicts() const {
        return m_hadMergeConflicts;
    }

    Q_INVOKABLE void refreshPreflight();
    Q_INVOKABLE void refreshCredentialStatus();
    Q_INVOKABLE QVariantList credentialStatus() const;
    Q_INVOKABLE bool syncEnrichmentCredentials();
    Q_INVOKABLE QVariantMap verifyCredentials(const QString &groupKey);
    Q_INVOKABLE QStringList enrichmentSourceKeys() const;
    Q_INVOKABLE void applyBuildPreset(int preset);
    Q_INVOKABLE QVariantMap currentFullBuildOptions() const;
    Q_INVOKABLE void startFullBuild(const QVariantMap &options);
    Q_INVOKABLE void startExtendBuild(const QVariantMap &options);
    Q_INVOKABLE void reattachToRunningBuild();
    Q_INVOKABLE void cancelBuild();
    Q_INVOKABLE void openLogFile();
    Q_INVOKABLE void openOutputFolder();

signals:
    void buildingChanged();
    void progressChanged();
    void lastErrorChanged();
    void logTailChanged();
    void preflightChanged();
    void pathsChanged();
    void credentialStatusChanged();
    void buildSummaryChanged();
    void buildFinished(bool success, int exitCode);

private slots:
    void pollProgress();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QString resolveRepoRoot() const;
    QString resolveBuildScript() const;
    QString resolveDetachedScript() const;
    QString resolveCliBinary() const;
    QString resolveWritableCompendiumDbPath(const QString &repoRoot) const;
    QString lockFilePath() const;
    QString progressFilePath() const;
    bool isBuildLockHeld() const;
    qint64 detachedBuildPid() const;
    bool isProcessAlive(qint64 pid) const;
    bool ensureCommandAvailable(const QString &command) const;
    void setBuilding(bool building, bool monitoringDetached = false);
    void setLastError(const QString &message);
    void applyCredentialEnvironment(QProcessEnvironment &env) const;
    bool writeEnrichmentCredentialsJson(const QString &path) const;
    bool writeCredentialEnvFile(const QString &path) const;
    bool bootstrapDatabaseIfNeeded();
    void updateProgressFromJson(const QJsonObject &obj);
    void readLogTail();
    void updateBuildSummary();
    qint64 countSignaturesInDb() const;
    void startMonitoring(qint64 pid, bool ownsProcess);
    void finishBuild(int exitCode, QProcess::ExitStatus status);
    CompendiumFullBuildOptions fullBuildOptionsFromMap(const QVariantMap &map) const;
    CompendiumExtendBuildOptions extendBuildOptionsFromMap(const QVariantMap &map) const;

    AppController *m_appController = nullptr;
    SettingsController *m_settingsController = nullptr;
    std::unique_ptr<QProcess> m_process;
    QTimer m_pollTimer;
    QString m_repoRoot;
    QString m_compendiumDbPath;
    QString m_buildLogPath;
    bool m_building = false;
    bool m_monitoringDetached = false;
    bool m_ownsProcess = false;
    qint64 m_monitoredPid = 0;
    bool m_preflightReady = false;
    QString m_preflightMessage;
    QStringList m_preflightWarnings;
    int m_progressPercent = 0;
    int m_progressValue = 0;
    int m_progressTotal = 0;
    QString m_progressMessage;
    QString m_buildPhase;
    QString m_lastError;
    QString m_logTail;
    QVariantList m_credentialStatusModel;
    QVariantMap m_buildSummary;
    bool m_hadMergeConflicts = false;
    CompendiumFullBuildOptions m_lastFullBuildOptions;
};

} // namespace Remus
