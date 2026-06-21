#ifndef REMUS_STEAMGRIDDB_PROVIDER_H
#define REMUS_STEAMGRIDDB_PROVIDER_H

#include "http_metadata_provider.h"
#include "../core/constants/providers.h"

namespace Remus {

/**
 * @brief SteamGridDB metadata provider (artwork only)
 *
 * Fetches community grids, heroes, and logos. Does not populate text metadata.
 * Lookup IDs: "steam:{appId}", "sgdb:{gameId}", or a plain SGDB game id.
 *
 * API Docs: https://www.steamgriddb.com/api/v2
 */
class SteamGridDBProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit SteamGridDBProvider(QObject *parent = nullptr);

    QString name() const override {
        return Constants::Providers::DISPLAY_STEAMGRIDDB;
    }
    bool requiresAuth() const override {
        return true;
    }

    QList<SearchResult> searchByName(
        const QString &title, const QString &system = QString(), const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    void setApiKey(const QString &apiKey);

    bool isAvailable() override;

    /**
     * @brief Build the lookup token passed to getArtwork().
     *
     * Prefers an explicit steam external id, then steam:/sgdb: prefixes on @p id.
     */
    static QString resolveArtworkLookupId(const QString &id, const QMap<QString, QString> &externalIds);

private:
    struct LookupTarget {
        enum class Kind { None, SteamAppId, GameId };

        Kind kind = Kind::None;
        QString value;
    };

    ApiResponse makeRequest(const QUrl &url);
    LookupTarget parseLookupId(const QString &id) const;
    QUrl bestAssetUrl(const QString &resourcePath, const QUrlQuery &query);

    QString m_apiKey;
};

} // namespace Remus

#endif // REMUS_STEAMGRIDDB_PROVIDER_H
