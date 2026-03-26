#ifndef REMUS_THEGAMESDB_PROVIDER_H
#define REMUS_THEGAMESDB_PROVIDER_H

#include "http_metadata_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"

namespace Remus {

/**
 * @brief TheGamesDB.net metadata provider
 * 
 * Secondary provider, free API with no registration required.
 * No hash-based lookup (name-based only).
 * 
 * API Docs: https://api.thegamesdb.net/
 */
class TheGamesDBProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit TheGamesDBProvider(QObject *parent = nullptr);

    QString name() const override { return Constants::Providers::DISPLAY_THEGAMESDB; }
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
    ApiResponse makeRequest(const QUrl &url);
    GameMetadata parseGameJson(const QJsonObject &game);
    void loadRequestCount();
    void incrementRequestCount();

    QString m_apiKey;
    int m_monthlyRequestCount = 0;
    QString m_currentMonth;
};

} // namespace Remus

#endif // REMUS_THEGAMESDB_PROVIDER_H
