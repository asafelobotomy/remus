#include "organize_engine.h"
#include "compendium_disc_bridge.h"
#include "disc_set_utils.h"
#include "disc_title_parser.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QSqlDatabase>
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

    QString dottedSuffix(const QString &path) {
        const QString suffix = QFileInfo(path).suffix();
        return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
    }

    bool restoreBackupPath(const QString &backupPath, const QString &destinationPath) {
        if (!QFile::exists(backupPath)) {
            return true;
        }

        if (QFile::exists(destinationPath) && !QFile::remove(destinationPath)) {
            return false;
        }

        return QFile::rename(backupPath, destinationPath);
    }

    QString discSetSubfolderName(Database &database, const FileRecord &fileRecord, const GameMetadata &metadata) {
        if (fileRecord.discSetKey.isEmpty())
            return { };

        const QList<FileRecord> members = database.getFilesByDiscSetKey(fileRecord.discSetKey);
        int catalogDiscCount = 0;
        QString catalogBaseTitle;

        const QString compendiumPath = database.compendiumDbPath();
        if (!compendiumPath.isEmpty() && QFileInfo::exists(compendiumPath)) {
            const QString connectionName
                = QStringLiteral("organize_catalog_%1").arg(QDateTime::currentMSecsSinceEpoch());
            QSqlDatabase catalogDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            catalogDb.setDatabaseName(compendiumPath);
            if (catalogDb.open()) {
                CatalogDiscSetSummary summary;
                if (lookupCatalogDiscSetSummary(catalogDb, fileRecord.discSetKey, summary) && summary.found) {
                    catalogDiscCount = summary.catalogDiscCount;
                    catalogBaseTitle = summary.baseTitle;
                }
                catalogDb.close();
                QSqlDatabase::removeDatabase(connectionName);
            }
        }

        const int effectiveDiscCount = catalogDiscCount >= 2 ? catalogDiscCount : members.size();
        if (effectiveDiscCount < 2)
            return { };

        QString folderName = catalogBaseTitle;
        if (folderName.isEmpty() && !fileRecord.baseTitle.isEmpty())
            folderName = fileRecord.baseTitle;
        if (folderName.isEmpty() && !metadata.title.isEmpty())
            folderName = DiscTitleParser::extractBaseTitle(metadata.title);
        if (folderName.isEmpty()) {
            folderName = DiscSetUtils::extractBaseTitle(DiscSetUtils::labelPath(fileRecord.currentPath,
                fileRecord.archivePath, fileRecord.archiveInternalPath, fileRecord.filename));
        }

        return DiscSetUtils::sanitizeFolderComponent(folderName);
    }

}

OrganizeEngine::OrganizeEngine(Database &db, QObject *parent)
    : QObject(parent)
    , m_database(db)
    , m_templateEngine(new TemplateEngine(this))
    , m_template(TemplateEngine::getNoIntroTemplate())
    , m_collisionStrategy(CollisionStrategy::Rename)
    , m_dryRun(false)
    , m_folderNaming(Constants::FolderNaming::Scheme::None) { }

void OrganizeEngine::setTemplate(const QString &templateStr) {
    if (TemplateEngine::validateTemplate(templateStr)) {
        m_template = templateStr;
        qInfo() << "Template set to:" << templateStr;
    } else {
        qWarning() << "Invalid template:" << templateStr;
        emit operationCompleted(-1, false, "Invalid template: " + templateStr);
    }
}

void OrganizeEngine::setCollisionStrategy(CollisionStrategy strategy) {
    m_collisionStrategy = strategy;
}

void OrganizeEngine::setDryRun(bool enabled) {
    m_dryRun = enabled;
    qInfo() << "Dry-run mode:" << (enabled ? "ENABLED" : "DISABLED");
}

void OrganizeEngine::setFolderNaming(Constants::FolderNaming::Scheme scheme) {
    m_folderNaming = scheme;
    qInfo() << "Folder naming scheme:" << Constants::FolderNaming::schemeDisplayName(scheme);
}

