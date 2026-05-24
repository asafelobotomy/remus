#pragma once

#include "http_metadata_provider.h"

namespace Remus {

/**
 * @brief ZXInfo (ZXDB) provider for ZX Spectrum game metadata.
 *
 * Uses the public ZXInfo API v3: https://api.zxinfo.dk/v3/
 * No authentication required.
 *
 * Rate limit: 250 ms per request (polite default; no documented limit).
 */
class ZXInfoProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit ZXInfoProvider(QObject *parent = nullptr);

    QString name() const override { return QStringLiteral("ZXInfo"); }
    bool requiresAuth() const override { return false; }
    bool isAvailable() override { return true; }

    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system = QString(),
                                     const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls  getArtwork(const QString &id) override;

    /**
     * @brief Search for a title and return full GameMetadata for the top matches.
     *
     * Parses metadata directly from the search response, avoiding extra
     * getById() round-trips.  Returns at most @p limit entries.
     */
    QList<GameMetadata> searchAndFetch(const QString &title, int limit = 5);

private:
    GameMetadata parseEntry(const QJsonObject &source) const;
    ApiResponse  makeRequest(const QUrl &url);
};

} // namespace Remus
