#include "database.h"
#include "patched_rom_parser.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QFileInfo>

namespace Remus {

bool Database::removeFile(int fileId)
{
    // Remove associated match record first to keep referential integrity
    QSqlQuery matchQuery(m_db);
    matchQuery.prepare("DELETE FROM matches WHERE file_id = ?");
    matchQuery.addBindValue(fileId);
    if (!matchQuery.exec()) {
        logError("Failed to delete match for file " + QString::number(fileId) + ": "
                 + matchQuery.lastError().text());
        // Continue — the file record removal is more important
    }

    QSqlQuery fileQuery(m_db);
    fileQuery.prepare("DELETE FROM files WHERE id = ?");
    fileQuery.addBindValue(fileId);
    if (!fileQuery.exec()) {
        logError("Failed to remove file record " + QString::number(fileId) + ": "
                 + fileQuery.lastError().text());
        return false;
    }

    return fileQuery.numRowsAffected() > 0;
}

int Database::insertFile(const FileRecord &record)
{
    const QString classificationName = !record.archiveInternalPath.isEmpty()
        ? record.archiveInternalPath
        : record.filename;
    const PatchedRomInfo patchedInfo = PatchedRomParser::parse(classificationName);
    const bool hasExplicitClassification = record.fileType != QStringLiteral("official") ||
        record.isPatched || !record.patchName.isEmpty();
    const QString baseTitle = record.baseTitle.isEmpty() ? patchedInfo.baseTitle : record.baseTitle;
    const QString fileType = hasExplicitClassification ? record.fileType : patchedInfo.fileType;
    const bool isPatched = record.isPatched || patchedInfo.isPatched;
    const QString patchName = record.patchName.isEmpty() ? patchedInfo.patchName : record.patchName;

    QSqlQuery query(m_db);
    // Use INSERT OR IGNORE to avoid duplicates based on original_path + filename
    query.prepare(R"(
        INSERT OR IGNORE INTO files 
        (library_id, original_path, current_path, filename, extension, 
         file_size, is_compressed, archive_path, archive_internal_path, 
         system_id, is_primary, parent_file_id, base_title, file_type, is_patched,
         patch_name, last_modified)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(record.libraryId);
    query.addBindValue(record.originalPath);
    query.addBindValue(record.currentPath);
    query.addBindValue(record.filename);
    query.addBindValue(record.extension);
    query.addBindValue(record.fileSize);
    query.addBindValue(record.isCompressed);
    query.addBindValue(record.archivePath.isEmpty() ? QVariant() : record.archivePath);
    query.addBindValue(record.archiveInternalPath.isEmpty() ? QVariant() : record.archiveInternalPath);
    query.addBindValue(record.systemId > 0 ? record.systemId : QVariant());
    query.addBindValue(record.isPrimary);
    query.addBindValue(record.parentFileId > 0 ? record.parentFileId : QVariant());
    query.addBindValue(baseTitle.isEmpty() ? QVariant() : baseTitle);
    query.addBindValue(fileType);
    query.addBindValue(isPatched);
    query.addBindValue(patchName.isEmpty() ? QVariant() : patchName);
    query.addBindValue(record.lastModified);

    if (!query.exec()) {
        logError("Failed to insert file: " + query.lastError().text());
        return 0;
    }

    return query.lastInsertId().toInt();
}

bool Database::updateFileHashes(int fileId, const QString &crc32,
                                 const QString &md5, const QString &sha1)
{
    QSqlQuery query(m_db);
    const AppliedPatchRecord lineage = findAppliedPatchByOutputHashes(crc32, md5, sha1);
    const bool hasLineage = lineage.id > 0;

    QString updateStatement = R"(
        UPDATE files 
        SET crc32 = ?, md5 = ?, sha1 = ?, hash_calculated = 1
    )";
    if (hasLineage) {
        updateStatement += R"(,
        base_title = COALESCE(?, base_title),
        file_type = COALESCE(?, file_type),
        is_patched = 1,
        patch_name = COALESCE(?, patch_name)
    )";
    }
    updateStatement += " WHERE id = ?";

    query.prepare(updateStatement);
    query.addBindValue(crc32);
    query.addBindValue(md5);
    query.addBindValue(sha1);
    if (hasLineage) {
        query.addBindValue(lineage.baseTitle.isEmpty() ? QVariant() : lineage.baseTitle);
        query.addBindValue(lineage.fileType.isEmpty() ? QVariant() : lineage.fileType);
        query.addBindValue(lineage.patchName.isEmpty() ? QVariant() : lineage.patchName);
    }
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to update file hashes: " + query.lastError().text());
        return false;
    }

    return true;
}

