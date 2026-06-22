#pragma once

#include "http_metadata_provider.h"
#include "../core/constants/providers.h"

namespace Remus {

/**
 * @brief Wikidata SPARQL metadata provider
 *
 * Queries Wikidata's SPARQL endpoint for video game metadata.
 * No authentication required. CC0 licensed data.
 *
 * Fields: title, description, genre, developer, publisher, release date.
 * Name search only — no hash support.
 *
 * Priority 30 (lowest — supplementary enrichment source).
 */
class WikidataProvider : public HttpMetadataProvider {
    Q_OBJECT

public:
    explicit WikidataProvider(QObject *parent = nullptr);
    ~WikidataProvider() override = default;

    QString name() const override {
        return Constants::Providers::DISPLAY_WIKIDATA;
    }
    bool requiresAuth() const override {
        return false;
    }

    QList<SearchResult> searchByName(
        const QString &title, const QString &system = QString(), const QString &region = QString()) override;
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    /**
     * @brief Bulk-fetch Wikidata video games whose platform label matches @p systemDisplayName.
     *
     * Paginates with @p limit and @p offset. Returns an empty list at end-of-data or on error.
     */
    QList<GameMetadata> fetchGamesForPlatform(const QString &systemDisplayName, int limit = 500, int offset = 0);

private:
    static constexpr const char *SPARQL_ENDPOINT = "https://query.wikidata.org/sparql";
    static constexpr int REQUEST_TIMEOUT_MS = 15000;

    QJsonObject executeSparql(const QString &query);
    QString buildSearchQuery(const QString &title, const QString &system) const;
    QString buildPlatformBulkQuery(const QString &system, int limit, int offset) const;
    QString buildDetailQuery(const QString &entityId) const;
    GameMetadata parseDetailBindings(const QJsonArray &bindings, const QString &entityId) const;
    QString platformFilterClause(const QString &system) const;
};

} // namespace Remus
