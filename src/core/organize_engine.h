#pragma once

#include <QObject>
#include <QString>
#include <QFileInfo>
#include <QList>
#include "template_engine.h"
#include "database.h"
#include "../metadata/metadata_provider.h"

namespace Remus {

/**
 * @brief File operation types for undo system
 */
enum class FileOperation {
    Move,
    Copy,
    Rename,
    Delete
};

/**
 * @brief Result of an organize operation
 */
struct OrganizeResult {
    bool success;
    QString oldPath;
    QString newPath;
    FileOperation operation;
    QString error;
    int undoId;  // ID for undo tracking in database
};

/**
 * @brief Collision resolution strategies
 */
enum class CollisionStrategy {
    Skip,           // Skip file if destination exists
    Overwrite,      // Overwrite destination file
    Rename,         // Auto-rename with suffix (file_1, file_2, etc.)
    Ask             // Ask user (for GUI, not CLI)
};

/**
 * @brief Engine for organizing and renaming ROM files
 * 
 * Features:
 * - Template-based renaming with No-Intro/Redump compliance
 * - Dry-run preview before execution
 * - Undo queue with database tracking
 * - Collision detection and resolution
 * - Safe move/copy with error handling
 * - Progress reporting via signals
 */
class OrganizeEngine : public QObject {
Q_OBJECT

public:
    explicit OrganizeEngine(Database &db, QObject *parent = nullptr);

    /**
     * @brief Set the naming template
     * @param templateStr Template string with variables
     */
    void setTemplate(const QString &templateStr);

    /**
     * @brief Set collision resolution strategy
     * @param strategy How to handle file name collisions
     */
    void setCollisionStrategy(CollisionStrategy strategy);

    /**
     * @brief Enable or disable dry-run mode
     * @param enabled If true, no files will be modified
     */
    void setDryRun(bool enabled);

    /**
     * @brief Organize a single file
     * @param fileId File ID from database
     * @param metadata Game metadata for naming
     * @param destinationDir Target directory
     * @param operation Move or Copy
     * @return Operation result
     */
    OrganizeResult organizeFile(int fileId,
                                const GameMetadata &metadata,
                                const QString &destinationDir,
                                FileOperation operation = FileOperation::Move);

    /**
     * @brief Organize multiple files with progress tracking
     * @param fileIds List of file IDs
     * @param metadataMap Map of file ID -> metadata
     * @param destinationDir Target directory
     * @param operation Move or Copy
     * @return List of results for each file
     */
    QList<OrganizeResult> organizeFiles(const QList<int> &fileIds,
                                       const QMap<int, GameMetadata> &metadataMap,
                                       const QString &destinationDir,
                                       FileOperation operation = FileOperation::Move);

    /**
     * @brief Undo the last operation
     * @param undoId Undo ID from operation result
     * @return True if undo succeeded
     */
    bool undoOperation(int undoId);

    /**
     * @brief Undo all operations in reverse order
     * @param limit Maximum number of operations to undo (0 = all)
     * @return Number of operations successfully undone
     */
    int undoAll(int limit = 0);

    /**
     * @brief Check if a path would cause a collision
     * @param path Destination path to check
     * @return True if file exists at path
     */
    static bool wouldCollide(const QString &path);

    /**
     * @brief Resolve collision by generating new path
     * @param path Original destination path
     * @param strategy Collision resolution strategy
     * @return Resolved path
     */
    static QString resolveCollision(const QString &path, CollisionStrategy strategy);

signals:
    void operationStarted(int fileId, const QString &oldPath, const QString &newPath);
    void operationCompleted(int fileId, bool success, const QString &error);
    void progressUpdate(int current, int total);
    void dryRunPreview(const QString &oldPath, const QString &newPath, FileOperation operation);

private:
    Database &m_database;
    TemplateEngine *m_templateEngine;
    QString m_template;
    CollisionStrategy m_collisionStrategy;
    bool m_dryRun;

    /**
     * @brief Execute file operation (move, copy, rename)
     * @param oldPath Source path
     * @param newPath Destination path
     * @param operation Operation type
     * @return True if successful
     */
    bool executeOperation(const QString &oldPath, const QString &newPath, FileOperation operation);

    /**
     * @brief Record operation in undo queue (database)
     * @param oldPath Original path
     * @param newPath New path
     * @param operation Operation type
     * @return Undo ID from database
     */
    int recordUndo(const QString &oldPath, const QString &newPath, FileOperation operation);

    /**
     * @brief Generate destination path from template
     * @param fileRecord File information
     * @param metadata Game metadata
     * @param destinationDir Target directory
     * @return Full destination path
     */
    QString generateDestinationPath(const FileRecord &fileRecord,
                                   const GameMetadata &metadata,
                                   const QString &destinationDir);
};

} // namespace Remus
