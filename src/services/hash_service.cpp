#include "hash_service.h"

#include "../core/hasher.h"
#include "../core/database.h"
#include "../core/archive_extractor.h"

#include <QFileInfo>
#include <QTemporaryDir>

namespace Remus {

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
    int hashed = 0;

    for (int i = 0; i < total; ++i) {
        if (cancelled && cancelled->load()) break;

        const FileRecord &file = files[i];
        if (progressCb) progressCb(i, total, file.currentPath);

        HashResult result = hashRecord(file);
        if (result.success) {
            db->updateFileHashes(file.id, result.crc32, result.md5, result.sha1);
            hashed++;
        } else if (logCb) {
            logCb(QString("Hash failed for %1: %2").arg(file.filename, result.error));
        }
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
