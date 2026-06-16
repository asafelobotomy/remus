#include "scan_controller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QSettings>
#include <QThread>

#include "app_controller.h"
#include "../../core/constants/constants.h"
#include "../../core/database.h"

namespace Remus {

namespace {

    constexpr int kProgressMinIntervalMs = 200;
    constexpr int kPersistProgressStep = 50;

    class ProgressThrottler {
    public:
        explicit ProgressThrottler(ScanController *controller)
            : m_controller(controller) { }

        LibraryService::ProgressCallback scanCallback() {
            return [this](int done, int total, const QString &path) { reportScan(done, total, path); };
        }

        LibraryService::ProgressCallback persistCallback() {
            return [this](int done, int total, const QString &) { reportPersist(done, total); };
        }

        void flushScan(bool force) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const bool finished = m_pendingTotal > 0 && m_pendingDone >= m_pendingTotal;
            if (!force && !finished && (now - m_lastFlushMs) < kProgressMinIntervalMs)
                return;

            m_lastFlushMs = now;
            const int done = m_pendingDone;
            const int total = m_pendingTotal;
            const QString path = m_pendingPath;
            m_pendingPath.clear();

            QMetaObject::invokeMethod(m_controller, "applyScanProgress", Qt::QueuedConnection, Q_ARG(int, done),
                Q_ARG(int, total), Q_ARG(QString, path));
        }

    private:
        void reportScan(int done, int total, const QString &path) {
            m_pendingDone = done;
            m_pendingTotal = total;
            if (!path.isEmpty())
                m_pendingPath = path;
            flushScan(false);
        }

        void reportPersist(int done, int total) {
            const bool finished = total > 0 && done >= total;
            if (!finished && done % kPersistProgressStep != 0)
                return;

            QMetaObject::invokeMethod(
                m_controller, "applyPersistProgress", Qt::QueuedConnection, Q_ARG(int, done), Q_ARG(int, total));
        }

        ScanController *m_controller;
        qint64 m_lastFlushMs = 0;
        int m_pendingDone = 0;
        int m_pendingTotal = 0;
        QString m_pendingPath;
    };

} // namespace

ScanController::ScanController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController) {
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    QString romSource = settings.value(QStringLiteral("gui/rom_source_directory")).toString().trimmed();
    if (romSource.isEmpty()) {
        romSource = settings.value(QStringLiteral("scan/last_directory")).toString().trimmed();
        if (!romSource.isEmpty()) {
            settings.setValue(QStringLiteral("gui/rom_source_directory"), romSource);
            settings.sync();
        }
    }
    m_lastDirectory = QDir::cleanPath(romSource);
}

ScanController::~ScanController() {
    if (m_thread && m_thread->isRunning()) {
        stopScan();
        m_thread->wait();
    }
}

