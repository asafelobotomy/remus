#include "match_service.h"

#include "../core/database.h"

namespace Remus {

MatchService::MatchService() = default;

MatchService::~MatchService() = default;

bool MatchService::confirmMatch(Database *db, int fileId) {
    if (!db)
        return false;
    return db->confirmMatch(fileId);
}

bool MatchService::rejectMatch(Database *db, int fileId) {
    if (!db)
        return false;
    return db->rejectMatch(fileId);
}

QMap<int, Database::MatchResult> MatchService::getAllMatches(Database *db) const {
    if (!db)
        return { };
    return db->getAllMatches();
}

Database::MatchResult MatchService::getMatchForFile(Database *db, int fileId) const {
    if (!db)
        return { };
    return db->getMatchForFile(fileId);
}

} // namespace Remus
