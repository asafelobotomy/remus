#ifndef REMUS_LIBRARY_SERVICE_H
#define REMUS_LIBRARY_SERVICE_H

#include <functional>
#include <memory>
#include <QString>
#include <QList>

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
    using LogCallback = std::function<void(const QString &message)>;

    LibraryService();
    ~LibraryService();

    /**
     * @brief Cancel a running scan
     */
    void cancelScan();

    /**
     * @brief Check if scan was cancelled
     */
    bool wasCancelled() const;

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

    /**
     * @brief Scan a directory for ROM files — filesystem only, no database access.
     *
     * Safe to call from a worker thread. Progress and log callbacks are invoked
     * on the calling thread; callers are responsible for thread-hopping if needed.
     *
     * @param path       Directory to scan
     * @param progressCb Progress callback (done, total, currentFile)
     * @param logCb      Optional log callback
     * @return Ordered list of scan results
     */
    QList<ScanResult> scanFilesystem(
        const QString &path, ProgressCallback progressCb = nullptr, LogCallback logCb = nullptr);

    /**
     * @brief Convert scan results to FileRecords and insert into DB.
     *
     * Must be called on the thread that owns the Database connection.
     */
    int persistScanResults(
        const QList<ScanResult> &results, int libraryId, Database *db, ProgressCallback progressCb = nullptr);

    std::unique_ptr<Scanner> m_scanner;
    std::unique_ptr<SystemDetector> m_detector;
};

} // namespace Remus

#endif // REMUS_LIBRARY_SERVICE_H