OrganizeResult OrganizeEngine::organizeFile(
    int fileId, const GameMetadata &metadata, const QString &destinationDir, FileOperation operation) {
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
    if (newPath.isEmpty()) {
        result.error = QStringLiteral("Unsafe destination path rejected");
        emit operationCompleted(fileId, false, result.error);
        return result;
    }
    result.newPath = newPath;

    // If source and destination resolve to the same file, no move is needed.
    // This happens when the bundle step already placed the file at the correct path.
    if (QFileInfo(result.oldPath).absoluteFilePath() == QFileInfo(newPath).absoluteFilePath()) {
        result.success = true;
        result.newPath = result.oldPath;
        emit operationCompleted(fileId, true, QString());
        return result;
    }

    emit operationStarted(fileId, result.oldPath, newPath);

    const bool overwriteExisting = wouldCollide(newPath) && m_collisionStrategy == CollisionStrategy::Overwrite;
    QString backupPath;

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
    }

    // Dry-run mode: preview only
    if (m_dryRun) {
        emit dryRunPreview(result.oldPath, newPath, operation);
        qInfo() << "[DRY RUN]" << (operation == FileOperation::Move ? "MOVE" : "COPY") << result.oldPath << "->"
                << newPath;
        result.success = true;
        return result;
    }

    if (overwriteExisting) {
        QFileInfo targetInfo(newPath);
        backupPath = targetInfo.absoluteDir().filePath(QStringLiteral(".%1.remus_backup_%2")
                .arg(targetInfo.fileName(), QString::number(QDateTime::currentMSecsSinceEpoch())));

        if (!QFile::rename(newPath, backupPath)) {
            result.error = "Failed to stage existing destination for overwrite";
            qWarning() << "Overwrite: failed to move existing file aside:" << newPath;
            emit operationCompleted(fileId, false, result.error);
            return result;
        }
    }

    bool operationSucceeded = false;
    if (overwriteExisting && operation == FileOperation::Copy) {
        QFileInfo targetInfo(newPath);
        const QString stagedCopyPath = targetInfo.absoluteDir().filePath(QStringLiteral(".%1.remus_copy_%2")
                .arg(targetInfo.fileName(), QString::number(QDateTime::currentMSecsSinceEpoch())));

        if (QFile::copy(result.oldPath, stagedCopyPath) && QFile::rename(stagedCopyPath, newPath)) {
            operationSucceeded = true;
        } else {
            QFile::remove(stagedCopyPath);
        }
    } else {
        operationSucceeded = executeOperation(result.oldPath, newPath, operation);
    }

    // Execute operation
    if (operationSucceeded) {
        if (!backupPath.isEmpty()) {
            QFile::remove(backupPath);
        }

        result.success = true;

        // Record undo information
        result.undoId = recordUndo(result.oldPath, newPath, operation);

        // Update database with new path
        FileRecord updatedRecord = fileRecord;
        updatedRecord.currentPath = newPath;
        if (updatedRecord.isCompressed && !updatedRecord.archivePath.isEmpty()) {
            updatedRecord.archivePath = newPath;
        } else {
            updatedRecord.filename = QFileInfo(newPath).fileName();
            updatedRecord.extension = dottedSuffix(newPath);
        }
        m_database.updateFileStorageState(updatedRecord);

        qInfo() << "✓" << (operation == FileOperation::Move ? "Moved" : "Copied") << result.oldPath << "->" << newPath;
        emit operationCompleted(fileId, true, "");
    } else {
        if (!backupPath.isEmpty() && !restoreBackupPath(backupPath, newPath)) {
            qWarning() << "Overwrite: failed to restore original destination after error:" << newPath;
        }
        result.error = "File operation failed";
        qWarning() << "✗ Operation failed:" << result.oldPath << "->" << newPath;
        emit operationCompleted(fileId, false, result.error);
    }

    return result;
}

QList<OrganizeResult> OrganizeEngine::organizeFiles(const QList<int> &fileIds,
    const QMap<int, GameMetadata> &metadataMap, const QString &destinationDir, FileOperation operation) {
    QList<OrganizeResult> results;
    int total = fileIds.size();
    int current = 0;

    qInfo() << "Organizing" << total << "files to" << destinationDir << (m_dryRun ? "(DRY RUN)" : "");

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

bool OrganizeEngine::wouldCollide(const QString &path) {
    return QFile::exists(path);
}

QString OrganizeEngine::resolveCollision(const QString &path, CollisionStrategy strategy) {
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

bool OrganizeEngine::executeOperation(const QString &oldPath, const QString &newPath, FileOperation operation) {
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

int OrganizeEngine::recordUndo(const QString &oldPath, const QString &newPath, FileOperation operation) {
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

QString OrganizeEngine::generateDestinationPath(
    const FileRecord &fileRecord, const GameMetadata &metadata, const QString &destinationDir) {
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

    // Sanitize: the template result is a filename component. Path separators
    // injected via metadata values must not allow escaping the destination root.
    filename.replace(QLatin1Char('/'), QLatin1Char('_'));
    filename.replace(QLatin1Char('\\'), QLatin1Char('_'));

    // Build destination: optionally add system subfolder, then multi-disc set folder.
    QDir destDir(destinationDir);
    if (m_folderNaming != Constants::FolderNaming::Scheme::None) {
        QString systemFolder = Constants::FolderNaming::folderNameForSystemId(fileRecord.systemId, m_folderNaming);
        if (!systemFolder.isEmpty()) {
            destDir = QDir(destDir.filePath(systemFolder));
        }
    }

    const QString discSetFolder = discSetSubfolderName(m_database, fileRecord, metadata);
    if (!discSetFolder.isEmpty())
        destDir = QDir(destDir.filePath(discSetFolder));

    const QString resolved = destDir.absoluteFilePath(filename);
    // Belt-and-suspenders containment check against the caller-supplied root.
    const QString root = QDir(destinationDir).absolutePath();
    if (!resolved.startsWith(root + QLatin1Char('/'))) {
        qWarning() << "OrganizeEngine: destination path escapes root, rejecting:" << resolved;
        return QString();
    }
    return resolved;
}

} // namespace Remus
