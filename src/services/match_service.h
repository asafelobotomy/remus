#ifndef REMUS_MATCH_SERVICE_H
#define REMUS_MATCH_SERVICE_H

#include <QString>
#include <QList>
#include <QMap>

#include "../core/database.h"

namespace Remus {

/**
 * @brief Shared offline matching service (non-QObject, callback-based)
 *
 * Wraps MatchingEngine (DAT-based offline matching) + Database match operations.
 * Online matching via ProviderOrchestrator remains in the metadata layer.
 * Usable by both GUI controllers and TUI screens.
 */
class MatchService {
public:
    MatchService();
    ~MatchService();

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
};

} // namespace Remus

#endif // REMUS_MATCH_SERVICE_H
