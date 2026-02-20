#include "hash_service.h"

#include "../core/hasher.h"
#include "../core/database.h"
#include "../core/archive_extractor.h"

#include <QFileInfo>
#include <QThread>
#include <QThreadPool>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentMap>

namespace Remus {

namespace {

struct HashTaskResult {
    int fileId = 0;
    QString filename;
    QString currentPath;
    HashResult result;
    bool skipped = false;
};

} // namespace

HashService::HashService()
    : m_hasher(new Hasher())
{
}

HashService::~HashService()
{
    delete m_hasher;
}

int HashService::hashAll(Database *db,
                         ProgressCallback progressCb,
                         LogCallback logCb,
                         const std::atomic<bool> *cancelled)
{
    if (!db) return 0;

    QList<FileRecord> files = db->getFilesWithoutHashes();
    const int total = files.size();
    if (progressCb) progressCb(0, total, QString());

    if (total == 0) {
        if (logCb) logCb(QString("Hashing complete: 0/0"));
        return 0;
    }

    if (cancelled && cancelled->load()) {
        if (logCb) logCb(QString("Hashing cancelled: 0/%1").arg(total));
        return 0;
    }

    const int idealThreads = QThread::idealThreadCount();
    const int maxThreads = qMax(1, qMin(idealThreads > 0 ? idealThreads : 1, 8));

    QThreadPool *pool = QThreadPool::globalInstance();
    const int originalMaxThreads = pool->maxThreadCount();
    pool->setMaxThreadCount(maxThreads);

    QList<HashTaskResult> taskResults = QtConcurrent::blockingMapped(files,
        [cancelled](const FileRecord &file) {
            HashTaskResult task;
            task.fileId = file.id;
            task.filename = file.filename;
            task.currentPath = file.currentPath;

            if (cancelled && cancelled->load()) {
                task.skipped = true;
                return task;
            }

            HashService worker;
            task.result = worker.hashRecord(file);
            return task;
        });

    pool->setMaxThreadCount(originalMaxThreads);

    int hashed = 0;
    int done = 0;
    for (const HashTaskResult &task : taskResults) {
        if (task.skipped) {
            done++;
            if (progressCb) progressCb(done, total, task.currentPath);
            continue;
        }

        if (task.result.success) {
            db->updateFileHashes(task.fileId,
                                 task.result.crc32,
                                 task.result.md5,
                                 task.result.sha1);
            hashed++;
        } else if (logCb) {
            logCb(QString("Hash failed for %1: %2").arg(task.filename, task.result.error));
        }

        done++;
        if (progressCb) progressCb(done, total, task.currentPath);
    }

    if (progressCb) progressCb(total, total, {});
    if (logCb) logCb(QString("Hashing complete: %1/%2").arg(hashed).arg(total));
    return hashed;
}

bool HashService::hashFile(Database *db, int fileId)
{
    if (!db) return false;

    FileRecord file = db->getFileById(fileId);
    if (file.id == 0) return false;

    HashResult result = hashRecord(file);
    if (result.success) {
        db->updateFileHashes(file.id, result.crc32, result.md5, result.sha1);
        return true;
    }
    return false;
}

HashResult HashService::hashRecord(const FileRecord &file)
{
    // Detect whether this file is inside an archive
    auto isArchivePath = [](const QString &path) {
        const QString lower = path.toLower();
        return lower.endsWith(".zip")  || lower.endsWith(".7z")  || lower.endsWith(".rar") ||
               lower.endsWith(".tar")  || lower.endsWith(".tar.gz") || lower.endsWith(".tgz") ||
               lower.endsWith(".tar.bz2") || lower.endsWith(".tbz2");
    };

    const QString archivePath = file.archivePath.isEmpty() ? file.currentPath : file.archivePath;
    const bool treatAsArchive = file.isCompressed || isArchivePath(archivePath);

    if (!treatAsArchive) {
        int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
        return m_hasher->calculateHashes(file.currentPath, headerSize > 0, headerSize);
    }

    // Archive-aware hashing: extract to temp dir, then hash
    HashResult result;
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists()) {
        result.error = "Archive file not found: " + archivePath;
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result.error = "Failed to create temporary directory";
        return result;
    }

    ArchiveExtractor extractor;
    const QString internalPath = file.archiveInternalPath.isEmpty()
        ? file.filename : file.archiveInternalPath;

    ExtractionResult extraction = extractor.extractFile(archivePath, internalPath, tempDir.path());

    if (!extraction.success || extraction.extractedFiles.isEmpty()) {
        // Fallback: extract entire archive and pick suitable file
        extraction = extractor.extract(archivePath, tempDir.path(), false);
        if (!extraction.success || extraction.extractedFiles.isEmpty()) {
            result.error = extraction.error.isEmpty()
                ? QString("Failed to extract %1 from archive").arg(internalPath)
                : extraction.error;
            return result;
        }

        // Pick first file matching the expected extension
        QString picked;
        for (const QString &path : extraction.extractedFiles) {
            if (path.endsWith(file.extension, Qt::CaseInsensitive)) {
                picked = path;
                break;
            }
        }
        if (picked.isEmpty()) picked = extraction.extractedFiles.first();

        int headerSize = Hasher::detectHeaderSize(picked, file.extension);
        return m_hasher->calculateHashes(picked, headerSize > 0, headerSize);
    }

    const QString extractedPath = extraction.extractedFiles.first();
    int headerSize = Hasher::detectHeaderSize(extractedPath, file.extension);
    return m_hasher->calculateHashes(extractedPath, headerSize > 0, headerSize);
}

} // namespace Remus
