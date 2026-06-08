#include "scan_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QSettings>
#include <QThread>

#include "app_controller.h"
#include "../../core/constants/constants.h"

namespace Remus {

ScanController::ScanController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController) {
    // Restore the last scanned directory across sessions.
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    m_lastDirectory = settings.value(QStringLiteral("scan/last_directory")).toString();
}

ScanController::~ScanController() {
    if (m_thread && m_thread->isRunning()) {
        m_libraryService.cancelScan();
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

    // Pre-insert the library entry on the main thread before going async.
    Database *db = m_appController->database();
    const int libraryId = db->insertLibrary(cleanedDirectory, QFileInfo(cleanedDirectory).fileName());

    // Run the filesystem scan on a worker thread so the event loop stays alive.
    m_thread = QThread::create([this, cleanedDirectory, db, libraryId]() {
        const QList<ScanResult> results = m_libraryService.scanFilesystem(
            cleanedDirectory,
            [this](int done, int total, const QString &path) {
                QMetaObject::invokeMethod(
                    this,
                    [this, done, total, path]() {
                        m_scannedFiles = done;
                        if (total > 0)
                            m_totalFiles = total;
                        m_progressMessage = QStringLiteral("Scanning files\u2026 %1 found").arg(done);
                        emit progressChanged();
                        emit progressMessageChanged();
                        if (!path.isEmpty())
                            appendLog(QFileInfo(path).fileName());
                    },
                    Qt::QueuedConnection);
            },
            [this](const QString &message) {
                QMetaObject::invokeMethod(this, [this, message]() { appendLog(message); }, Qt::QueuedConnection);
            });

        // Hand results back to the main thread for database insertion.
        QMetaObject::invokeMethod(
            this,
            [this, db, libraryId, results]() {
                if (m_libraryService.wasCancelled()) {
                    m_scanning = false;
                    m_progressMessage = { };
                    emit scanningChanged();
                    emit progressMessageChanged();
                    m_appController->setStatusMessage(QStringLiteral("Scan cancelled."));
                    emit scanError(QStringLiteral("Scan cancelled."));
                    return;
                }

                // Transition to Phase 2: saving to library
                const int toInsert = results.size();
                m_scannedFiles = 0;
                m_totalFiles = toInsert;
                m_progressMessage = QStringLiteral("Saving to library\u2026 0 / %1").arg(toInsert);
                emit progressChanged();
                emit progressMessageChanged();

                const int inserted = m_libraryService.persistScanResults(
                    results, libraryId, db, [this, toInsert](int done, int total, const QString &) {
                        m_scannedFiles = done;
                        m_totalFiles = total > 0 ? total : toInsert;
                        m_progressMessage
                            = QStringLiteral("Saving to library\u2026 %1 / %2").arg(done).arg(m_totalFiles);
                        emit progressChanged();
                        emit progressMessageChanged();
                        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    });

                // Phase complete — fill bar to 100 %
                m_totalFiles = toInsert;
                m_scannedFiles = m_totalFiles;
                m_progressMessage = QStringLiteral("Inserted %1 files into database").arg(inserted);
                emit progressChanged();
                emit progressMessageChanged();

                m_scanning = false;
                emit scanningChanged();

                m_appController->setStatusMessage(QStringLiteral("Scan complete: %1 files added.").arg(inserted));
                emit scanCompleted(inserted);
                emit libraryChanged();
            },
            Qt::QueuedConnection);
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

    m_libraryService.cancelScan();
}

void ScanController::setLastDirectory(const QString &directory) {
    const QString cleaned = QDir::cleanPath(directory.trimmed());
    if (m_lastDirectory == cleaned) {
        return;
    }

    m_lastDirectory = cleaned;
    emit lastDirectoryChanged();

    // Persist across sessions.
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    settings.setValue(QStringLiteral("scan/last_directory"), cleaned);
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

} // namespace Remus