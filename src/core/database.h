#ifndef REMUS_DATABASE_H
#define REMUS_DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include "scanner.h"
#include "system_detector.h"
#include "system_resolver.h"

namespace Remus {

/**
 * @brief File record for database storage
 */
struct FileRecord {
    int id = 0;
    int libraryId = 0;
    QString originalPath;
    QString currentPath;
    QString filename;
    QString extension;
    qint64 fileSize = 0;
    bool isCompressed = false;
    QString archivePath;
    QString archiveInternalPath;
    int systemId = 0;
    QString crc32;
    QString md5;
    QString sha1;
    bool hashCalculated = false;
    bool isPrimary = true;
    int parentFileId = 0;
    bool isProcessed = false;
    QString processingStatus = "unprocessed";
    QDateTime lastModified;
    QDateTime scannedAt;
};

/**
 * @brief SQLite database manager
 */
class Database : public QObject {
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    /**
     * @brief Get the underlying QSqlDatabase connection
     * @return Reference to the database connection
     */
    QSqlDatabase& database() { return m_db; }
    /**
     * @brief Initialize database connection and create schema
     * @param dbPath Path to SQLite database file
     * @param connectionName Optional unique connection name (default: auto-generated)
     * @return True if successful
     */
    bool initialize(const QString &dbPath, const QString &connectionName = QString());

    /**
     * @brief Close database connection
     */
    void close();

    /**
     * @brief Create database schema (tables, indexes)
     * @return True if successful
     */
    bool createSchema();
    
    /**
     * @brief Populate default systems on new database
     * @return True if successful
     */
    bool populateDefaultSystems();
    
    /**
     * @brief Run database migrations for schema updates
     */
    void runMigrations();

    /**
     * @brief Insert or get library
     * @param path Library path
     * @param name Library name
     * @return Library ID
     */
    int insertLibrary(const QString &path, const QString &name = QString());

    /**
     * @brief Delete a library and all associated files
     * @param libraryId Library ID
     * @return True if successful
     */
    bool deleteLibrary(int libraryId);

    /**
     * @brief Get library path by ID
     * @param libraryId Library ID
     * @return Library path (empty if not found)
     */
    QString getLibraryPath(int libraryId);

    /**
     * @brief Delete all files for a library
     * @param libraryId Library ID
     * @return True if successful
     */
    bool deleteFilesForLibrary(int libraryId);

    /**
     * @brief Insert system info
     * @param system System information
     * @return System ID
     */
    int insertSystem(const SystemInfo &system);

    /**
     * @brief Get system ID by name
     * @param name System name
     * @return System ID (0 if not found)
     */
    int getSystemId(const QString &name);

    /**
     * @brief Get system display name by ID
     * @param systemId System ID
     * @return System display name (empty if not found)
     */
    QString getSystemDisplayName(int systemId);

    /**
     * @brief Insert file record
     * @param record File record to insert
     * @return File ID
     */
    int insertFile(const FileRecord &record);

    /**
     * @brief Update file hashes
     * @param fileId File ID
     * @param crc32 CRC32 hash
     * @param md5 MD5 hash
     * @param sha1 SHA1 hash
     * @return True if successful
     */
    bool updateFileHashes(int fileId, const QString &crc32, 
                          const QString &md5, const QString &sha1);

    /**
     * @brief Get files without calculated hashes
     * @return List of file records
     */
    QList<FileRecord> getFilesWithoutHashes();

    /**
     * @brief Get file count by system
     * @return Map of system name to count
     */
    QMap<QString, int> getFileCountBySystem();

    /**
     * @brief Get file by ID
     * @param fileId File ID
     * @return File record (id=0 if not found)
     */
    FileRecord getFileById(int fileId);

    /**
     * @brief Get all files from database (includes stale entries with non-existent paths)
     * @return List of all file records
     */
    QList<FileRecord> getAllFiles();

    /**
     * @brief Get files whose currentPath exists on disk
     * @return List of file records with valid paths only
     */
    QList<FileRecord> getExistingFiles();

    /**
     * @brief Get files by system name
     * @param systemName System name
     * @return List of file records
     */
    QList<FileRecord> getFilesBySystem(const QString &systemName);

    /**
     * @brief Get child files linked to a parent file (e.g. .bin tracks for a .cue)
     * @param parentId Parent file ID
     * @return List of child file records
     */
    QList<FileRecord> getFilesByParent(int parentId);

    /**
     * @brief Update file's current path (for organize/rename)
     * @param fileId File ID
     * @param newPath New current path
     * @return True if successful
     */
    bool updateFilePath(int fileId, const QString &newPath);
    
