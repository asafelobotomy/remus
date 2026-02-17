#pragma once

#include "metadata_provider.h"
#include "rate_limiter.h"
#include "../core/constants/hash_algorithms.h"
#include "../core/constants/api.h"
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QUrlQuery>
#include <QMap>
#include <QSettings>

namespace Remus {

/**
 * @brief Hasheous metadata provider
 * 
 * Hasheous is a FREE hash-based ROM matching service that requires no authentication.
 * It proxies IGDB metadata and provides RetroAchievements IDs.
 * 
 * Benefits:
 * - No API key required
 * - Fast hash-based matching (MD5, SHA1, CRC32)
 * - Community-voted corrections
 * - Proxies IGDB data automatically via MetadataProxy
 * - Returns DAT match info (No-Intro, Redump, TOSEC)
 * 
 * API: https://hasheous.org/api/v1/
 * Swagger: https://hasheous.org/swagger/index.html
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
    GameMetadata getByHashes(const QString &crc32,
                             const QString &md5,
                             const QString &sha1,
                             const QString &system);
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

protected:
    QNetworkAccessManager *m_network;
    RateLimiter *m_rateLimiter;
    QString m_clientApiKey;
    QMap<int, QString> m_companyCache;

    // API base
    static constexpr const char* API_BASE = "https://hasheous.org/api/v1";
    
    /**
     * @brief Detect hash type from hash string length
     * @param hash The hash string
     * @return "md5", "sha1", "crc32", or empty if unknown
     */
    QString detectHashType(const QString &hash) const;
    
    /**
     * @brief Parse Hasheous hash lookup response to GameMetadata
     * @param json The JSON response from Hasheous
     * @return Populated GameMetadata structure
     */
    GameMetadata parseGameJson(const QJsonObject &json) const;
    
    /**
     * @brief Make GET request to Hasheous API
     * @param endpoint API endpoint
     * @param params URL query parameters
     * @return JSON response object
     */
    virtual QJsonObject makeRequest(const QString &endpoint, const QUrlQuery &params);
    
    /**
     * @brief Make POST request with JSON body to Hasheous API
     * @param endpoint API endpoint (e.g., "/Lookup/ByHash")
     * @param body JSON object to send as request body
     * @param params Optional URL query parameters
     * @return JSON response object
     */
    virtual QJsonObject makePostRequest(const QString &endpoint, const QJsonObject &body, const QUrlQuery &params = QUrlQuery());
    
    /**
     * @brief Fetch full IGDB metadata via Hasheous MetadataProxy
     * @param igdbId The IGDB game ID obtained from hash lookup
     * @return Populated GameMetadata with full IGDB data
     */
    virtual GameMetadata fetchIgdbMetadata(int igdbId);

private:
};

} // namespace Remus
