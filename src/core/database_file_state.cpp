#include "database.h"

#include <QSqlError>
#include <QSqlQuery>

namespace Remus {

bool Database::updateFilePath(int fileId, const QString &newPath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET current_path = ? WHERE id = ?");
    query.addBindValue(newPath);
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to update file path: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::updateFileStorageState(const FileRecord &record)
{
    if (record.id <= 0) {
        logError("Failed to update file storage state: invalid file id");
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE files
        SET current_path = ?,
            filename = ?,
            extension = ?,
            file_size = ?,
            is_compressed = ?,
            archive_path = ?,
            archive_internal_path = ?,
            crc32 = ?,
            md5 = ?,
            sha1 = ?,
            hash_calculated = ?
        WHERE id = ?
    )");
    query.addBindValue(record.currentPath);
    query.addBindValue(record.filename);
    query.addBindValue(record.extension);
    query.addBindValue(record.fileSize);
    query.addBindValue(record.isCompressed);
    query.addBindValue(record.archivePath.isEmpty() ? QVariant() : record.archivePath);
    query.addBindValue(record.archiveInternalPath.isEmpty() ? QVariant() : record.archiveInternalPath);
    query.addBindValue(record.crc32.isEmpty() ? QVariant() : record.crc32);
    query.addBindValue(record.md5.isEmpty() ? QVariant() : record.md5);
    query.addBindValue(record.sha1.isEmpty() ? QVariant() : record.sha1);
    query.addBindValue(record.hashCalculated);
    query.addBindValue(record.id);

    if (!query.exec()) {
        logError("Failed to update file storage state: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::updateFileOriginalPath(int fileId, const QString &newOriginalPath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET original_path = ?, current_path = ? WHERE id = ?");
    query.addBindValue(newOriginalPath);
    query.addBindValue(newOriginalPath);
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to update file original path: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

QString Database::getFilePath(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT current_path FROM files WHERE id = ?");
    query.addBindValue(fileId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

bool Database::markFileProcessed(int fileId, const QString &status)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET is_processed = 1, processing_status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to mark file as processed: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::markFileUnprocessed(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET is_processed = 0, processing_status = 'unprocessed' WHERE id = ?");
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to mark file as unprocessed: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

QList<FileRecord> Database::getProcessedFiles()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files WHERE is_primary = 1 AND is_processed = 1")) {
        logError("Failed to get processed files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        const int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }

    return files;
}

QList<FileRecord> Database::getUnprocessedFiles()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files WHERE is_primary = 1 AND is_processed = 0")) {
        logError("Failed to get unprocessed files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        const int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }

    return files;
}

} // namespace Remus