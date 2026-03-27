#include "provider_orchestrator.h"
#include "filename_normalizer.h"
#include "hasheous_provider.h"
#include "metadata_cache.h"
#include <QDebug>
#include <algorithm>
#include "../core/constants/constants.h"
#include "../core/constants/match_methods.h"
#include "../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logMetadata)
#define qInfo() qCInfo(logMetadata)
#define qWarning() qCWarning(logMetadata)
#define qCritical() qCCritical(logMetadata)

namespace Remus {

using namespace Constants;

ProviderOrchestrator::ProviderOrchestrator(QObject *parent)
    : QObject(parent)
{
}

void ProviderOrchestrator::setCache(MetadataCache *cache)
{
    m_cache = cache;
}

void ProviderOrchestrator::addProvider(const QString &name, MetadataProvider *provider, int priority)
{
    if (!provider) {
        qWarning() << "Cannot add null provider:" << name;
        return;
    }
    
    provider->setParent(this);
    
    ProviderInfo info;
    info.provider = provider;
    info.priority = priority;
    info.enabled = true;
    info.supportsHash = detectHashSupport(name);
    
    m_providers[name] = info;
    
    qInfo() << "Added provider:" << name 
            << "| Priority:" << priority 
            << "| Hash support:" << (info.supportsHash ? "YES" : "NO");
}

void ProviderOrchestrator::removeProvider(const QString &name)
{
    if (m_providers.contains(name)) {
        ProviderInfo info = m_providers.take(name);
        delete info.provider;
        qInfo() << "Removed provider:" << name;
    }
}

void ProviderOrchestrator::setProviderEnabled(const QString &name, bool enabled)
{
    if (m_providers.contains(name)) {
        m_providers[name].enabled = enabled;
        qInfo() << "Provider" << name << (enabled ? "enabled" : "disabled");
    }
}

bool ProviderOrchestrator::detectHashSupport(const QString &name) const
{
    QStringList hashProviders = Constants::Providers::getHashSupportingProviders();
    hashProviders << QStringLiteral("retroachievements") 
                  << QStringLiteral("playmatch")
                  << QStringLiteral("localdatabase"); // Local DAT files support hash matching
    return hashProviders.contains(name.toLower());
}

bool ProviderOrchestrator::providerSupportsHash(const QString &name) const
{
    if (m_providers.contains(name)) {
        return m_providers[name].supportsHash;
    }
    return false;
}

QStringList ProviderOrchestrator::getSortedProviders(bool hashOnly) const
{
    QList<QPair<QString, int>> providerPriorities;
    
    for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
        const ProviderInfo &info = it.value();
        
        if (!info.enabled) {
            continue;
        }
        
        if (hashOnly && !info.supportsHash) {
            continue;
        }
        
        providerPriorities.append(qMakePair(it.key(), info.priority));
    }
    
    // Sort by priority (descending)
    std::sort(providerPriorities.begin(), providerPriorities.end(),
              [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                  return a.second > b.second;
              });
    
    QStringList result;
    for (const auto &pair : providerPriorities) {
        result.append(pair.first);
    }
    
    return result;
}

QStringList ProviderOrchestrator::getEnabledProviders() const
{
    return getSortedProviders(false);
}

GameMetadata ProviderOrchestrator::getByHashWithFallback(const QString &hash,
                                                         const QString &system,
                                                         const QString &crc32,
                                                         const QString &md5,
                                                         const QString &sha1)
{
    if (hash.isEmpty()) {
        qWarning() << "Cannot search by hash: hash is empty";
        return GameMetadata();
    }

    // Check cache first
    if (m_cache) {
        GameMetadata cached = m_cache->getByHash(hash, system);
        if (!cached.title.isEmpty()) {
            qInfo() << "Cache hit for hash:" << hash << "-" << cached.title;
            return cached;
        }
    }
    
    QStringList hashProviders = getSortedProviders(true);
    
    if (hashProviders.isEmpty()) {
        qWarning() << "No hash-capable providers enabled";
        emit allProvidersFailed();
        return GameMetadata();
    }
    
    for (const QString &providerName : hashProviders) {
        const ProviderInfo &info = m_providers[providerName];
        
        emit tryingProvider(providerName, MatchMethods::HASH);
        
        try {
            GameMetadata metadata;

            if (providerName.compare(Constants::Providers::HASHEOUS, Qt::CaseInsensitive) == 0) {
                // Prefer multi-hash path when available
                auto *hasheous = qobject_cast<HasheousProvider *>(info.provider);
                if (hasheous && (!crc32.isEmpty() || !md5.isEmpty() || !sha1.isEmpty())) {
                    metadata = hasheous->getByHashes(crc32, md5, sha1, system);
                } else {
                    metadata = info.provider->getByHash(hash, system);
                }
            } else {
                metadata = info.provider->getByHash(hash, system);
            }
            
            if (!metadata.title.isEmpty()) {
                qInfo() << "Hash match:" << metadata.title << "via" << providerName;
                emit providerSucceeded(providerName, MatchMethods::HASH);
                if (m_cache) {
                    m_cache->store(metadata, hash, system);
                }
                return metadata;
            } else {
                emit providerFailed(providerName, "No results");
            }
        } catch (const std::exception &e) {
            qWarning() << providerName << "error:" << e.what();
            emit providerFailed(providerName, e.what());
        }
    }
    
    qDebug() << "No hash match from" << hashProviders.size() << "providers for:" << hash;
    emit allProvidersFailed();
    return GameMetadata();
}

