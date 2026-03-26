#include "metadata_editor_controller.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace Remus {

MetadataEditorController::MetadataEditorController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

void MetadataEditorController::setCurrentGameId(int id)
{
    if (m_currentGameId != id) {
        // Save pending changes if switching games
        if (m_hasChanges) {
            saveChanges();
        }
        
        m_currentGameId = id;
        emit currentGameIdChanged();
        
        loadCurrentGame();
    }
}

void MetadataEditorController::loadCurrentGame()
{
    if (m_currentGameId > 0) {
        m_currentGame = getGameDetails(m_currentGameId);
    } else {
        m_currentGame.clear();
    }
    clearPendingChanges();
    emit currentGameChanged();
}

void MetadataEditorController::clearPendingChanges()
{
    m_pendingChanges.clear();
    m_hasChanges = false;
    emit hasChangesChanged();
}

QVariantMap MetadataEditorController::getGameDetails(int gameId)
{
    QVariantMap result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, g.system, g.region, g.year, g.publisher, g.developer,
               g.genre, g.description, g.players, g.created_at, g.updated_at
        FROM games g
        WHERE g.id = ?
    )");
    query.addBindValue(gameId);
    
    if (query.exec() && query.next()) {
        result["id"] = query.value("id");
        result["title"] = query.value("title");
        result["system"] = query.value("system");
        result["region"] = query.value("region");
        result["year"] = query.value("year");
        result["publisher"] = query.value("publisher");
        result["developer"] = query.value("developer");
        result["genre"] = query.value("genre");
        result["description"] = query.value("description");
        result["players"] = query.value("players");
        result["createdAt"] = query.value("created_at");
        result["updatedAt"] = query.value("updated_at");
        
        // Get file count
        QSqlQuery fileQuery(m_db->database());
        fileQuery.prepare("SELECT COUNT(*) FROM matches WHERE game_id = ?");
        fileQuery.addBindValue(gameId);
        if (fileQuery.exec() && fileQuery.next()) {
            result["fileCount"] = fileQuery.value(0).toInt();
        }
        
        // Get best match confidence
        QSqlQuery matchQuery(m_db->database());
        matchQuery.prepare("SELECT MAX(confidence) FROM matches WHERE game_id = ?");
        matchQuery.addBindValue(gameId);
        if (matchQuery.exec() && matchQuery.next()) {
            result["confidence"] = matchQuery.value(0).toInt();
        }
    }
    
    return result;
}

QVariantList MetadataEditorController::getMetadataSources(int gameId)
{
    QVariantList result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT ms.id, ms.provider_name, ms.provider_id, ms.title, ms.raw_data,
               ms.fetched_at, ms.priority
        FROM metadata_sources ms
        WHERE ms.game_id = ?
        ORDER BY ms.priority DESC
    )");
    query.addBindValue(gameId);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap source;
            source["id"] = query.value("id");
            source["providerName"] = query.value("provider_name");
            source["providerId"] = query.value("provider_id");
            source["title"] = query.value("title");
            source["fetchedAt"] = query.value("fetched_at");
            source["priority"] = query.value("priority");
            
            // Parse raw_data JSON
            QString rawData = query.value("raw_data").toString();
            if (!rawData.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(rawData.toUtf8());
                if (doc.isObject()) {
                    source["metadata"] = doc.object().toVariantMap();
                }
            }
            
            result.append(source);
        }
    }
    
    return result;
}

bool MetadataEditorController::updateField(int gameId, const QString &field, const QVariant &value)
{
    // Store in pending changes
    QString key = QString("%1_%2").arg(gameId).arg(field);
    m_pendingChanges[key] = value;
    
    if (!m_hasChanges) {
        m_hasChanges = true;
        emit hasChangesChanged();
    }
    
    // Update current game cache if it's the one being edited
    if (gameId == m_currentGameId) {
        m_currentGame[field] = value;
        emit currentGameChanged();
    }
    
    return true;
}

