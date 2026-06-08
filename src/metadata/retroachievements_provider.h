#pragma once

#include "http_metadata_provider.h"
#include "../core/constants/providers.h"
#include <QJsonObject>

namespace Remus {

/**
 * @brief RetroAchievements metadata provider
 *
 * Hash-based game identification via the RetroAchievements API.
 * Requires a free account and web API key (registration at retroachievements.org).
 *
 * Flow:
 * 1. Hash lookup: resolve MD5 hash to RA game ID
 * 2. Game metadata: fetch title, system, genre, publisher, developer, images
 *
 * Priority 45 (between IGDB 40 and TheGamesDB 50).
 */
class RetroAchievementsProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit RetroAchievementsProvider(QObject *parent = nullptr);
    ~RetroAchievementsProvider() override = default;

    QString name() const override {
        return Constants::Providers::DISPLAY_RETROACHIEVEMENTS;
    }
    bool requiresAuth() const override {
        return true;
    }

    void setCredentials(const QString &username, const QString &apiKey) override;

    QList<SearchResult> searchByName(
        const QString &title, const QString &system = QString(), const QString &region = QString()) override;
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    /**
     * @brief One entry from a bulk game-list API response.
     *
     * Returned by fetchGameListBySystemId().  The @p md5Hashes list is
     * populated only when the API supports the @c h=1 parameter.
     */
    struct RAGameListEntry {
        int gameId = 0;
        QString title;
        int achievementCount = 0;
        QStringList md5Hashes; ///< lower-case MD5 hashes
    };

    /**
     * @brief Bulk-fetch all games for a RetroAchievements console.
     *
     * Calls @c API_GetGameList.php?i=raSystemId&h=1 to obtain the full game
     * list with MD5 hashes in a single request.  Entries without hashes are
     * omitted from the result.
     *
     * @param raSystemId  Numeric RetroAchievements console ID.
     * @return            Populated list, or empty on failure / no games.
     */
    QList<RAGameListEntry> fetchGameListBySystemId(int raSystemId);

private:
    static constexpr const char *API_BASE = "https://retroachievements.org/API";
    static constexpr const char *MEDIA_BASE = "https://media.retroachievements.org";
    static constexpr int REQUEST_TIMEOUT_MS = 10000;

    QString m_apiKey;

    int resolveHashToGameId(const QString &md5Hash);
    QJsonObject fetchGameJson(int gameId);
    GameMetadata parseGameJson(const QJsonObject &json, int gameId) const;
};

} // namespace Remus
