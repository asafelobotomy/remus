#ifndef REMUS_THEGAMESDB_PROVIDER_H
#define REMUS_THEGAMESDB_PROVIDER_H

#include "metadata_provider.h"
#include "rate_limiter.h"
#include "../core/system_resolver.h"
#include <QNetworkAccessManager>

namespace Remus {

/**
 * @brief TheGamesDB.net metadata provider
 * 
 * Secondary provider, free API with no registration required.
 * No hash-based lookup (name-based only).
 * 
 * API Docs: https://api.thegamesdb.net/
 */
class TheGamesDBProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit TheGamesDBProvider(QObject *parent = nullptr);

    QString name() const override { return "TheGamesDB"; }
    bool requiresAuth() const override { return false; }

    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system = QString(),
                                     const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    /**
     * @brief Set API key (optional but recommended)
     */
    void setApiKey(const QString &apiKey);

    bool isAvailable() override;

private:
    struct ApiResponse {
        bool success = false;
        QString error;
        QByteArray data;
    };

    ApiResponse makeRequest(const QUrl &url);
    GameMetadata parseGameJson(const QJsonObject &game);
    QString mapSystemToTheGamesDB(const QString &system);

    QNetworkAccessManager *m_networkManager;
    RateLimiter *m_rateLimiter;
    QString m_apiKey;
    
    static constexpr int REQUEST_DELAY_MS = 1000;  // 1 second between requests
};

} // namespace Remus

#endif // REMUS_THEGAMESDB_PROVIDER_H
