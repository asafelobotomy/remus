#pragma once

#include <QObject>
#include <QStringList>

#include "../../services/library_service.h"

namespace Remus {

class AppController;

class ScanController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(QStringList recentLogs READ recentLogs NOTIFY recentLogsChanged)
    Q_PROPERTY(QString lastDirectory READ lastDirectory WRITE setLastDirectory NOTIFY lastDirectoryChanged)

public:
    explicit ScanController(AppController *appController, QObject *parent = nullptr);

    bool isScanning() const { return m_scanning; }
    int scannedFiles() const { return m_scannedFiles; }
    int totalFiles() const { return m_totalFiles; }
    QStringList recentLogs() const { return m_recentLogs; }
    QString lastDirectory() const { return m_lastDirectory; }

    Q_INVOKABLE void startScan(const QString &directory);
    Q_INVOKABLE void stopScan();

public slots:
    void setLastDirectory(const QString &directory);

signals:
    void scanningChanged();
    void progressChanged();
    void recentLogsChanged();
    void lastDirectoryChanged();
    void scanCompleted(int insertedFiles);
    void scanError(const QString &message);
    void libraryChanged();

private:
    void appendLog(const QString &message);

    AppController *m_appController;
    LibraryService m_libraryService;
    bool m_scanning = false;
    int m_scannedFiles = 0;
    int m_totalFiles = 0;
    QStringList m_recentLogs;
    QString m_lastDirectory;
};

} // namespace Remus