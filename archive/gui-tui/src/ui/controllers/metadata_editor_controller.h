#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include "../../core/database.h"

namespace Remus {

/**
 * @brief Controller for viewing and editing game metadata
 * 
 * Allows users to view, edit, and override fetched metadata.
 * Exposed to QML as a context property.
 */
class MetadataEditorController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentGameId READ currentGameId WRITE setCurrentGameId NOTIFY currentGameIdChanged)
    Q_PROPERTY(QVariantMap currentGame READ currentGame NOTIFY currentGameChanged)
    Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY hasChangesChanged)

public:
    explicit MetadataEditorController(Database *db, QObject *parent = nullptr);

    int currentGameId() const { return m_currentGameId; }
    void setCurrentGameId(int id);
    
    QVariantMap currentGame() const { return m_currentGame; }
    bool hasChanges() const { return m_hasChanges; }

    /**
     * @brief Get game details by ID
     * @param gameId Game database ID
     * @return Map with all game metadata
     */
    Q_INVOKABLE QVariantMap getGameDetails(int gameId);

    /**
     * @brief Get metadata from all sources for a game
     * @param gameId Game database ID
     * @return List of metadata from different providers
     */
    Q_INVOKABLE QVariantList getMetadataSources(int gameId);

    /**
     * @brief Update a single metadata field
     * @param gameId Game database ID
     * @param field Field name to update
     * @param value New value
     * @return True if successful
     */
    Q_INVOKABLE bool updateField(int gameId, const QString &field, const QVariant &value);

    /**
     * @brief Save all pending changes
     */
    Q_INVOKABLE bool saveChanges();

    /**
     * @brief Discard pending changes
     */
    Q_INVOKABLE void discardChanges();

    /**
     * @brief Reset a field to the original fetched value
     */
    Q_INVOKABLE void resetField(int gameId, const QString &field);

    /**
     * @brief Reset all fields to original values
     */
    Q_INVOKABLE void resetAllFields(int gameId);

    /**
     * @brief Get list of files associated with a game
     */
    Q_INVOKABLE QVariantList getGameFiles(int gameId);

    /**
     * @brief Get match information for a file
     */
    Q_INVOKABLE QVariantMap getMatchInfo(int fileId);

    /**
     * @brief Search games by title
     * @param query Search query
     * @param limit Maximum results
     * @return List of matching games
     */
    Q_INVOKABLE QVariantList searchGames(const QString &query, int limit = 50);

    /**
     * @brief Get all games for a system
     */
    Q_INVOKABLE QVariantList getGamesBySystem(const QString &system, int limit = 100);

    /**
     * @brief Manually confirm/reject a match
     * @param matchId Match database ID
     * @param confirm True to confirm, false to reject
     */
    Q_INVOKABLE bool setMatchConfirmation(int matchId, bool confirm);

    /**
     * @brief Create a custom/manual match
     * @param fileId File database ID
     * @param title Game title
     * @param system System name
     * @return New game ID
     */
    Q_INVOKABLE int createManualMatch(int fileId, const QString &title, const QString &system);

signals:
    void currentGameIdChanged();
    void currentGameChanged();
    void hasChangesChanged();
    
    void gameSaved(int gameId);
    void gameUpdated(int gameId);
    void matchUpdated(int matchId);

private:
    void loadCurrentGame();
    void clearPendingChanges();

    Database *m_db;
    int m_currentGameId = -1;
    QVariantMap m_currentGame;
    QVariantMap m_pendingChanges;
    bool m_hasChanges = false;
};

} // namespace Remus
