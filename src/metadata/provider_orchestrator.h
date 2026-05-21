#pragma once

#include "metadata_provider.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>

namespace Remus {

class MetadataCache;

/**
 * @brief Metadata provider orchestrator with intelligent fallback
 * 
 * Implements smart provider fallback strategy:
 * 1. ScreenScraper (hash) - highest confidence, requires auth
 * 2. Hasheous (hash) - FREE fallback, no auth required
 * 3. ScreenScraper (name) - authenticated name search
 * 4. TheGamesDB (name) - free name search
 * 5. IGDB (name) - richest metadata
 * 
 * Provider order can be customized via configuration.
 */
class ProviderOrchestrator : public QObject {
    Q_OBJECT

public:
    /** @brief Set of field name constants used to track enrichment gaps. */
    using FieldSet = QSet<QString>;

    explicit ProviderOrchestrator(QObject *parent = nullptr);
    ~ProviderOrchestrator() override = default;

    /**
     * @brief Set the metadata cache for result caching
     * @param cache Cache instance (not owned by orchestrator)
     */
    void setCache(MetadataCache *cache);

    /**
     * @brief Add a provider to the orchestrator
     * @param name Provider identifier (e.g., "screenscraper", "hasheous")
     * @param provider Provider instance (orchestrator takes ownership)
     * @param priority Higher numbers = higher priority (tried first)
     */
    void addProvider(const QString &name, MetadataProvider *provider, int priority = 0);
    
    /**
     * @brief Remove a provider from the orchestrator
     * @param name Provider identifier
     */
    void removeProvider(const QString &name);
    
    /**
     * @brief Set enabled state for a provider
     * @param name Provider identifier
     * @param enabled Whether provider should be used
     */
    void setProviderEnabled(const QString &name, bool enabled);
    
    /**
     * @brief Search for game with intelligent fallback
     * 
     * Tries hash-based providers first (if hash provided),
     * then falls back to name-based providers.
     * 
     * @param hash Optional file hash (CRC32/MD5/SHA1)
     * @param name Game name
     * @param system System identifier (e.g., "NES", "PlayStation")
     * @param crc32 Optional CRC32 hash
     * @param md5 Optional MD5 hash
     * @param sha1 Optional SHA1 hash
     * @param serial Optional disc serial (e.g., from IP.BIN header)
    * @param requireArtwork Continue fallback until boxArtUrl is populated
     * @return GameMetadata from first successful provider
     */
    GameMetadata searchWithFallback(const QString &hash,
                                    const QString &name,
                                    const QString &system,
                                    const QString &crc32 = QString(),
                                    const QString &md5 = QString(),
                                    const QString &sha1 = QString(),
                             const QString &serial = QString(),
                             bool requireArtwork = false);
    
    /**
     * @brief Get list of all search results from all providers
     * @param name Game name
     * @param system System identifier
     * @return Combined results from all enabled name-based providers
     */
    QList<SearchResult> searchAllProviders(const QString &name, const QString &system);
    
    /**
     * @brief Get metadata by hash from all hash-capable providers
     * @param hash File hash (CRC32/MD5/SHA1)
     * @param system System identifier
     * @return GameMetadata from first successful hash match
     */
    GameMetadata getByHashWithFallback(const QString &hash,
                                       const QString &system,
                                       const QString &crc32 = QString(),
                                       const QString &md5 = QString(),
                                       const QString &sha1 = QString());
    
    /**
     * @brief Get artwork with fallback
     * @param id Game ID
     * @param system System identifier
     * @param providerName Preferred provider name
     * @return ArtworkUrls from successful provider
     */
    ArtworkUrls getArtworkWithFallback(const QString &id, const QString &system, const QString &providerName = QString());

    /**
     * @brief Compute which metadata fields are still empty in @p m.
     */
    static FieldSet computeFieldGap(const GameMetadata &m);

    /**
     * @brief Fill @p missing fields by querying providers in priority order.
     *
     * Local (offline) providers are queried first, then remote providers.
     * Returns a merged copy of @p existing with any newly fetched data.
     */
    GameMetadata enrichMissingFields(const FieldSet &missing,
                                     const GameMetadata &existing,
                                     const QString &hash,
                                     const QString &name,
                                     const QString &system,
                                     const QString &crc32 = QString(),
                                     const QString &md5 = QString(),
                                     const QString &sha1 = QString(),
                                     const QString &serial = QString());

    /**
     * @brief Get list of enabled providers
     * @return List of provider names
     */
    QStringList getEnabledProviders() const;

    /**
     * @brief Return the raw provider pointer registered under @p name, or nullptr.
     */
    MetadataProvider* getProvider(const QString &name) const;
    
    /**
     * @brief Check if a provider supports hash-based matching
     * @param name Provider identifier
     * @return True if provider supports hash matching
     */
    bool providerSupportsHash(const QString &name) const;

signals:
    /**
     * @brief Emitted when trying a provider
     * @param providerName Name of provider being tried
     * @param method "hash" or "name"
     */
    void tryingProvider(const QString &providerName, const QString &method);
    
    /**
     * @brief Emitted when a provider succeeds
     * @param providerName Name of successful provider
     * @param method "hash" or "name"
     */
    void providerSucceeded(const QString &providerName, const QString &method);
    
    /**
     * @brief Emitted when a provider fails
     * @param providerName Name of failed provider
     * @param error Error message
     */
    void providerFailed(const QString &providerName, const QString &error);
    
    /**
     * @brief Emitted when all providers fail
     */
    void allProvidersFailed();

private:
    struct ProviderInfo {
        MetadataProvider *provider;
        int priority;
        bool enabled;
        bool supportsHash;
        bool isLocal;   ///< True for offline providers (localdatabase, gametdb)
    };
    
    QMap<QString, ProviderInfo> m_providers;
    MetadataCache *m_cache = nullptr;

    mutable bool m_sortCacheDirty = true;
    mutable QStringList m_cachedSortedAll;
    mutable QStringList m_cachedSortedHash;
    
    /**
     * @brief Get providers sorted by priority (highest first)
     * @param hashOnly Only return hash-capable providers
     * @return Sorted list of provider names
     */
    QStringList getSortedProviders(bool hashOnly = false) const;

    /** @brief Get local (offline) providers sorted by priority */
    QStringList getSortedLocalProviders() const;

    /** @brief Get remote (online) providers sorted by priority */
    QStringList getSortedRemoteProviders() const;

    /**
     * @brief Query a single provider and merge results into the accumulator
     *
     * Tries hash → serial (LocalDatabase only) → name search in sequence.
     * Fills only the fields that are still empty in @p accumulator (additive).
     */
    void queryProvider(GameMetadata &accumulator,
                       const QString &providerName,
                       const QString &hash,
                       const QString &name,
                       const QString &system,
                       const QString &crc32,
                       const QString &md5,
                       const QString &sha1,
                       const QString &serial);

    /**
     * @brief Determine if provider supports hash matching
     * @param name Provider identifier
     * @return True if hash-capable
     */
    bool detectHashSupport(const QString &name) const;

    /**
     * @brief Determine if provider is offline (no network required)
     * @param name Provider identifier
     * @return True for local/offline providers
     */
    bool detectLocalProvider(const QString &name) const;

    /**
     * @brief Merge non-empty fields from @p source into @p target (first-non-empty wins).
     *
     * Used by queryProvider(), searchWithFallback(), and enrichMissingFields().
     * Defined once here to avoid duplicate anonymous-namespace copies across TUs.
     */
    static void mergeMetadata(GameMetadata &target, const GameMetadata &source);
};

} // namespace Remus
