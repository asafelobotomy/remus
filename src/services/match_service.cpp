#include "match_service.h"

#include "../core/matching_engine.h"
#include "../core/database.h"

#include <QFileInfo>

namespace Remus {

MatchService::MatchService()
    : m_engine(new MatchingEngine())
{
}

MatchService::~MatchService()
{
    delete m_engine;
}

MatchService::MatchStats MatchService::matchAll(Database *db,
                                                 ProgressCallback progressCb,
                                                 LogCallback logCb,
                                                 const std::atomic<bool> *cancelled)
{
    MatchStats stats;
    if (!db) return stats;

    QList<FileRecord> files = db->getAllFiles();
    const int total = files.size();
    int done = 0;

    for (const FileRecord &fr : files) {
        if (cancelled && cancelled->load()) break;
        if (progressCb) progressCb(done, total, fr.currentPath);

        QFileInfo info(fr.currentPath);
        QString hash = fr.crc32.isEmpty() ? fr.md5 : fr.crc32;
        QString systemName = db->getSystemDisplayName(fr.systemId);

        Match match = m_engine->matchFile(fr.currentPath, hash,
                                          info.completeBaseName(), systemName);

        if (match.matchMethod == "hash") {
            stats.hashMatches++;
        } else if (match.matchMethod.contains("name")) {
            stats.nameMatches++;
        } else {
            stats.noMatch++;
        }

        // Persist match to DB
        if (match.confidence > 0) {
            int gameId = db->insertGame(match.title, fr.systemId, match.region);
            if (gameId > 0) {
                db->insertMatch(fr.id, gameId, match.confidence, match.matchMethod, match.nameMatchScore);
            }
        }

        ++done;
    }

    if (progressCb) progressCb(total, total, {});
    if (logCb) {
        logCb(QString("Matching complete: %1 hash, %2 name, %3 unmatched")
              .arg(stats.hashMatches).arg(stats.nameMatches).arg(stats.noMatch));
    }
    return stats;
}

Match MatchService::matchFile(Database *db, int fileId)
{
    Match empty;
    if (!db) return empty;

    FileRecord fr = db->getFileById(fileId);
    if (fr.id == 0) return empty;

    QFileInfo info(fr.currentPath);
    QString hash = fr.crc32.isEmpty() ? fr.md5 : fr.crc32;
    QString systemName = db->getSystemDisplayName(fr.systemId);

    Match match = m_engine->matchFile(fr.currentPath, hash,
                                      info.completeBaseName(), systemName);

    if (match.confidence > 0) {
        int gameId = db->insertGame(match.title, fr.systemId, match.region);
        if (gameId > 0) {
            db->insertMatch(fr.id, gameId, match.confidence, match.matchMethod, match.nameMatchScore);
        }
    }
    return match;
}

bool MatchService::confirmMatch(Database *db, int fileId)
{
    if (!db) return false;
    return db->confirmMatch(fileId);
}

bool MatchService::rejectMatch(Database *db, int fileId)
{
    if (!db) return false;
    return db->rejectMatch(fileId);
}

QMap<int, Database::MatchResult> MatchService::getAllMatches(Database *db) const
{
    if (!db) return {};
    return db->getAllMatches();
}

Database::MatchResult MatchService::getMatchForFile(Database *db, int fileId) const
{
    if (!db) return {};
    return db->getMatchForFile(fileId);
}

} // namespace Remus
