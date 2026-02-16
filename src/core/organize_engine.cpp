#include "organize_engine.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logCore)
#define qInfo() qCInfo(logCore)
#define qWarning() qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

OrganizeEngine::OrganizeEngine(Database &db, QObject *parent)
    : QObject(parent)
    , m_database(db)
    , m_templateEngine(new TemplateEngine(this))
    , m_template(TemplateEngine::getNoIntroTemplate())
    , m_collisionStrategy(CollisionStrategy::Rename)
    , m_dryRun(false)
{
}

void OrganizeEngine::setTemplate(const QString &templateStr)
{
    if (TemplateEngine::validateTemplate(templateStr)) {
        m_template = templateStr;
        qInfo() << "Template set to:" << templateStr;
    } else {
        qWarning() << "Invalid template:" << templateStr;
        emit operationCompleted(-1, false, "Invalid template: " + templateStr);
    }
}

void OrganizeEngine::setCollisionStrategy(CollisionStrategy strategy)
{
    m_collisionStrategy = strategy;
}

void OrganizeEngine::setDryRun(bool enabled)
{
    m_dryRun = enabled;
    qInfo() << "Dry-run mode:" << (enabled ? "ENABLED" : "DISABLED");
}

OrganizeResult OrganizeEngine::organizeFile(int fileId,
                                            const GameMetadata &metadata,
                                            const QString &destinationDir,
                                            FileOperation operation)
{
    OrganizeResult result;
    result.success = false;
    result.operation = operation;
    result.undoId = -1;

    // Get file info from database
    FileRecord fileRecord = m_database.getFileById(fileId);
    if (fileRecord.id == 0) {
        result.error = "File not found in database";
        emit operationCompleted(fileId, false, result.error);
        return result;
    }

    result.oldPath = fileRecord.currentPath;

    // Generate destination path
    QString newPath = generateDestinationPath(fileRecord, metadata, destinationDir);
    result.newPath = newPath;

    emit operationStarted(fileId, result.oldPath, newPath);

    // Check for collision
    if (wouldCollide(newPath)) {
        if (m_collisionStrategy == CollisionStrategy::Skip) {
            result.error = "File exists at destination, skipping";
            qInfo() << "Skipping (file exists):" << newPath;
            emit operationCompleted(fileId, false, result.error);
            return result;
        } else if (m_collisionStrategy == CollisionStrategy::Rename) {
            newPath = resolveCollision(newPath, m_collisionStrategy);
            result.newPath = newPath;
            qInfo() << "Collision detected, renamed to:" << newPath;
        }
        // Overwrite strategy falls through
    }

    // Dry-run mode: preview only
    if (m_dryRun) {
        emit dryRunPreview(result.oldPath, newPath, operation);
        qInfo() << "[DRY RUN]" << (operation == FileOperation::Move ? "MOVE" : "COPY") 
                << result.oldPath << "->" << newPath;
        result.success = true;
        return result;
    }

    // Execute operation
    if (executeOperation(result.oldPath, newPath, operation)) {
        result.success = true;

        // Record undo information
        result.undoId = recordUndo(result.oldPath, newPath, operation);

        // Update database with new path
        m_database.updateFilePath(fileId, newPath);

        qInfo() << "✓" << (operation == FileOperation::Move ? "Moved" : "Copied")
                << result.oldPath << "->" << newPath;
        emit operationCompleted(fileId, true, "");
    } else {
        result.error = "File operation failed";
        qWarning() << "✗ Operation failed:" << result.oldPath << "->" << newPath;
        emit operationCompleted(fileId, false, result.error);
    }

    return result;
}

