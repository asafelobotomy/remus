#include "database.h"
#include "patched_rom_parser.h"
#include "constants/file_types.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QFileInfo>

namespace Remus {

namespace {

// Column order must match kFileSelectColumns below exactly.
// 0=id, 1=library_id, 2=original_path, 3=current_path, 4=filename, 5=extension,
// 6=file_size, 7=is_compressed, 8=archive_path, 9=archive_internal_path,
// 10=system_id, 11=crc32, 12=md5, 13=sha1, 14=hash_calculated,
// 15=is_primary, 16=parent_file_id, 17=base_title, 18=file_type, 19=is_patched,
// 20=patch_name, 21=is_processed, 22=processing_status,
// 23=last_modified, 24=scanned_at
static const char kFileSelectColumns[] =
    "id, library_id, original_path, current_path, filename, extension, "
    "file_size, is_compressed, archive_path, archive_internal_path, "
    "system_id, crc32, md5, sha1, hash_calculated, "
    "is_primary, parent_file_id, base_title, file_type, is_patched, "
    "patch_name, is_processed, processing_status, last_modified, scanned_at";

static FileRecord fileRecordFromRow(const QSqlQuery &q)
{
    FileRecord r;
    r.id               = q.value(0).toInt();
    r.libraryId        = q.value(1).toInt();
    r.originalPath     = q.value(2).toString();
    r.currentPath      = q.value(3).toString();
    r.filename         = q.value(4).toString();
    r.extension        = q.value(5).toString();
    r.fileSize         = q.value(6).toLongLong();
    r.isCompressed     = q.value(7).toBool();
    r.archivePath      = q.value(8).toString();
    r.archiveInternalPath = q.value(9).toString();
    r.systemId         = q.value(10).toInt();
    r.crc32            = q.value(11).toString();
    r.md5              = q.value(12).toString();
    r.sha1             = q.value(13).toString();
    r.hashCalculated   = q.value(14).toBool();
    r.isPrimary        = q.value(15).toBool();
    r.parentFileId     = q.value(16).toInt();
    r.baseTitle        = q.value(17).toString();
    r.fileType         = q.value(18).toString();
    r.isPatched        = q.value(19).toBool();
    r.patchName        = q.value(20).toString();
    r.isProcessed      = q.value(21).toBool();
    r.processingStatus = q.value(22).toString();
    r.lastModified     = q.value(23).toDateTime();
    r.scannedAt        = q.value(24).toDateTime();
    return r;
}

} // anonymous namespace

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
    const bool hasExplicitClassification = !Constants::FileTypes::isOfficial(record.fileType) ||
        record.isPatched || !record.patchName.isEmpty();
    const QString baseTitle = record.baseTitle.isEmpty() ? patchedInfo.baseTitle : record.baseTitle;
    const QString fileType = hasExplicitClassification ? record.fileType : patchedInfo.fileType;
    const bool isPatched = record.isPatched || patchedInfo.isPatched;
    const QString patchName = record.patchName.isEmpty() ? patchedInfo.patchName : record.patchName;

    QSqlQuery existingQuery(m_db);
    existingQuery.prepare(
        "SELECT id FROM files WHERE original_path = ? AND filename = ? "
        "AND COALESCE(archive_internal_path, '') = COALESCE(?, '')");
    existingQuery.addBindValue(record.originalPath);
    existingQuery.addBindValue(record.filename);
    existingQuery.addBindValue(record.archiveInternalPath);
    if (!existingQuery.exec()) {
        logError("Failed to detect duplicate file: " + existingQuery.lastError().text());
        return 0;
    }
    if (existingQuery.next()) {
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO files 
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

    const int newId = query.lastInsertId().toInt();
    if (newId <= 0) {
        logError("Failed to resolve inserted file id: lastInsertId returned 0");
        return 0;
    }
    return newId;
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
    if (!query.exec(R"(
        SELECT s.name, COUNT(f.id)
        FROM files f
        LEFT JOIN systems s ON f.system_id = s.id
        WHERE f.is_primary = 1
        GROUP BY s.name
    )")) {
        logError("Failed to get file count by system: " + query.lastError().text());
        return counts;
    }

    while (query.next()) {
        const QString systemName = query.value(0).toString();
        counts[systemName.isEmpty() ? QStringLiteral("Unknown") : systemName] = query.value(1).toInt();
    }

    return counts;
}

FileRecord Database::getFileById(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT %1 FROM files WHERE id = ?").arg(QLatin1String(kFileSelectColumns)));
    query.addBindValue(fileId);
    if (query.exec() && query.next())
        return fileRecordFromRow(query);
    return {};
}

QList<FileRecord> Database::getAllFiles()
{
    QSqlQuery query(m_db);
    if (!query.exec(QString("SELECT %1 FROM files").arg(QLatin1String(kFileSelectColumns)))) {
        logError("Failed to get all files: " + query.lastError().text());
        return {};
    }
    QList<FileRecord> files;
    while (query.next())
        files.append(fileRecordFromRow(query));
    return files;
}

QList<FileRecord> Database::getExistingFiles()
{
    QSqlQuery query(m_db);
    if (!query.exec(QString("SELECT %1 FROM files").arg(QLatin1String(kFileSelectColumns)))) {
        logError("Failed to get existing files: " + query.lastError().text());
        return {};
    }
    QList<FileRecord> files;
    while (query.next()) {
        FileRecord record = fileRecordFromRow(query);
        if (QFileInfo::exists(record.currentPath))
            files.append(record);
    }
    return files;
}

QList<FileRecord> Database::getFilesWithConfirmedMatch()
{
    QSqlQuery query(m_db);
    const QString sql = QString(
        "SELECT %1 FROM files "
        "WHERE id IN ("
        "  SELECT DISTINCT file_id FROM matches WHERE is_confirmed = 1 AND is_rejected = 0"
        ")").arg(QLatin1String(kFileSelectColumns));
    if (!query.exec(sql)) {
        logError("Failed to get files with confirmed match: " + query.lastError().text());
        return {};
    }
    QList<FileRecord> files;
    while (query.next())
        files.append(fileRecordFromRow(query));
    return files;
}

QList<FileRecord> Database::getFilesBySystem(const QString &systemName)
{
    const int systemId = getSystemId(systemName);
    if (systemId == 0) {
        logError("System not found: " + systemName);
        return {};
    }
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT %1 FROM files WHERE system_id = ? AND is_primary = 1")
                      .arg(QLatin1String(kFileSelectColumns)));
    query.addBindValue(systemId);
    if (!query.exec()) {
        logError("Failed to get files by system: " + query.lastError().text());
        return {};
    }
    QList<FileRecord> files;
    while (query.next())
        files.append(fileRecordFromRow(query));
    return files;
}

QList<FileRecord> Database::getFilesByParent(int parentId)
{
    QSqlQuery query(m_db);
    query.prepare(QString("SELECT %1 FROM files WHERE parent_file_id = ?")
                      .arg(QLatin1String(kFileSelectColumns)));
    query.addBindValue(parentId);
    if (!query.exec()) {
        logError("Failed to get files by parent: " + query.lastError().text());
        return {};
    }
    QList<FileRecord> files;
    while (query.next())
        files.append(fileRecordFromRow(query));
    return files;
}

} // namespace Remus