    /**
     * @brief Update file's original path (used when file is extracted from archive)
     * @param fileId File ID
     * @param newOriginalPath New original path
     * @return True if successful
     */
    bool updateFileOriginalPath(int fileId, const QString &newOriginalPath);
    
    /**
     * @brief Match info result from database query
     */
    struct MatchResult {
        int matchId = 0;
        int fileId = 0;
        int gameId = 0;
        QString matchMethod;
        float confidence = 0;
        bool isConfirmed = false;
        bool isRejected = false;
        QString gameTitle;
        QString publisher;
        QString developer;
        int releaseYear = 0;
        QString description;
        QString genre;
        QString players;
        QString region;
        float rating = 0.0f;
        float nameMatchScore = 0.0f; ///< Fuzzy name-match score (0.0–1.0) from MatchingEngine
    };
    
    /**
     * @brief Get match information for all files (for FileListModel)
     * @return Map of fileId -> MatchResult
     */
    QMap<int, MatchResult> getAllMatches();
    
    /**
     * @brief Get match for a specific file
     * @param fileId File ID
     * @return Match result (matchId=0 if no match)
     */
    MatchResult getMatchForFile(int fileId);
    
    /**
     * @brief Insert game metadata
     * @param title Game title
     * @param systemId System ID
     * @param region Region code
     * @param publisher Publisher name
     * @param developer Developer name
     * @param releaseDate Release date (ISO 8601)
     * @param description Game description
     * @param genres Genres (comma separated)
     * @param players Player count
     * @param rating Rating (0-10)
     * @return Game ID (0 if failed)
     */
    int insertGame(const QString &title, int systemId, const QString &region = QString(),
                   const QString &publisher = QString(), const QString &developer = QString(),
                   const QString &releaseDate = QString(), const QString &description = QString(),
                   const QString &genres = QString(), const QString &players = QString(),
                   float rating = 0.0f);

    /**
     * @brief Update an existing game record with enriched metadata.
     * @param gameId Game ID to update
     * @param publisher Publisher (empty → keep existing)
     * @param developer Developer (empty → keep existing)
     * @param releaseDate Release date (empty → keep existing)
     * @param description Description (empty → keep existing)
     * @param genres Genre string (empty → keep existing)
     * @param players Player count (empty → keep existing)
     * @param rating Rating 0-10 (negative → keep existing)
     * @return True if updated successfully
     */
    bool updateGame(int gameId,
                    const QString &publisher = QString(),
                    const QString &developer = QString(),
                    const QString &releaseDate = QString(),
                    const QString &description = QString(),
                    const QString &genres = QString(),
                    const QString &players = QString(),
                    float rating = -1.0f);
    
    /**
     * @brief Insert or update a metadata match
     * @param fileId File ID
     * @param gameId Game ID (metadata source ID)
     * @param confidence Match confidence (0-100)
     * @param matchMethod Match method (hash/exact/fuzzy/user_confirmed)
     * @return True if successful
     */
    bool insertMatch(int fileId, int gameId, float confidence, const QString &matchMethod,
                     float nameMatchScore = 0.0f);
    
    /**
     * @brief Confirm a match (user verification)
     * @param fileId File ID
     * @return True if successful
     */
    bool confirmMatch(int fileId);
    
    /**
     * @brief Reject a match (user verification)
     * @param fileId File ID  
     * @return True if successful
     */
    bool rejectMatch(int fileId);
    
    /**
     * @brief Get file path by ID
     * @param fileId File ID
     * @return File path (empty if not found)
     */
    QString getFilePath(int fileId);
    
    /**
     * @brief Mark a file as processed
     * @param fileId File ID
     * @param status Processing status ("processed", "failed", etc.)
     * @return True if successful
     */
    bool markFileProcessed(int fileId, const QString &status = "processed");
    
    /**
     * @brief Mark a file as unprocessed
     * @param fileId File ID  
     * @return True if successful
     */
    bool markFileUnprocessed(int fileId);
    
    /**
     * @brief Get all processed files
     * @return List of processed file records
     */
    QList<FileRecord> getProcessedFiles();
    
    /**
     * @brief Get all unprocessed files
     * @return List of unprocessed file records
     */
    QList<FileRecord> getUnprocessedFiles();

signals:
    void databaseError(const QString &error);

private:
    bool executeSqlFile(const QString &filePath);
    void logError(const QString &message);

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_connectionName;
};

} // namespace Remus

#endif // REMUS_DATABASE_H
