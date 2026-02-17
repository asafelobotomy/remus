#ifndef REMUS_HASH_SERVICE_H
#define REMUS_HASH_SERVICE_H

#include <functional>
#include <QString>
#include <QList>

namespace Remus {

class Database;
class Hasher;
class SystemDetector;
struct FileRecord;
struct HashResult;

/**
 * @brief Shared hashing service (non-QObject, callback-based)
 *
 * Wraps Hasher + per-system header detection + DB hash persistence.
 * Supports archive-aware hashing (extracts compressed files to hash them).
 * Usable by both GUI controllers and TUI screens.
 */
class HashService {
public:
    using ProgressCallback = std::function<void(int done, int total, const QString &path)>;
    using LogCallback      = std::function<void(const QString &message)>;

    HashService();
    ~HashService();

    /**
     * @brief Hash all unhashed files in the database
     * @param db         Database (file records read + hash results written)
     * @param progressCb Progress callback (done, total, currentFile)
     * @param logCb      Optional log callback
     * @param cancelled  Optional pointer checked between files to allow cancellation
     * @return Number of files successfully hashed
     */
    int hashAll(Database *db,
                ProgressCallback progressCb = nullptr,
                LogCallback logCb = nullptr,
                const std::atomic<bool> *cancelled = nullptr);

    /**
     * @brief Hash a single file and persist the result
     * @param db     Database
     * @param fileId File ID to hash
     * @return True if hashing succeeded
     */
    bool hashFile(Database *db, int fileId);

    /**
     * @brief Hash a single FileRecord (archive-aware)
     *
     * Handles header stripping and archive extraction transparently.
     * Does NOT persist to database — caller decides what to do with the result.
     */
    HashResult hashRecord(const FileRecord &file);

private:
    Hasher *m_hasher = nullptr;
};

} // namespace Remus

#endif // REMUS_HASH_SERVICE_H
