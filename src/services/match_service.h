#ifndef REMUS_MATCH_SERVICE_H
#define REMUS_MATCH_SERVICE_H

#include <functional>
#include <QString>
#include <QList>
#include <QMap>
#include <atomic>

#include "../core/database.h"

namespace Remus {

class MatchingEngine;
struct Match;

/**
 * @brief Shared offline matching service (non-QObject, callback-based)
 *
 * Wraps MatchingEngine (DAT-based offline matching) + Database match operations.
 * Online matching via ProviderOrchestrator remains in the metadata layer.
 * Usable by both GUI controllers and TUI screens.
 */
class MatchService {
public:
    using ProgressCallback = std::function<void(int done, int total, const QString &path)>;
    using LogCallback      = std::function<void(const QString &message)>;

    /** Aggregate result of a matchAll() run. */
    struct MatchStats {
        int hashMatches = 0;
        int nameMatches = 0;
        int noMatch     = 0;
    };

    MatchService();
    ~MatchService();

    /**
     * @brief Match all files in the database (offline, DAT-based)
     * @param db         Database (reads files, writes matches)
     * @param progressCb Progress callback
     * @param logCb      Optional log callback
     * @param cancelled  Optional cancellation token
     * @return Summary statistics
     */
    MatchStats matchAll(Database *db,
                        ProgressCallback progressCb = nullptr,
                        LogCallback logCb = nullptr,
                        const std::atomic<bool> *cancelled = nullptr);

    /**
     * @brief Match a single file (offline, DAT-based)
     * @param db     Database
     * @param fileId File ID to match
     * @return The match result (confidence == 0 if no match)
     */
    Match matchFile(Database *db, int fileId);

    /**
     * @brief Confirm a match
     * @return True if the DB update succeeded
     */
    bool confirmMatch(Database *db, int fileId);

    /**
     * @brief Reject a match
     * @return True if the DB update succeeded
     */
    bool rejectMatch(Database *db, int fileId);

    /**
     * @brief Get all matches from the database
     */
    QMap<int, Database::MatchResult> getAllMatches(Database *db) const;

    /**
     * @brief Get the match for a specific file
     */
    Database::MatchResult getMatchForFile(Database *db, int fileId) const;

private:
    MatchingEngine *m_engine = nullptr;
};

} // namespace Remus

#endif // REMUS_MATCH_SERVICE_H
