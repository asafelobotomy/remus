#ifndef REMUS_IGDB_PROVIDER_H
#define REMUS_IGDB_PROVIDER_H

#include "http_metadata_provider.h"
#include "../core/constants/providers.h"

namespace Remus {

/**
 * @brief IGDB (Internet Game Database) provider
 * 
 * Tertiary provider, requires Twitch API credentials.
 * Comprehensive database but more complex authentication.
 * 
 * API Docs: https://api-docs.igdb.com/
 */
class IGDBProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit IGDBProvider(QObject *parent = nullptr);

    QString name() const override { return Constants::Providers::DISPLAY_IGDB; }
    bool requiresAuth() const override { return true; }

    void setCredentials(const QString &clientId, const QString &clientSecret) override;

    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system = QString(),
                                     const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    /**
     * @brief Bulk-fetch all IGDB games for a platform slug (for compendium enrichment).
     *
     * Uses Apicalypse: `where platforms.slug = "<slug>"` with pagination.
     * Returns at most @p limit entries starting from @p offset.
     * An empty list either means end-of-data or an authentication/network failure.
     */
    QList<GameMetadata> fetchGamesByPlatformSlug(const QString &platformSlug,
                                                  int offset = 0,
                                                  int limit = 500);

    bool isAvailable() override;

private:
    bool authenticate();
    ApiResponse makeRequest(const QString &endpoint, const QString &body);
    GameMetadata parseGameJson(const QJsonObject &game);
    QString mapSystemToIGDB(const QString &system);

    QString m_clientId;
    QString m_clientSecret;
    QString m_accessToken;
    QDateTime m_tokenExpiry;
};

} // namespace Remus

#endif // REMUS_IGDB_PROVIDER_H
