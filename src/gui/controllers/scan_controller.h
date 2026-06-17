#pragma once

#include <QObject>
#include <QThread>
#include <atomic>

#include "../../services/library_service.h"

namespace Remus {

class AppController;

class ScanController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QString lastDirectory READ lastDirectory WRITE setLastDirectory NOTIFY lastDirectoryChanged)

public:
    explicit ScanController(AppController *appController, QObject *parent = nullptr);
    ~ScanController();

    bool isScanning() const {
        return m_scanning;
    }
    int scannedFiles() const {
        return m_scannedFiles;
    }
    int totalFiles() const {
        return m_totalFiles;
    }
    QString progressMessage() const {
        return m_progressMessage;
    }
    QString lastDirectory() const {
        return m_lastDirectory;
    }

    Q_INVOKABLE void startScan(const QString &directory);
    Q_INVOKABLE void stopScan();

public slots:
    void setLastDirectory(const QString &directory);

private slots:
    void applyScanProgress(int done, int total, const QString &path);
    void applyPersistProgress(int done, int total);
    void finishScanCancelled();
    void finishScanError(const QString &message);
    void beginPersistPhase(int total);
    void finishScanSuccess(int inserted, int total);

signals:
    void scanningChanged();
    void progressChanged();
    void progressMessageChanged();
    void lastDirectoryChanged();
    void scanCompleted(int insertedFiles);
    void scanError(const QString &message);
    void libraryChanged();

private:
    AppController *m_appController;
    LibraryService m_libraryService;
    std::atomic<LibraryService *> m_workerService { nullptr };
    bool m_scanning = false;
    int m_scannedFiles = 0;
    int m_totalFiles = 0;
    QString m_progressMessage;
    QString m_lastDirectory;
    QThread *m_thread = nullptr;
};

} // namespace Remus
