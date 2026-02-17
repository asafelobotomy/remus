#ifndef REMUS_LIBRARY_SERVICE_H
#define REMUS_LIBRARY_SERVICE_H

#include <functional>
#include <QString>
#include <QList>
#include <QVariantMap>
#include <QVariantList>

namespace Remus {

class Database;
class Scanner;
class SystemDetector;
struct ScanResult;
struct FileRecord;

/**
 * @brief Shared library scanning service (non-QObject, callback-based)
 *
 * Wraps Scanner + SystemDetector + Database file operations.
 * Usable by both GUI controllers and TUI screens.
 */
class LibraryService {
public:
    using ProgressCallback = std::function<void(int done, int total, const QString &path)>;
    using LogCallback      = std::function<void(const QString &message)>;

    LibraryService();
    ~LibraryService();

    /**
     * @brief Scan a directory, persist results to database
     * @param path       Directory to scan
     * @param db         Database to persist into (caller owns)
     * @param progressCb Progress callback (done, total, currentFile)
     * @param logCb      Optional log callback
     * @return Number of files inserted
     */
    int scan(const QString &path, Database *db,
             ProgressCallback progressCb = nullptr,
             LogCallback logCb = nullptr,
             int existingLibraryId = 0);

    /**
     * @brief Cancel a running scan
     */
    void cancelScan();

    /**
     * @brief Check if scan was cancelled
     */
    bool wasCancelled() const;

    /**
     * @brief Get library statistics
     * @param db Database to query
     * @return Map with totalFiles, totalSystems, etc.
     */
    QVariantMap getStats(Database *db) const;

    /**
     * @brief Get list of systems found in the library
     * @param db Database to query
     * @return List of system info maps
     */
    QVariantList getSystems(Database *db) const;

    /**
     * @brief Get file path for a given file ID
     */
    QString getFilePath(Database *db, int fileId) const;

    /**
     * @brief Remove a library and its files
     */
    bool removeLibrary(Database *db, int libraryId);

    /**
     * @brief Get all extensions the scanner recognizes
     */
    QStringList getAllExtensions() const;

private:
    /**
     * @brief Convert scan results to FileRecords and insert into DB
     */
    int persistScanResults(const QList<ScanResult> &results,
                           int libraryId, Database *db);

    Scanner        *m_scanner  = nullptr;
    SystemDetector *m_detector = nullptr;
};

} // namespace Remus

#endif // REMUS_LIBRARY_SERVICE_H
