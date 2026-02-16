#ifndef REMUS_IGDB_PROVIDER_H
#define REMUS_IGDB_PROVIDER_H

#include "metadata_provider.h"
#include "rate_limiter.h"
#include <QNetworkAccessManager>

namespace Remus {

/**
 * @brief IGDB (Internet Game Database) provider
 * 
 * Tertiary provider, requires Twitch API credentials.
 * Comprehensive database but more complex authentication.
 * 
 * API Docs: https://api-docs.igdb.com/
 */
class IGDBProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit IGDBProvider(QObject *parent = nullptr);

    QString name() const override { return "IGDB"; }
    bool requiresAuth() const override { return true; }

    void setCredentials(const QString &clientId, const QString &clientSecret) override;

    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system = QString(),
                                     const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    bool isAvailable() override;

private:
    struct ApiResponse {
        bool success = false;
        QString error;
        QByteArray data;
    };

    bool authenticate();
    ApiResponse makeRequest(const QString &endpoint, const QString &body);
    GameMetadata parseGameJson(const QJsonObject &game);
    QString mapSystemToIGDB(const QString &system);

    QNetworkAccessManager *m_networkManager;
    RateLimiter *m_rateLimiter;
    QString m_clientId;
    QString m_clientSecret;
    QString m_accessToken;
    QDateTime m_tokenExpiry;
    
    static constexpr int REQUEST_DELAY_MS = 250;  // 4 requests per second max
};

} // namespace Remus

#endif // REMUS_IGDB_PROVIDER_H