bool MetadataEditorController::saveChanges()
{
    if (!m_hasChanges) return true;
    
    QSqlDatabase db = m_db->database();
    db.transaction();
    
    // Group changes by game ID
    QHash<int, QVariantMap> changesByGame;
    
    for (auto it = m_pendingChanges.begin(); it != m_pendingChanges.end(); ++it) {
        QString key = it.key();
        QStringList parts = key.split('_');
        if (parts.size() >= 2) {
            int gameId = parts[0].toInt();
            QString field = parts.mid(1).join('_');
            changesByGame[gameId][field] = it.value();
        }
    }
    
    // Apply changes for each game
    for (auto it = changesByGame.begin(); it != changesByGame.end(); ++it) {
        int gameId = it.key();
        QVariantMap changes = it.value();
        
        // Build dynamic UPDATE query
        QStringList setClauses;
        QVariantList values;
        
        static const QStringList validFields = {
            "title", "system", "region", "year", "publisher", 
            "developer", "genre", "description", "players"
        };
        
        for (auto fieldIt = changes.begin(); fieldIt != changes.end(); ++fieldIt) {
            if (validFields.contains(fieldIt.key())) {
                setClauses.append(fieldIt.key() + " = ?");
                values.append(fieldIt.value());
            }
        }
        
        if (setClauses.isEmpty()) continue;
        
        setClauses.append("updated_at = CURRENT_TIMESTAMP");
        
        QString sql = QString("UPDATE games SET %1 WHERE id = ?").arg(setClauses.join(", "));
        QSqlQuery query(db);
        query.prepare(sql);
        
        for (const QVariant &value : values) {
            query.addBindValue(value);
        }
        query.addBindValue(gameId);
        
        if (!query.exec()) {
            qWarning() << "Failed to update game" << gameId << ":" << query.lastError().text();
            db.rollback();
            return false;
        }
        
        emit gameUpdated(gameId);
    }
    
    if (!db.commit()) {
        qWarning() << "Failed to commit changes:" << db.lastError().text();
        return false;
    }
    
    // Reload current game if it was updated
    if (changesByGame.contains(m_currentGameId)) {
        loadCurrentGame();
        emit gameSaved(m_currentGameId);
    }
    
    clearPendingChanges();
    return true;
}

void MetadataEditorController::discardChanges()
{
    clearPendingChanges();
    
    // Reload current game to revert UI
    if (m_currentGameId > 0) {
        loadCurrentGame();
    }
}

void MetadataEditorController::resetField(int gameId, const QString &field)
{
    // Remove from pending changes
    QString key = QString("%1_%2").arg(gameId).arg(field);
    m_pendingChanges.remove(key);
    
    if (m_pendingChanges.isEmpty() && m_hasChanges) {
        m_hasChanges = false;
        emit hasChangesChanged();
    }
    
    // Reload game to get original value
    if (gameId == m_currentGameId) {
        loadCurrentGame();
    }
}

void MetadataEditorController::resetAllFields(int gameId)
{
    // Remove all pending changes for this game
    QStringList keysToRemove;
    for (auto it = m_pendingChanges.begin(); it != m_pendingChanges.end(); ++it) {
        if (it.key().startsWith(QString::number(gameId) + "_")) {
            keysToRemove.append(it.key());
        }
    }
    
    for (const QString &key : keysToRemove) {
        m_pendingChanges.remove(key);
    }
    
    if (m_pendingChanges.isEmpty() && m_hasChanges) {
        m_hasChanges = false;
        emit hasChangesChanged();
    }
    
    if (gameId == m_currentGameId) {
        loadCurrentGame();
    }
}

QVariantList MetadataEditorController::getGameFiles(int gameId)
{
    QVariantList result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT f.id, f.filename, f.filepath, f.size, f.crc32, f.md5, f.sha1,
               m.confidence, m.match_type, m.user_confirmed
        FROM files f
        JOIN matches m ON f.id = m.file_id
        WHERE m.game_id = ?
        ORDER BY f.filename
    )");
    query.addBindValue(gameId);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap file;
            file["id"] = query.value("id");
            file["filename"] = query.value("filename");
            file["filepath"] = query.value("filepath");
            file["size"] = query.value("size");
            file["crc32"] = query.value("crc32");
            file["md5"] = query.value("md5");
            file["sha1"] = query.value("sha1");
            file["confidence"] = query.value("confidence");
            file["matchType"] = query.value("match_type");
            file["userConfirmed"] = query.value("user_confirmed").toBool();
            result.append(file);
        }
    }
    
    return result;
}

