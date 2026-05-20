#include "hash_controller.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent>

#include "app_controller.h"
#include "../../core/database.h"

namespace Remus {

HashController::HashController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(this, &HashController::libraryChanged, m_appController, &AppController::refreshSelectedFile);
}

void HashController::startHashAll()
{
    if (m_hashing) {
        emit hashError(QStringLiteral("Hashing is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit hashError(QStringLiteral("Open a library before hashing files."));
        return;
    }

    m_hashing = true;
    m_hashedFiles = 0;
    m_totalFiles = 0;
    m_progressMessage = QStringLiteral("Hashing files\u2026");
    emit hashingChanged();
    emit progressChanged();
    emit progressMessageChanged();

    // Phase 1 (main thread): read file list from DB — fast.
    const QList<FileRecord> files = m_appController->database()->getFilesWithoutHashes();
    m_totalFiles = files.size();
    emit progressChanged();

    if (files.isEmpty()) {
        m_hashing = false;
        m_progressMessage = QStringLiteral("No files to hash.");
        emit hashingChanged();
        emit progressMessageChanged();
        emit hashCompleted(0);
        return;
    }

    // Phase 2 (worker thread): compute hashes — no DB access, safe to run off GUI thread.
    // Use a dedicated 1-thread pool so blockingMapped inside computeHashes() has the full
    // global thread pool available (avoids thread-pool starvation deadlock).
    auto *workerPool = new QThreadPool(this);
    workerPool->setMaxThreadCount(1);

    using BatchResult = HashService::HashBatchResult;
    auto *watcher = new QFutureWatcher<QList<BatchResult>>(this);

    connect(watcher, &QFutureWatcher<QList<BatchResult>>::finished,
            this, [this, watcher, workerPool]() {
        const QList<BatchResult> results = watcher->result();
        watcher->deleteLater();
        workerPool->deleteLater();

        // Phase 3 (main thread): write results to DB.
        int hashed = 0;
        Database *db = m_appController->database();
        for (const BatchResult &r : results) {
            if (!r.skipped && r.result.success) {
                db->updateFileHashes(r.fileId, r.result.crc32, r.result.md5, r.result.sha1);
                ++hashed;
            }
        }

        m_hashing = false;
        m_progressMessage = QStringLiteral("Hashed %1 file(s).").arg(hashed);
        emit hashingChanged();
        emit progressMessageChanged();
        m_appController->setStatusMessage(QStringLiteral("Hashed %1 file(s).").arg(hashed));
        emit hashCompleted(hashed);
        emit libraryChanged();
    });

    watcher->setFuture(QtConcurrent::run(workerPool, [this, files]() -> QList<BatchResult> {
        return m_hashService.computeHashes(
            files,
            [this](int done, int total, const QString &path) {
                // Marshal progress updates back to the GUI thread.
                QMetaObject::invokeMethod(this, [this, done, total]() {
                    m_hashedFiles = done;
                    m_totalFiles  = total;
                    m_progressMessage = QStringLiteral("Hashing files\u2026 %1 / %2").arg(done).arg(total);
                    emit progressChanged();
                    emit progressMessageChanged();
                }, Qt::QueuedConnection);
                Q_UNUSED(path)
            });
    }));
}

void HashController::hashSelected()
{
    if (m_hashing) {
        emit hashError(QStringLiteral("Hashing is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit hashError(QStringLiteral("Open a library before hashing files."));
        return;
    }

    const int selectedFileId = m_appController->selectedFileId();
    if (selectedFileId <= 0) {
        emit hashError(QStringLiteral("Select a file first."));
        return;
    }

    m_progressMessage = QStringLiteral("Hashing selected file\u2026");
    emit progressMessageChanged();

    if (!m_hashService.hashFile(m_appController->database(), selectedFileId)) {
        m_progressMessage = QStringLiteral("Failed to hash the selected file.");
        emit progressMessageChanged();
        emit hashError(QStringLiteral("Failed to hash the selected file."));
        return;
    }

    m_progressMessage = QStringLiteral("File hashed.");
    emit progressMessageChanged();
    m_appController->setStatusMessage(QStringLiteral("Selected file hashed."));
    emit hashCompleted(1);
    emit libraryChanged();
}

} // namespace Remus