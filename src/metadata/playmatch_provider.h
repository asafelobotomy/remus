#pragma once

#include "http_metadata_provider.h"
#include "../core/constants/api.h"
#include "../core/constants/providers.h"
#include <QJsonObject>
#include <QUrlQuery>

namespace Remus {

/**
 * @brief PlayMatch metadata provider (RetroRealm hash→metadata bridge)
 *
 * Uses the public PlayMatch API to identify ROMs by hash and/or filename+size,
 * then fetches IGDB metadata via PlayMatch's IGDB proxy (no Twitch credentials
 * required on the public instance).
 *
 * API: https://playmatch.retrorealm.dev
 */
class PlayMatchProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit PlayMatchProvider(QObject *parent = nullptr);
    ~PlayMatchProvider() override = default;

    QString name() const override {
        return Constants::Providers::DISPLAY_PLAYMATCH;
    }
    bool requiresAuth() const override {
        return false;
    }

    QList<SearchResult> searchByName(
        const QString &title, const QString &system = QString(), const QString &region = QString()) override;
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata identifyBySignals(const QString &fileName, qint64 fileSize, const QString &crc32, const QString &md5,
        const QString &sha1, const QString &sha256, const QString &system = QString());
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

protected:
    virtual QJsonObject makeRequest(const QString &endpoint, const QUrlQuery &params);

    GameMetadata parseIdentifyResponse(const QJsonObject &json) const;
    GameMetadata parseIgdbGameJson(const QJsonObject &json, int igdbId) const;
    GameMetadata fetchIgdbMetadata(int igdbId);
    int extractIgdbId(const QJsonObject &identifyJson) const;
    static float matchScoreForType(const QString &gameMatchType);
    static QString matchMethodForType(const QString &gameMatchType);
};

} // namespace Remus
