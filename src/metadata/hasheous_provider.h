#pragma once

#include "metadata_provider.h"
#include "rate_limiter.h"
#include "../core/constants/hash_algorithms.h"
#include <QNetworkAccessManager>
#include <QJsonObject>

namespace Remus {

/**
 * @brief Hasheous metadata provider
 * 
 * Hasheous is a FREE hash-based ROM matching service that requires no authentication.
 * It proxies IGDB metadata and provides RetroAchievements IDs.
 * 
 * Benefits:
 * - No API key required
 * - Fast hash-based matching (MD5, SHA1)
 * - Community-voted corrections
 * - Proxies IGDB data automatically
 * 
 * API: https://hasheous.org/api/v1/
 */
class HasheousProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit HasheousProvider(QObject *parent = nullptr);
    ~HasheousProvider() override = default;

    // MetadataProvider interface
    QString name() const override { return "Hasheous"; }
    bool requiresAuth() const override { return false; }
    
    QList<SearchResult> searchByName(const QString &title, 
                                     const QString &system = QString(),
                                     const QString &region = QString()) override;
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

private:
    QNetworkAccessManager *m_network;
    RateLimiter *m_rateLimiter;

    // API endpoints
    static constexpr const char* API_BASE = "https://hasheous.org/api/v1";
    
    /**
     * @brief Detect hash type from hash string length
     * @param hash The hash string
     * @return "md5" or "sha1" or empty if unknown
     */
    QString detectHashType(const QString &hash) const;
    
    /**
     * @brief Parse Hasheous API response to GameMetadata
     * @param json The JSON response from Hasheous
     * @return Populated GameMetadata structure
     */
    GameMetadata parseGameJson(const QJsonObject &json) const;
    
    /**
     * @brief Make HTTP request to Hasheous API
     * @param endpoint API endpoint (e.g., "/lookup")
     * @param params URL query parameters
     * @return JSON response object
     */
    QJsonObject makeRequest(const QString &endpoint, const QUrlQuery &params);
};

} // namespace Remus