QVariantMap MetadataEditorController::getMatchInfo(int fileId)
{
    QVariantMap result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT m.id, m.file_id, m.game_id, m.confidence, m.match_type, 
               m.user_confirmed, m.created_at,
               g.title as game_title, g.system as game_system
        FROM matches m
        JOIN games g ON m.game_id = g.id
        WHERE m.file_id = ?
        ORDER BY m.confidence DESC
        LIMIT 1
    )");
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        result["id"] = query.value("id");
        result["fileId"] = query.value("file_id");
        result["gameId"] = query.value("game_id");
        result["confidence"] = query.value("confidence");
        result["matchType"] = query.value("match_type");
        result["userConfirmed"] = query.value("user_confirmed").toBool();
        result["createdAt"] = query.value("created_at");
        result["gameTitle"] = query.value("game_title");
        result["gameSystem"] = query.value("game_system");
    }
    
    return result;
}

QVariantList MetadataEditorController::searchGames(const QString &queryStr, int limit)
{
    QVariantList result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, g.system, g.region, g.year,
               (SELECT COUNT(*) FROM matches m WHERE m.game_id = g.id) as file_count
        FROM games g
        WHERE g.title LIKE ?
        ORDER BY g.title
        LIMIT ?
    )");
    query.addBindValue("%" + queryStr + "%");
    query.addBindValue(limit);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap game;
            game["id"] = query.value("id");
            game["title"] = query.value("title");
            game["system"] = query.value("system");
            game["region"] = query.value("region");
            game["year"] = query.value("year");
            game["fileCount"] = query.value("file_count");
            result.append(game);
        }
    }
    
    return result;
}

QVariantList MetadataEditorController::getGamesBySystem(const QString &system, int limit)
{
    QVariantList result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, g.system, g.region, g.year,
               (SELECT COUNT(*) FROM matches m WHERE m.game_id = g.id) as file_count
        FROM games g
        WHERE g.system = ?
        ORDER BY g.title
        LIMIT ?
    )");
    query.addBindValue(system);
    query.addBindValue(limit);
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap game;
            game["id"] = query.value("id");
            game["title"] = query.value("title");
            game["system"] = query.value("system");
            game["region"] = query.value("region");
            game["year"] = query.value("year");
            game["fileCount"] = query.value("file_count");
            result.append(game);
        }
    }
    
    return result;
}

bool MetadataEditorController::setMatchConfirmation(int matchId, bool confirm)
{
    QSqlQuery query(m_db->database());
    query.prepare("UPDATE matches SET user_confirmed = ?, confidence = CASE WHEN ? THEN 100 ELSE confidence END WHERE id = ?");
    query.addBindValue(confirm);
    query.addBindValue(confirm);
    query.addBindValue(matchId);
    
    if (query.exec()) {
        emit matchUpdated(matchId);
        return true;
    }
    
    qWarning() << "Failed to update match confirmation:" << query.lastError().text();
    return false;
}

int MetadataEditorController::createManualMatch(int fileId, const QString &title, const QString &system)
{
    QSqlDatabase db = m_db->database();
    db.transaction();
    
    // Create new game entry
    QSqlQuery gameQuery(db);
    gameQuery.prepare(R"(
        INSERT INTO games (title, system, created_at, updated_at)
        VALUES (?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
    )");
    gameQuery.addBindValue(title);
    gameQuery.addBindValue(system);
    
    if (!gameQuery.exec()) {
        qWarning() << "Failed to create game:" << gameQuery.lastError().text();
        db.rollback();
        return -1;
    }
    
    int gameId = gameQuery.lastInsertId().toInt();
    
    // Create match entry
    QSqlQuery matchQuery(db);
    matchQuery.prepare(R"(
        INSERT INTO matches (file_id, game_id, confidence, match_type, user_confirmed, created_at)
        VALUES (?, ?, 100, 'manual', 1, CURRENT_TIMESTAMP)
    )");
    matchQuery.addBindValue(fileId);
    matchQuery.addBindValue(gameId);
    
    if (!matchQuery.exec()) {
        qWarning() << "Failed to create match:" << matchQuery.lastError().text();
        db.rollback();
        return -1;
    }
    
    db.commit();
    return gameId;
}

} // namespace Remus
