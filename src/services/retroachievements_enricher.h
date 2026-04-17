#pragma once

#include "mod_catalog_provider.h"

#include <QMap>
#include <QString>

namespace Remus {

/**
 * @brief Enriches a mod catalog with data from the RetroAchievements API.
 *
 * Tier 2 of the three-tier architecture (remote, free API).
 * Requires a free RA account + web API key.
 *
 * If no API key is provided (neither via setApiKey() nor the
 * REMUS_RA_API_KEY / REMUS_RA_USERNAME env vars), all enrich
 * calls return gracefully with an empty result or skip message.
 *
 * Features:
 * - Build a hash→PatchUrl reverse index for a system
 * - Enrich existing ModEntry list with RA game IDs and PatchUrls
 * - Fetch system list for system ID mapping
 */
class RetroAchievementsEnricher {
public:
    struct EnrichResult {
        int enrichedCount = 0;
        int skippedCount = 0;
        QString error;
        bool skippedNoApiKey = false;
    };

    /**
     * @brief Set the API credentials explicitly.
     *
     * If not called, the enricher will try REMUS_RA_API_KEY and
     * REMUS_RA_USERNAME environment variables. If neither is available,
     * enrichment is silently skipped.
     */
    void setApiKey(const QString &username, const QString &apiKey);

    /**
     * @brief Check whether an API key is available (explicit or env).
     */
    bool hasApiKey() const;

    /**
     * @brief Resolve the effective API key (explicit > env > empty).
     */
    QString effectiveApiKey() const;
    QString effectiveUsername() const;

    /**
     * @brief Fetch the RA system list and return a map of system ID → name.
     */
    QMap<int, QString> fetchSystemList() const;

    /**
     * @brief Hash→PatchUrl entry from the RA API.
     */
    struct HashPatchEntry {
        QString md5;
        QString name;
        QStringList labels;
        QString patchUrl;
    };

    /**
     * @brief Fetch all hashes + PatchUrls for a given RA game ID.
     */
    QList<HashPatchEntry> fetchGameHashes(int gameId) const;

    /**
     * @brief Enrich a list of ModEntry objects with RA PatchUrl data.
     *
     * For each entry that has a baseMd5, queries the RA API to find
     * matching PatchUrl data and fills in patchUrl if empty.
     *
     * @param mods The entries to enrich (modified in-place)
     * @return EnrichResult with counts
     */
    EnrichResult enrichCatalog(QList<ModEntry> &mods) const;

    QString lastError() const { return m_lastError; }

private:
    QString m_username;
    QString m_apiKey;
    mutable QString m_lastError;

    QByteArray makeApiRequest(const QString &endpoint,
                              const QMap<QString, QString> &params) const;
};

} // namespace Remus
