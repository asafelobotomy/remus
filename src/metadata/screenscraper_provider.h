#ifndef REMUS_SCREENSCRAPER_PROVIDER_H
#define REMUS_SCREENSCRAPER_PROVIDER_H

#include "metadata_provider.h"
#include "rate_limiter.h"
#include "../core/constants/hash_algorithms.h"
#include <QNetworkAccessManager>

namespace Remus {

/**
 * @brief ScreenScraper.fr metadata provider
 * 
 * Primary provider with extensive database and hash-based matching.
 * Requires user account for API access.
 * 
 * API Docs: https://www.screenscraper.fr/webapi.php
 */
class ScreenScraperProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit ScreenScraperProvider(QObject *parent = nullptr);

    QString name() const override { return "ScreenScraper"; }
    bool requiresAuth() const override { return true; }

    void setCredentials(const QString &username, const QString &password) override;

    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system = QString(),
                                     const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    /**
     * @brief Set developer credentials (devid, devpassword)
     * Required for API access
     */
    void setDeveloperCredentials(const QString &devId, const QString &devPassword);

    bool isAvailable() override;

private:
    struct ApiResponse {
        bool success = false;
        QString error;
        QByteArray data;
    };

    ApiResponse makeRequest(const QUrl &url);
    GameMetadata parseGameJson(const QByteArray &json);
    ArtworkUrls parseArtworkJson(const QByteArray &json) const;
    ArtworkUrls parseArtworkFromGameObject(const QJsonObject &game) const;
    QString pickArtworkUrl(const QJsonObject &media) const;
    QString mapSystemToScreenScraper(const QString &system);
    QString detectHashType(const QString &hash);

    QNetworkAccessManager *m_networkManager;
    RateLimiter *m_rateLimiter;
    QString m_devId;
    QString m_devPassword;
    QString m_softwareName = "Remus";
    
    static constexpr int MAX_REQUESTS_PER_DAY = 10000;  // Unregistered: 10k/day
    static constexpr int REQUEST_DELAY_MS = 2000;       // 2 seconds between requests
};

} // namespace Remus

#endif // REMUS_SCREENSCRAPER_PROVIDER_H