QList<SearchResult> ProviderOrchestrator::searchAllProviders(const QString &name, const QString &system)
{
    if (name.isEmpty()) {
        qWarning() << "Cannot search: name is empty";
        return QList<SearchResult>();
    }
    
    QStringList providers = getSortedProviders(false);
    QList<SearchResult> allResults;
    
    for (const QString &providerName : providers) {
        const ProviderInfo &info = m_providers[providerName];
        
        emit tryingProvider(providerName, MatchMethods::NAME);
        
        try {
            QList<SearchResult> results = info.provider->searchByName(name, system);
            
            if (!results.isEmpty()) {
                qDebug() << providerName << "found" << results.size() << "results for:" << name;
                
                // Tag results with provider name
                for (SearchResult &result : results) {
                    result.provider = providerName;
                }
                
                allResults.append(results);
                emit providerSucceeded(providerName, MatchMethods::NAME);
            } else {
                emit providerFailed(providerName, "No results");
            }
        } catch (const std::exception &e) {
            qWarning() << providerName << "search error:" << e.what();
            emit providerFailed(providerName, e.what());
        }
    }
    
    if (allResults.isEmpty()) {
        qDebug() << "No name matches from" << providers.size() << "providers for:" << name;
        emit allProvidersFailed();
    }
    
    return allResults;
}

GameMetadata ProviderOrchestrator::searchWithFallback(const QString &hash,
                                                      const QString &name,
                                                      const QString &system,
                                                      const QString &crc32,
                                                      const QString &md5,
                                                      const QString &sha1)
{
    // Strategy:
    // 1. Try hash-based providers first (if hash provided)
    // 2. Fall back to name-based search on all providers
    
    if (!hash.isEmpty()) {
        qInfo() << "Attempting hash-based search first for:" << name;
        GameMetadata metadata = getByHashWithFallback(hash, system, crc32, md5, sha1);
        
        if (!metadata.title.isEmpty()) {
            metadata.matchScore = 1.0f;  // Hash match = 100% confidence
            metadata.matchMethod = MatchMethods::HASH;
            return metadata;
        }
        
        qInfo() << "Hash-based search failed, falling back to name-based search";
    }
    
    // Try name-based search
    if (!name.isEmpty()) {
        // Normalize filename: remove extension, region tags, brackets, etc.
        QString normalizedName = Metadata::FilenameNormalizer::normalize(name);
        qInfo() << "Normalized name for search:" << name << "->" << normalizedName;
        
        if (normalizedName.isEmpty()) {
            qWarning() << "Normalized name is empty — skipping name-based search to avoid false positives";
        } else {

        QStringList providers = getSortedProviders(false);
        
        for (const QString &providerName : providers) {
            const ProviderInfo &info = m_providers[providerName];
            
            emit tryingProvider(providerName, MatchMethods::NAME);
            qInfo() << "Trying" << providerName << "with name:" << normalizedName;
            
            try {
                QList<SearchResult> results = info.provider->searchByName(normalizedName, system, QString());
                
                if (!results.isEmpty()) {
                    // Use the best match (first result)
                    const SearchResult &best = results.first();
                    qInfo() << "✓" << providerName << "found match:" << best.title
                            << "(score:" << best.matchScore << ")";

                    // Fetch full metadata
                    GameMetadata metadata = info.provider->getById(best.id);

                    if (!metadata.title.isEmpty()) {
                        metadata.matchScore = best.matchScore;
                        metadata.matchMethod = (best.matchScore >= 0.95f) ? MatchMethods::NAME : MatchMethods::FUZZY;
                        emit providerSucceeded(providerName, MatchMethods::NAME);
                        if (m_cache && !hash.isEmpty()) {
                            m_cache->store(metadata, hash, system);
                        }
                        return metadata;
                    }

                    const QString detailError = QStringLiteral("Detail fetch failed after search hit: %1 (%2)")
                                                    .arg(best.title, best.id);
                    qWarning() << "✗" << providerName << detailError;
                    emit providerFailed(providerName, detailError);
                    continue;
                }

                qInfo() << "✗" << providerName << "returned no results";
                emit providerFailed(providerName, "No results");
                
            } catch (const std::exception &e) {
                qWarning() << "✗" << providerName << "error:" << e.what();
                emit providerFailed(providerName, e.what());
            }
        }
        } // end normalizedName not empty
    }
    
    qWarning() << "All providers failed for:" << name;
    emit allProvidersFailed();
    return GameMetadata();
}

ArtworkUrls ProviderOrchestrator::getArtworkWithFallback(const QString &id, const QString &system, const QString &providerName)
{
    // Check cache first
    if (m_cache) {
        ArtworkUrls cached = m_cache->getArtwork(id);
        if (!cached.boxFront.isEmpty()) {
            qInfo() << "Cache hit for artwork ID:" << id;
            return cached;
        }
    }

    // If provider specified, try that first
    if (!providerName.isEmpty() && m_providers.contains(providerName)) {
        const ProviderInfo &info = m_providers[providerName];
        
        if (info.enabled) {
            qInfo() << "Fetching artwork from preferred provider:" << providerName;
            ArtworkUrls artwork = info.provider->getArtwork(id);
            if (!artwork.boxFront.isEmpty() && m_cache) {
                m_cache->storeArtwork(id, artwork);
            }
            return artwork;
        }
    }
    
    // Otherwise try all providers
    QStringList providers = getSortedProviders(false);
    
    for (const QString &name : providers) {
        const ProviderInfo &info = m_providers[name];
        
        qInfo() << "Trying artwork from:" << name;
        ArtworkUrls artwork = info.provider->getArtwork(id);
        
        if (!artwork.boxFront.isEmpty()) {
            qInfo() << "✓ Got artwork from:" << name;
            if (m_cache) {
                m_cache->storeArtwork(id, artwork);
            }
            return artwork;
        }
    }
    
    qWarning() << "No providers returned artwork for ID:" << id;
    return ArtworkUrls();
}

} // namespace Remus