void ScanController::startScan(const QString &directory) {
    if (m_scanning) {
        emit scanError(QStringLiteral("A scan is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit scanError(QStringLiteral("Open a library database before scanning."));
        return;
    }

    const QString cleanedDirectory = QDir::cleanPath(directory.trimmed());
    if (!QDir(cleanedDirectory).exists()) {
        emit scanError(QStringLiteral("Directory does not exist: %1").arg(cleanedDirectory));
        return;
    }

    m_scanning = true;
    m_scannedFiles = 0;
    m_totalFiles = 0;
    m_progressMessage = QStringLiteral("Scanning files\u2026");
    m_recentLogs.clear();
    m_lastDirectory = cleanedDirectory;
    emit scanningChanged();
    emit progressChanged();
    emit progressMessageChanged();
    emit recentLogsChanged();
    emit lastDirectoryChanged();

    Database *db = m_appController->database();
    const int libraryId = db->insertLibrary(cleanedDirectory, QFileInfo(cleanedDirectory).fileName());
    const QString dbPath = m_appController->libraryPath();

    m_thread = QThread::create([this, cleanedDirectory, dbPath, libraryId]() {
        LibraryService workerService;
        m_workerService.store(&workerService);

        ProgressThrottler throttler(this);
        const QList<ScanResult> results
            = workerService.scanFilesystem(cleanedDirectory, throttler.scanCallback(), nullptr);

        throttler.flushScan(true);

        if (workerService.wasCancelled()) {
            m_workerService.store(nullptr);
            QMetaObject::invokeMethod(this, [this]() { finishScanCancelled(); }, Qt::QueuedConnection);
            return;
        }

        Database workerDb;
        const QString connectionName
            = QStringLiteral("remus-scan-%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        if (!workerDb.initialize(dbPath, connectionName)) {
            m_workerService.store(nullptr);
            QMetaObject::invokeMethod(
                this, [this]() { finishScanError(QStringLiteral("Failed to open database for scan save.")); },
                Qt::QueuedConnection);
            return;
        }

        const int toInsert = results.size();
        QMetaObject::invokeMethod(this, [this, toInsert]() { beginPersistPhase(toInsert); }, Qt::QueuedConnection);

        const int inserted
            = workerService.persistScanResults(results, libraryId, &workerDb, throttler.persistCallback());

        workerDb.close();
        m_workerService.store(nullptr);

        QMetaObject::invokeMethod(
            this, [this, inserted, toInsert]() { finishScanSuccess(inserted, toInsert); }, Qt::QueuedConnection);
    });

    connect(
        m_thread, &QThread::finished, this,
        [this]() {
            m_thread->deleteLater();
            m_thread = nullptr;
        },
        Qt::QueuedConnection);
    m_thread->start();
}

void ScanController::stopScan() {
    if (!m_scanning) {
        return;
    }

    if (LibraryService *worker = m_workerService.load())
        worker->cancelScan();
    else
        m_libraryService.cancelScan();
}

void ScanController::setLastDirectory(const QString &directory) {
    const QString cleaned = QDir::cleanPath(directory.trimmed());
    if (m_lastDirectory == cleaned) {
        return;
    }

    m_lastDirectory = cleaned;
    emit lastDirectoryChanged();

    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    settings.setValue(QStringLiteral("gui/rom_source_directory"), cleaned);
    settings.setValue(QStringLiteral("scan/last_directory"), cleaned);
    settings.sync();
}

void ScanController::appendLog(const QString &message) {
    if (message.isEmpty()) {
        return;
    }

    m_recentLogs.append(message);
    while (m_recentLogs.size() > 12) {
        m_recentLogs.removeFirst();
    }
    emit recentLogsChanged();
}

void ScanController::applyScanProgress(int done, int total, const QString &path) {
    m_scannedFiles = done;
    if (total > 0)
        m_totalFiles = total;
    m_progressMessage = total > 0 && done >= total ? QStringLiteral("Scan complete: %1 files found").arg(done)
                                                   : QStringLiteral("Scanning files\u2026 %1 found").arg(done);
    emit progressChanged();
    emit progressMessageChanged();
    if (!path.isEmpty())
        appendLog(QFileInfo(path).fileName());
}

void ScanController::applyPersistProgress(int done, int total) {
    m_scannedFiles = done;
    m_totalFiles = total > 0 ? total : done;
    m_progressMessage = QStringLiteral("Saving to library\u2026 %1 / %2").arg(done).arg(m_totalFiles);
    emit progressChanged();
    emit progressMessageChanged();
}

void ScanController::finishScanCancelled() {
    m_scanning = false;
    m_progressMessage.clear();
    emit scanningChanged();
    emit progressMessageChanged();
    m_appController->setStatusMessage(QStringLiteral("Scan cancelled."));
    emit scanError(QStringLiteral("Scan cancelled."));
}

void ScanController::finishScanError(const QString &message) {
    m_scanning = false;
    emit scanningChanged();
    m_appController->setStatusMessage(message);
    emit scanError(message);
}

void ScanController::beginPersistPhase(int total) {
    m_scannedFiles = 0;
    m_totalFiles = total;
    m_progressMessage = QStringLiteral("Saving to library\u2026 0 / %1").arg(total);
    emit progressChanged();
    emit progressMessageChanged();
}

void ScanController::finishScanSuccess(int inserted, int total) {
    m_totalFiles = total;
    m_scannedFiles = total;
    m_progressMessage = QStringLiteral("Inserted %1 files into database").arg(inserted);
    emit progressChanged();
    emit progressMessageChanged();

    m_scanning = false;
    emit scanningChanged();

    m_appController->setStatusMessage(QStringLiteral("Scan complete: %1 files added.").arg(inserted));
    emit scanCompleted(inserted);
    emit libraryChanged();
}

} // namespace Remus
