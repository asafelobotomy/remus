#include "scan_controller.h"

#include <QDir>
#include <QFileInfo>

#include "app_controller.h"

namespace Remus {

ScanController::ScanController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
}

void ScanController::startScan(const QString &directory)
{
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
    m_recentLogs.clear();
    m_lastDirectory = cleanedDirectory;
    emit scanningChanged();
    emit progressChanged();
    emit recentLogsChanged();
    emit lastDirectoryChanged();

    Database *db = m_appController->database();
    const int libraryId = db->insertLibrary(cleanedDirectory, QFileInfo(cleanedDirectory).fileName());
    const int inserted = m_libraryService.scan(
        cleanedDirectory,
        db,
        [this](int done, int total, const QString &path) {
            m_scannedFiles = done;
            m_totalFiles = total;
            emit progressChanged();
            if (!path.isEmpty()) {
                appendLog(QFileInfo(path).fileName());
            }
        },
        [this](const QString &message) {
            appendLog(message);
        },
        libraryId);

    m_scanning = false;
    emit scanningChanged();

    if (m_libraryService.wasCancelled()) {
        m_appController->setStatusMessage(QStringLiteral("Scan cancelled."));
        emit scanError(QStringLiteral("Scan cancelled."));
        return;
    }

    m_appController->setStatusMessage(QStringLiteral("Scan complete: %1 files added.").arg(inserted));
    emit scanCompleted(inserted);
    emit libraryChanged();
}

void ScanController::stopScan()
{
    if (!m_scanning) {
        return;
    }

    m_libraryService.cancelScan();
}

void ScanController::setLastDirectory(const QString &directory)
{
    const QString cleaned = QDir::cleanPath(directory.trimmed());
    if (m_lastDirectory == cleaned) {
        return;
    }

    m_lastDirectory = cleaned;
    emit lastDirectoryChanged();
}

void ScanController::appendLog(const QString &message)
{
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