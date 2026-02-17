#pragma once

#include "metadata_provider.h"
#include <QObject>
#include <QList>
#include <QMap>

namespace Remus {

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
    explicit ProviderOrchestrator(QObject *parent = nullptr);
    ~ProviderOrchestrator() override = default;

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
     * @return GameMetadata from first successful provider
     */
    GameMetadata searchWithFallback(const QString &hash,
                                    const QString &name,
                                    const QString &system,
                                    const QString &crc32 = QString(),
                                    const QString &md5 = QString(),
                                    const QString &sha1 = QString());
    
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
     * @brief Get list of enabled providers
     * @return List of provider names
     */
    QStringList getEnabledProviders() const;
    
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
    };
    
    QMap<QString, ProviderInfo> m_providers;
    
    /**
     * @brief Get providers sorted by priority (highest first)
     * @param hashOnly Only return hash-capable providers
     * @return Sorted list of provider names
     */
    QStringList getSortedProviders(bool hashOnly = false) const;
    
    /**
     * @brief Determine if provider supports hash matching
     * @param name Provider identifier
     * @return True if hash-capable
     */
    bool detectHashSupport(const QString &name) const;
};

} // namespace Remus