QList<FileRecord> Database::getFilesWithoutHashes()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, library_id, current_path, filename, extension,
             file_size, system_id, is_primary, is_compressed,
             archive_path, archive_internal_path, base_title, file_type,
             is_patched, patch_name
        FROM files 
        WHERE hash_calculated = 0 AND is_primary = 1
    )");

    if (!query.exec()) {
        logError("Failed to query files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        FileRecord record;
        record.id = query.value(0).toInt();
        record.libraryId = query.value(1).toInt();
        record.currentPath = query.value(2).toString();
        record.filename = query.value(3).toString();
        record.extension = query.value(4).toString();
        record.fileSize = query.value(5).toLongLong();
        record.systemId = query.value(6).toInt();
        record.isPrimary = query.value(7).toBool();
        record.isCompressed = query.value(8).toBool();
        record.archivePath = query.value(9).toString();
        record.archiveInternalPath = query.value(10).toString();
        record.baseTitle = query.value(11).toString();
        record.fileType = query.value(12).toString();
        record.isPatched = query.value(13).toBool();
        record.patchName = query.value(14).toString();
        files.append(record);
    }

    return files;
}

QMap<QString, int> Database::getFileCountBySystem()
{
    QMap<QString, int> counts;

    QSqlQuery query(m_db);
    query.exec(R"(
        SELECT s.name, COUNT(f.id) 
        FROM files f 
        LEFT JOIN systems s ON f.system_id = s.id 
        WHERE f.is_primary = 1
        GROUP BY s.name
    )");

    while (query.next()) {
        QString systemName = query.value(0).toString();
        int count = query.value(1).toInt();
        counts[systemName.isEmpty() ? "Unknown" : systemName] = count;
    }

    return counts;
}

FileRecord Database::getFileById(int fileId)
{
    FileRecord record;
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, library_id, original_path, current_path, filename, extension,
               file_size, is_compressed, archive_path, archive_internal_path,
               system_id, crc32, md5, sha1, hash_calculated, 
               is_primary, parent_file_id, base_title, file_type, is_patched,
             patch_name, is_processed, processing_status,
             last_modified, scanned_at
        FROM files 
        WHERE id = ?
    )");
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        record.id = query.value(0).toInt();
        record.libraryId = query.value(1).toInt();
        record.originalPath = query.value(2).toString();
        record.currentPath = query.value(3).toString();
        record.filename = query.value(4).toString();
        record.extension = query.value(5).toString();
        record.fileSize = query.value(6).toLongLong();
        record.isCompressed = query.value(7).toBool();
        record.archivePath = query.value(8).toString();
        record.archiveInternalPath = query.value(9).toString();
        record.systemId = query.value(10).toInt();
        record.crc32 = query.value(11).toString();
        record.md5 = query.value(12).toString();
        record.sha1 = query.value(13).toString();
        record.hashCalculated = query.value(14).toBool();
        record.isPrimary = query.value(15).toBool();
        record.parentFileId = query.value(16).toInt();
        record.baseTitle = query.value(17).toString();
        record.fileType = query.value(18).toString();
        record.isPatched = query.value(19).toBool();
        record.patchName = query.value(20).toString();
        record.isProcessed = query.value(21).toBool();
        record.processingStatus = query.value(22).toString();
        record.lastModified = query.value(23).toDateTime();
        record.scannedAt = query.value(24).toDateTime();
    }
    
    return record;
}

QList<FileRecord> Database::getAllFiles()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files")) {
        logError("Failed to get all files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }

    return files;
}

QList<FileRecord> Database::getExistingFiles()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files")) {
        logError("Failed to get existing files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            QFileInfo pathInfo(record.currentPath);
            if (pathInfo.exists()) {
                files.append(record);
            }
        }
    }

    return files;
}

QList<FileRecord> Database::getFilesBySystem(const QString &systemName)
{
    QList<FileRecord> files;
    
    // First get system ID
    int systemId = getSystemId(systemName);
    if (systemId == 0) {
        logError("System not found: " + systemName);
        return files;
    }
    
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM files WHERE system_id = ? AND is_primary = 1");
    query.addBindValue(systemId);
    
    if (!query.exec()) {
        logError("Failed to get files by system: " + query.lastError().text());
        return files;
    }
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

QList<FileRecord> Database::getFilesByParent(int parentId)
{
    QList<FileRecord> files;
    
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM files WHERE parent_file_id = ?");
    query.addBindValue(parentId);
    
    if (!query.exec()) {
        logError("Failed to get files by parent: " + query.lastError().text());
        return files;
    }
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

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
        int fileId = query.value(0).toInt();
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
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

} // namespace Remus
