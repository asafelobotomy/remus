#include "database.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSet>

namespace Remus {

QMap<int, Database::MatchResult> Database::getAllMatches() {
    QMap<int, MatchResult> results;
    QSqlQuery query(m_db);

    // Join matches with games to get full info including metadata
    // SELECT ONLY THE BEST MATCH per file using a CTE:
    // 1. Prefer confirmed matches (is_confirmed = 1)
    // 2. Prefer matches with higher confidence
    // 3. Prefer manual matches over automatic
    // 4. Use highest ID (most recent) as tiebreaker
    query.prepare(R"(
        WITH best_matches AS (
            SELECT file_id, MAX(
                is_confirmed * 1000000 + 
                confidence * 1000 + 
                CASE match_method 
                    WHEN 'manual' THEN 300
                    WHEN 'hash' THEN 200
                    WHEN 'filename' THEN 100
                    ELSE 0
                END +
                (id * 0.001)
            ) as score
            FROM matches
            GROUP BY file_id
        )
        SELECT m.id, m.file_id, m.game_id, g.system_id, m.match_method, m.confidence, 
               m.is_confirmed, m.is_rejected,
               g.title, g.publisher, g.release_date, g.developer, g.description,
               g.genres, g.players, g.region, g.rating, m.name_match_score
        FROM matches m
        LEFT JOIN games g ON m.game_id = g.id
        INNER JOIN best_matches bm ON m.file_id = bm.file_id
        WHERE (
            m.is_confirmed * 1000000 + 
            m.confidence * 1000 + 
            CASE m.match_method 
                WHEN 'manual' THEN 300
                WHEN 'hash' THEN 200
                WHEN 'filename' THEN 100
                ELSE 0
            END +
            (m.id * 0.001)
        ) = bm.score
    )");

    if (!query.exec()) {
        logError("Failed to get all matches: " + query.lastError().text());
        return results;
    }

    while (query.next()) {
        MatchResult result;
        result.matchId = query.value(0).toInt();
        result.fileId = query.value(1).toInt();
        result.gameId = query.value(2).toInt();
        result.systemId = query.value(3).toInt();
        result.matchMethod = query.value(4).toString();
        result.confidence = query.value(5).toFloat();
        result.isConfirmed = query.value(6).toBool();
        result.isRejected = query.value(7).toBool();
        result.gameTitle = query.value(8).toString();
        result.publisher = query.value(9).toString();

        // Parse year from release_date (ISO format: YYYY-MM-DD)
        QString releaseDate = query.value(10).toString();
        if (!releaseDate.isEmpty()) {
            result.releaseYear = releaseDate.left(4).toInt();
            result.releaseDate = releaseDate;
        }

        // Populate remaining metadata fields
        result.developer = query.value(11).toString();
        result.description = query.value(12).toString();
        result.genre = query.value(13).toString();
        result.players = query.value(14).toString();
        result.region = query.value(15).toString();
        result.rating = query.value(16).toFloat();
        result.nameMatchScore = query.value(17).toFloat();

        results[result.fileId] = result;
    }

    qDebug() << "Database::getAllMatches() loaded" << results.count() << "matches";
    return results;
}

int Database::getUnconfirmedMatchCount() {
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM matches WHERE is_confirmed = 0 AND is_rejected = 0")))
        return 0;
    return q.next() ? q.value(0).toInt() : 0;
}

Database::MatchResult Database::getMatchForFile(int fileId) {
    MatchResult result;
    QSqlQuery query(m_db);

    query.prepare(R"(
        SELECT m.id, m.file_id, m.game_id, g.system_id, m.match_method, m.confidence, 
               m.is_confirmed, m.is_rejected,
               g.title, g.publisher, g.developer, g.release_date,
               g.description, g.genres, g.players, g.region, g.rating,
               m.name_match_score
        FROM matches m
        LEFT JOIN games g ON m.game_id = g.id
        WHERE m.file_id = ?
        ORDER BY m.confidence DESC
        LIMIT 1
    )");
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to get match for file: " + query.lastError().text());
        return result;
    }

    if (query.next()) {
        result.matchId = query.value(0).toInt();
        result.fileId = query.value(1).toInt();
        result.gameId = query.value(2).toInt();
        result.systemId = query.value(3).toInt();
        result.matchMethod = query.value(4).toString();
        result.confidence = query.value(5).toFloat();
        result.isConfirmed = query.value(6).toBool();
        result.isRejected = query.value(7).toBool();
        result.gameTitle = query.value(8).toString();
        result.publisher = query.value(9).toString();
        result.developer = query.value(10).toString();

        QString releaseDate = query.value(11).toString();
        if (!releaseDate.isEmpty()) {
            result.releaseYear = releaseDate.left(4).toInt();
            result.releaseDate = releaseDate;
        }

        result.description = query.value(12).toString();
        result.genre = query.value(13).toString();
        result.players = query.value(14).toString();
        result.region = query.value(15).toString();
        result.rating = query.value(16).toFloat();
        result.nameMatchScore = query.value(17).toFloat();
    }

    return result;
}