QList<OrganizeResult> OrganizeEngine::organizeFiles(const QList<int> &fileIds,
                                                    const QMap<int, GameMetadata> &metadataMap,
                                                    const QString &destinationDir,
                                                    FileOperation operation)
{
    QList<OrganizeResult> results;
    int total = fileIds.size();
    int current = 0;

    qInfo() << "Organizing" << total << "files to" << destinationDir 
            << (m_dryRun ? "(DRY RUN)" : "");

    for (int fileId : fileIds) {
        current++;
        emit progressUpdate(current, total);

        if (!metadataMap.contains(fileId)) {
            qWarning() << "No metadata for file ID:" << fileId << ", skipping";
            OrganizeResult result;
            result.success = false;
            result.error = "No metadata available";
            results.append(result);
            continue;
        }

        const GameMetadata &metadata = metadataMap[fileId];
        OrganizeResult result = organizeFile(fileId, metadata, destinationDir, operation);
        results.append(result);
    }

    qInfo() << "Organization complete:" << total << "files processed";
    return results;
}

bool OrganizeEngine::undoOperation(int undoId)
{
    QSqlQuery query(m_database.database());
    query.prepare(R"(
        SELECT operation_type, old_path, new_path, file_id, undone
        FROM undo_queue
        WHERE id = ?
    )");
    query.addBindValue(undoId);

    if (!query.exec() || !query.next()) {
        qWarning() << "Undo record not found for ID:" << undoId;
        return false;
    }

    QString operationType = query.value(0).toString();
    QString oldPath = query.value(1).toString();
    QString newPath = query.value(2).toString();
    int fileId = query.value(3).toInt();
    bool undone = query.value(4).toBool();

    if (undone) {
        qWarning() << "Undo record already applied for ID:" << undoId;
        return false;
    }

    bool success = false;

    if (operationType == "move" || operationType == "rename") {
        if (!QFile::exists(newPath)) {
            qWarning() << "Cannot undo move, source missing:" << newPath;
            return false;
        }

        QFileInfo oldInfo(oldPath);
        QDir oldDir = oldInfo.absoluteDir();
        if (!oldDir.exists() && !oldDir.mkpath(".")) {
            qWarning() << "Failed to create directory for undo:" << oldDir.absolutePath();
            return false;
        }

        success = QFile::rename(newPath, oldPath);
        if (success && fileId > 0) {
            m_database.updateFilePath(fileId, oldPath);
        }
    } else if (operationType == "copy") {
        if (QFile::exists(newPath)) {
            success = QFile::remove(newPath);
        } else {
            qWarning() << "Cannot undo copy, file missing:" << newPath;
            success = false;
        }

        if (success && fileId > 0) {
            m_database.updateFilePath(fileId, oldPath);
        }
    } else if (operationType == "delete") {
        qWarning() << "Undo not supported for delete operations";
        return false;
    } else {
        qWarning() << "Unknown undo operation type:" << operationType;
        return false;
    }

    if (!success) {
        qWarning() << "Undo failed for ID:" << undoId;
        return false;
    }

    QSqlQuery update(m_database.database());
    update.prepare("UPDATE undo_queue SET undone = 1, undone_at = CURRENT_TIMESTAMP WHERE id = ?");
    update.addBindValue(undoId);
    if (!update.exec()) {
        qWarning() << "Failed to mark undo as completed:" << update.lastError().text();
        return false;
    }

    qInfo() << "Undo completed for ID:" << undoId;
    return true;
}

int OrganizeEngine::undoAll(int limit)
{
    QSqlQuery query(m_database.database());

    QString sql = "SELECT id FROM undo_queue WHERE undone = 0 ORDER BY executed_at DESC";
    if (limit > 0) {
        sql += " LIMIT ?";
    }

    query.prepare(sql);
    if (limit > 0) {
        query.addBindValue(limit);
    }

    if (!query.exec()) {
        qWarning() << "Failed to query undo queue:" << query.lastError().text();
        return 0;
    }

    QList<int> undoIds;
    while (query.next()) {
        undoIds.append(query.value(0).toInt());
    }

    int undoneCount = 0;
    for (int id : undoIds) {
        if (undoOperation(id)) {
            undoneCount++;
        }
    }

    qInfo() << "Undo all complete:" << undoneCount << "operations";
    return undoneCount;
}

