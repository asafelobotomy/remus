#ifndef REMUS_HASH_SERVICE_H
#define REMUS_HASH_SERVICE_H

#include <functional>
#include <memory>
#include <QString>
#include <QList>
#include <QSet>

#include "../core/hasher.h" // HashResult

namespace Remus {

class Database;
class Hasher;
class SystemDetector;
struct FileRecord;

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
    using LogCallback = std::function<void(const QString &message)>;

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
    int hashAll(Database *db, ProgressCallback progressCb = nullptr, LogCallback logCb = nullptr,
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

    /**
     * @brief Compute hashes for a list of files without touching the database.
     *
     * Safe to call from a worker thread — no DB access occurs here.
     * The caller is responsible for writing results to the DB.
     */
    struct HashBatchResult {
        int fileId = 0;
        QString filename;
        HashResult result;
        bool skipped = false;
        QString skipReason;
    };

    QList<HashBatchResult> computeHashes(const QList<FileRecord> &files, ProgressCallback progressCb = nullptr,
        const std::atomic<bool> *cancelled = nullptr);

    /**
     * @brief Fill @c chd_sha1 for .chd files that were hashed before header digest support.
     *
     * Uses @c chdman info only (no full container re-read). Skips archive-contained CHDs.
     * @return Number of files updated
     */
    int backfillChdSha1(Database *db, ProgressCallback progressCb = nullptr, LogCallback logCb = nullptr,
        const std::atomic<bool> *cancelled = nullptr, const QSet<int> *fileScopeIds = nullptr);

    /// Extract Hasheous/MAME header SHA1 for a standalone .chd on disk (empty when unavailable).
    static QString chdDiscSha1ForPath(const QString &chdPath);

    /**
     * @brief Fill @c rvz_sha1 for .rvz/.gcz files that were hashed before content digest support.
     */
    int backfillRvzSha1(Database *db, ProgressCallback progressCb = nullptr, LogCallback logCb = nullptr,
        const std::atomic<bool> *cancelled = nullptr, const QSet<int> *fileScopeIds = nullptr);

    /// Extract dolphin-tool disc content SHA1 for RVZ/GCZ on disk (empty when unavailable).
    static QString rvzDiscContentSha1ForPath(const QString &discPath);

private:
    std::unique_ptr<Hasher> m_hasher;
};

} // namespace Remus

#endif // REMUS_HASH_SERVICE_H