int Database::insertGame(const QString &title, int systemId, const QString &region, const QString &publisher,
    const QString &developer, const QString &releaseDate, const QString &description, const QString &genres,
    const QString &players, float rating) {
    QSqlQuery query(m_db);

    // Check if game already exists
    query.prepare("SELECT id FROM games WHERE title = ? AND system_id = ? AND region = ?");
    query.addBindValue(title);
    query.addBindValue(systemId);
    query.addBindValue(region);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    // Insert new game
    query.prepare("INSERT INTO games (title, system_id, region, publisher, developer, release_date, "
                  "description, genres, players, rating) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(title);
    query.addBindValue(systemId);
    query.addBindValue(region);
    query.addBindValue(publisher);
    query.addBindValue(developer);
    query.addBindValue(releaseDate);
    query.addBindValue(description);
    query.addBindValue(genres);
    query.addBindValue(players);
    query.addBindValue(rating);

    if (!query.exec()) {
        logError("Failed to insert game: " + query.lastError().text());
        return 0;
    }

    return query.lastInsertId().toInt();
}

bool Database::updateGame(int gameId, const QString &publisher, const QString &developer, const QString &releaseDate,
    const QString &description, const QString &genres, const QString &players, float rating, const QString &title,
    const QString &region) {
    static const QSet<QString> kAllowedUpdateColumns = { QStringLiteral("title"), QStringLiteral("region"),
        QStringLiteral("publisher"), QStringLiteral("developer"), QStringLiteral("release_date"),
        QStringLiteral("description"), QStringLiteral("genres"), QStringLiteral("players"), QStringLiteral("rating") };

    QStringList setClauses;
    QVariantList values;
    bool invalidColumn = false;

    auto addIfSet = [&](const QString &col, const QString &val) {
        if (!kAllowedUpdateColumns.contains(col)) {
            invalidColumn = true;
            return;
        }
        if (!val.isEmpty()) {
            setClauses << col + " = ?";
            values << val;
        }
    };

    addIfSet("title", title);
    addIfSet("region", region);
    addIfSet("publisher", publisher);
    addIfSet("developer", developer);
    addIfSet("release_date", releaseDate);
    addIfSet("description", description);
    addIfSet("genres", genres);
    addIfSet("players", players);

    if (invalidColumn) {
        logError("Failed to update game: rejected unknown update column");
        return false;
    }

    if (rating > 0.0f) {
        if (!kAllowedUpdateColumns.contains(QStringLiteral("rating"))) {
            logError("Failed to update game: rejected unknown update column");
            return false;
        }
        setClauses << "rating = ?";
        values << static_cast<double>(rating);
    }

    if (setClauses.isEmpty())
        return true; // nothing to update

    setClauses << "updated_at = CURRENT_TIMESTAMP";

    QSqlQuery query(m_db);
    query.prepare("UPDATE games SET " + setClauses.join(", ") + " WHERE id = ?");
    for (const auto &v : values)
        query.addBindValue(v);
    query.addBindValue(gameId);

    if (!query.exec()) {
        logError("Failed to update game: " + query.lastError().text());
        return false;
    }
    return true;
}

bool Database::insertMatch(int fileId, int gameId, float confidence, const QString &matchMethod, float nameMatchScore) {
    QSqlQuery query(m_db);

    // First check if match already exists
    query.prepare("SELECT id FROM matches WHERE file_id = ? AND game_id = ?");
    query.addBindValue(fileId);
    query.addBindValue(gameId);

    if (!query.exec()) {
        logError("Failed to check existing match: " + query.lastError().text());
        return false;
    }

    if (query.next()) {
        // Update existing match
        int matchId = query.value(0).toInt();
        query.prepare("UPDATE matches SET confidence = ?, match_method = ?, name_match_score = ?, "
                      "matched_at = CURRENT_TIMESTAMP WHERE id = ?");
        query.addBindValue(confidence);
        query.addBindValue(matchMethod);
        query.addBindValue(nameMatchScore);
        query.addBindValue(matchId);
    } else {
        // Insert new match
        query.prepare("INSERT INTO matches (file_id, game_id, confidence, match_method, name_match_score, matched_at) "
                      "VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)");
        query.addBindValue(fileId);
        query.addBindValue(gameId);
        query.addBindValue(confidence);
        query.addBindValue(matchMethod);
        query.addBindValue(nameMatchScore);
    }

    if (!query.exec()) {
        logError("Failed to insert/update match: " + query.lastError().text());
        return false;
    }

    return true;
}

bool Database::confirmMatch(int fileId) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE matches SET is_confirmed = 1, is_rejected = 0, confidence = 100 WHERE file_id = ?");
    query.addBindValue(fileId);

    if (!query.exec() || query.numRowsAffected() == 0) {
        logError("Failed to confirm match: " + query.lastError().text());
        return false;
    }

    // Propagate the confirmed game title into files.base_title so the queue
    // sidebar reflects the canonical name immediately after confirmation.
    QSqlQuery titleQ(m_db);
    titleQ.prepare("UPDATE files SET base_title = "
                   " (SELECT g.title FROM games g "
                   "  JOIN matches m ON m.game_id = g.id "
                   "  WHERE m.file_id = :fid AND m.is_confirmed = 1 "
                   "  LIMIT 1) "
                   "WHERE id = :fid");
    titleQ.bindValue(QStringLiteral(":fid"), fileId);
    if (!titleQ.exec()) {
        logError("Failed to update base_title on confirm: " + titleQ.lastError().text());
        // Non-fatal — match is already confirmed
    }

    reconcileDiscSetForConfirmedFile(fileId);

    return true;
}

bool Database::rejectMatch(int fileId) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE matches SET is_rejected = 1, is_confirmed = 0 WHERE file_id = ?");
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to reject match: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

} // namespace Remus
