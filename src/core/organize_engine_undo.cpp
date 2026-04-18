#include "organize_engine.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
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

namespace {

QString dottedSuffix(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
}

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
            FileRecord fileRecord = m_database.getFileById(fileId);
            if (fileRecord.id > 0) {
                fileRecord.currentPath = oldPath;
                if (fileRecord.isCompressed && !fileRecord.archivePath.isEmpty()) {
                    fileRecord.archivePath = oldPath;
                } else {
                    fileRecord.filename = QFileInfo(oldPath).fileName();
                    fileRecord.extension = dottedSuffix(oldPath);
                }
                m_database.updateFileStorageState(fileRecord);
            }
        }
    } else if (operationType == "copy") {
        if (QFile::exists(newPath)) {
            success = QFile::remove(newPath);
        } else {
            qWarning() << "Cannot undo copy, file missing:" << newPath;
            success = false;
        }

        if (success && fileId > 0) {
            FileRecord fileRecord = m_database.getFileById(fileId);
            if (fileRecord.id > 0) {
                fileRecord.currentPath = oldPath;
                if (fileRecord.isCompressed && !fileRecord.archivePath.isEmpty()) {
                    fileRecord.archivePath = oldPath;
                } else {
                    fileRecord.filename = QFileInfo(oldPath).fileName();
                    fileRecord.extension = dottedSuffix(oldPath);
                }
                m_database.updateFileStorageState(fileRecord);
            }
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

} // namespace Remus