bool OrganizeEngine::wouldCollide(const QString &path)
{
    return QFile::exists(path);
}

QString OrganizeEngine::resolveCollision(const QString &path, CollisionStrategy strategy)
{
    if (strategy == CollisionStrategy::Overwrite) {
        return path;
    }

    if (strategy == CollisionStrategy::Skip) {
        return path;
    }

    // Rename strategy: add suffix
    QFileInfo info(path);
    QString baseName = info.completeBaseName();
    QString extension = info.suffix();
    QString dir = info.absolutePath();

    int counter = 1;
    QString newPath;

    do {
        newPath = dir + "/" + baseName + "_" + QString::number(counter);
        if (!extension.isEmpty()) {
            newPath += "." + extension;
        }
        counter++;
    } while (QFile::exists(newPath));

    return newPath;
}

bool OrganizeEngine::executeOperation(const QString &oldPath, const QString &newPath, FileOperation operation)
{
    // Ensure destination directory exists
    QFileInfo destInfo(newPath);
    QDir destDir = destInfo.absoluteDir();
    if (!destDir.exists()) {
        if (!destDir.mkpath(".")) {
            qWarning() << "Failed to create destination directory:" << destDir.absolutePath();
            return false;
        }
    }

    switch (operation) {
        case FileOperation::Move:
        case FileOperation::Rename:
            return QFile::rename(oldPath, newPath);

        case FileOperation::Copy:
            return QFile::copy(oldPath, newPath);

        case FileOperation::Delete:
            return QFile::remove(oldPath);

        default:
            qWarning() << "Unknown file operation";
            return false;
    }
}

int OrganizeEngine::recordUndo(const QString &oldPath, const QString &newPath, FileOperation operation)
{
    QString operationType;
    switch (operation) {
        case FileOperation::Move:
            operationType = "move";
            break;
        case FileOperation::Copy:
            operationType = "copy";
            break;
        case FileOperation::Rename:
            operationType = "rename";
            break;
        case FileOperation::Delete:
            operationType = "delete";
            break;
        default:
            operationType = "unknown";
            break;
    }

    int fileId = 0;
    QSqlQuery fileQuery(m_database.database());
    fileQuery.prepare("SELECT id FROM files WHERE current_path = ? LIMIT 1");
    fileQuery.addBindValue(oldPath);
    if (fileQuery.exec() && fileQuery.next()) {
        fileId = fileQuery.value(0).toInt();
    }

    QSqlQuery query(m_database.database());
    query.prepare(R"(
        INSERT INTO undo_queue (operation_type, old_path, new_path, file_id)
        VALUES (?, ?, ?, ?)
    )");
    query.addBindValue(operationType);
    query.addBindValue(oldPath);
    query.addBindValue(newPath);
    query.addBindValue(fileId > 0 ? QVariant(fileId) : QVariant());

    if (!query.exec()) {
        qWarning() << "Failed to record undo operation:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

QString OrganizeEngine::generateDestinationPath(const FileRecord &fileRecord,
                                               const GameMetadata &metadata,
                                               const QString &destinationDir)
{
    // Build variable map for template
    QMap<QString, QString> variables;

    // File info
    QFileInfo info(fileRecord.currentPath);
    variables["ext"] = info.suffix();

    // Check if this is a multi-disc game (extract disc number)
    int discNum = TemplateEngine::extractDiscNumber(info.fileName());
    if (discNum > 0) {
        variables["disc"] = QString::number(discNum);
    }

    // Apply template
    QString filename = m_templateEngine->applyTemplate(m_template, metadata, variables);

    // Combine with destination directory
    QString fullPath = QDir(destinationDir).filePath(filename);

    return fullPath;
}

} // namespace Remus
