#include "provider_orchestrator.h"
#include "filename_normalizer.h"
#include "hasheous_provider.h"
#include "local_database_provider.h"
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

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------
namespace {

/// Fill @p target's empty fields from @p source — never overwrites a set value.
void mergeMetadata(GameMetadata &target, const GameMetadata &source)
{
    if (target.title.isEmpty()       && !source.title.isEmpty())       target.title       = source.title;
    if (target.system.isEmpty()      && !source.system.isEmpty())      target.system      = source.system;
    if (target.region.isEmpty()      && !source.region.isEmpty())      target.region      = source.region;
    if (target.publisher.isEmpty()   && !source.publisher.isEmpty())   target.publisher   = source.publisher;
    if (target.developer.isEmpty()   && !source.developer.isEmpty())   target.developer   = source.developer;
    if (target.genres.isEmpty()      && !source.genres.isEmpty())      target.genres      = source.genres;
    if (target.releaseDate.isEmpty() && !source.releaseDate.isEmpty()) target.releaseDate = source.releaseDate;
    if (target.description.isEmpty() && !source.description.isEmpty()) target.description = source.description;
    if (target.players == 0          && source.players != 0)           target.players     = source.players;
    if (target.rating  == 0.0f       && source.rating  != 0.0f)        target.rating      = source.rating;
    if (target.ratingSource.isEmpty()  && !source.ratingSource.isEmpty())  target.ratingSource  = source.ratingSource;
    if (target.boxArtUrl.isEmpty()     && !source.boxArtUrl.isEmpty())     target.boxArtUrl     = source.boxArtUrl;
    if (target.screenshotUrls.isEmpty() && !source.screenshotUrls.isEmpty()) target.screenshotUrls = source.screenshotUrls;
    // Extend externalIds — add keys not already present
    for (auto it = source.externalIds.constBegin(); it != source.externalIds.constEnd(); ++it) {
        if (!target.externalIds.contains(it.key()))
            target.externalIds[it.key()] = it.value();
    }
    if (target.providerId.isEmpty() && !source.providerId.isEmpty())
        target.providerId = source.providerId;
    if (!target.fetchedAt.isValid() && source.fetchedAt.isValid())
        target.fetchedAt = source.fetchedAt;
    if (target.matchScore == 0.0f && source.matchScore > 0.0f)
        target.matchScore = source.matchScore;
    if (target.matchMethod.isEmpty() && !source.matchMethod.isEmpty())
        target.matchMethod = source.matchMethod;
}

/// True when all core metadata fields are populated — no further enrichment needed.
bool isSufficientlyEnriched(const GameMetadata &m)
{
    return !m.title.isEmpty()
        && !m.publisher.isEmpty()
        && !m.developer.isEmpty()
        && !m.releaseDate.isEmpty()
        && !m.genres.isEmpty()
        && m.players != 0;
}

} // anonymous namespace
// ---------------------------------------------------------------------------

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
    info.isLocal = detectLocalProvider(name);
    
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

bool ProviderOrchestrator::detectLocalProvider(const QString &name) const
{
    // Providers that work entirely offline (no network required)
    const QString lower = name.toLower();
    return lower == QStringLiteral("localdatabase")
        || lower == QStringLiteral("gametdb");
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

QStringList ProviderOrchestrator::getSortedLocalProviders() const
{
    QList<QPair<QString, int>> result;
    for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
        if (it.value().enabled && it.value().isLocal)
            result.append(qMakePair(it.key(), it.value().priority));
    }
    std::sort(result.begin(), result.end(),
              [](const QPair<QString,int> &a, const QPair<QString,int> &b){ return a.second > b.second; });
    QStringList names;
    for (const auto &p : result) names.append(p.first);
    return names;
}

QStringList ProviderOrchestrator::getSortedRemoteProviders() const
{
    QList<QPair<QString, int>> result;
    for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
        if (it.value().enabled && !it.value().isLocal)
            result.append(qMakePair(it.key(), it.value().priority));
    }
    std::sort(result.begin(), result.end(),
              [](const QPair<QString,int> &a, const QPair<QString,int> &b){ return a.second > b.second; });
    QStringList names;
    for (const auto &p : result) names.append(p.first);
    return names;
}

/**
 * Query a single provider and additively merge its result into @p accumulator.
 *
 * Strategy (first non-empty result wins within this provider):
 *   1. Hash match (if hash signals available and provider supports hash)
 *   2. Serial match via LocalDatabase (serial/disc-based entries)
 *   3. Name search (using confirmed accumulator title, or normalised filename)
 *
 * After merging, LocalDatabaseProvider also runs libretro CRC enrichment
 * to fill any field the DAT match didn't cover.
 */
void ProviderOrchestrator::queryProvider(GameMetadata &accumulator,
                                         const QString &providerName,
                                         const QString &hash,
                                         const QString &name,
                                         const QString &system,
                                         const QString &crc32,
                                         const QString &md5,
                                         const QString &sha1,
                                         const QString &serial)
{
    if (!m_providers.contains(providerName)) return;
    const ProviderInfo &info = m_providers[providerName];
    if (!info.enabled) return;

    GameMetadata result;

    // 1. Hash match
    if (!hash.isEmpty() && info.supportsHash) {
        emit tryingProvider(providerName, MatchMethods::HASH);
        try {
            if (providerName.compare(Constants::Providers::HASHEOUS, Qt::CaseInsensitive) == 0) {
                auto *hasheous = qobject_cast<HasheousProvider *>(info.provider);
                if (hasheous && (!crc32.isEmpty() || !md5.isEmpty() || !sha1.isEmpty()))
                    result = hasheous->getByHashes(crc32, md5, sha1, system);
                else
                    result = info.provider->getByHash(hash, system);
            } else {
                result = info.provider->getByHash(hash, system);
            }
            if (!result.title.isEmpty()) {
                result.matchScore  = 1.0f;
                result.matchMethod = MatchMethods::HASH;
                qInfo() << "Hash match via" << providerName << ":" << result.title;
                emit providerSucceeded(providerName, MatchMethods::HASH);
            } else {
                emit providerFailed(providerName, "No hash result");
            }
        } catch (const std::exception &e) {
            qWarning() << providerName << "hash error:" << e.what();
            emit providerFailed(providerName, e.what());
        }
    }

    // 2. Serial match (LocalDatabaseProvider only; for disc images without CRC in DAT)
    if (result.title.isEmpty() && !serial.isEmpty()) {
        auto *localDb = qobject_cast<LocalDatabaseProvider *>(info.provider);
        if (localDb) {
            ROMSignals romSignals;
            romSignals.crc32     = crc32;
            romSignals.md5       = md5;
            romSignals.sha1      = sha1;
            romSignals.filename  = name;
            romSignals.serial    = serial;
            QList<MultiSignalMatch> matches = localDb->matchROM(romSignals);
            if (!matches.isEmpty() && matches.first().serialMatch) {
                result = localDb->getMetadataForEntry(matches.first());
                if (!result.title.isEmpty()) {
                    result.matchScore  = matches.first().confidencePercent() / 100.0f;
                    result.matchMethod = QStringLiteral("serial");
                    qInfo() << "Serial match via" << providerName << ":" << result.title;
                    emit providerSucceeded(providerName, QStringLiteral("serial"));
                }
            }
        }
    }

    // 3. Name search — runs if no result yet from hash/serial
    //    Uses confirmed title from accumulator when available (enrichment mode);
    //    otherwise falls back to the normalised filename.
    if (result.title.isEmpty()) {
        const QString searchTerm = accumulator.title.isEmpty()
            ? Metadata::FilenameNormalizer::normalize(name)
            : accumulator.title;

        if (!searchTerm.isEmpty()) {
            emit tryingProvider(providerName, MatchMethods::NAME);
            try {
                QList<SearchResult> results = info.provider->searchByName(searchTerm, system);
                if (!results.isEmpty()) {
                    const SearchResult &best = results.first();
                    result = info.provider->getById(best.id);
                    if (!result.title.isEmpty()) {
                        result.matchScore  = best.matchScore;
                        result.matchMethod = (best.matchScore >= 0.95f) ? MatchMethods::NAME : MatchMethods::FUZZY;
                        qInfo() << "Name match via" << providerName << ":" << result.title
                                << "(score:" << best.matchScore << ")";
                        emit providerSucceeded(providerName, MatchMethods::NAME);
                    } else {
                        const QString detailError =
                            QStringLiteral("Detail fetch failed after search hit: %1 (%2)")
                                .arg(best.title, best.id);
                        qWarning() << "✗" << providerName << detailError;
                        emit providerFailed(providerName, detailError);
                    }
                } else {
                    emit providerFailed(providerName, "No name results");
                }
            } catch (const std::exception &e) {
                qWarning() << providerName << "name search error:" << e.what();
                emit providerFailed(providerName, e.what());
            }
        }
    }

    // Merge this provider's result into the accumulator (fill gaps only)
    if (!result.title.isEmpty() || !result.publisher.isEmpty() || !result.developer.isEmpty()) {
        mergeMetadata(accumulator, result);
    }

    // LocalDatabase bonus: libretro CRC enrichment fills genre/developer/publisher
    // for any entry that was matched by hash but has no inline DAT metadata
    {
        auto *localDb = qobject_cast<LocalDatabaseProvider *>(info.provider);
        if (localDb && !crc32.isEmpty())
            localDb->enrichFromLibretro(accumulator, crc32);
    }
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
                                                      const QString &sha1,
                                                      const QString &serial)
{
    // Local-first waterfall:
    //   Phase 1 — all local (offline) providers in priority order, additive merge
    //   Phase 2 — all remote providers in priority order, additive merge for gaps only
    //
    // Each provider is queried exactly once per phase.  Within each provider,
    // hash → serial (LocalDB) → name search is tried in sequence; the first
    // non-empty result is merged into the accumulator, filling only empty fields.

    // Check cache first
    if (!hash.isEmpty() && m_cache) {
        GameMetadata cached = m_cache->getByHash(hash, system);
        if (!cached.title.isEmpty()) {
            qInfo() << "Cache hit for hash:" << hash << "-" << cached.title;
            return cached;
        }
    }

    GameMetadata accumulator;

    // --- Phase 1: Local (offline) providers ---
    const QStringList localProviders = getSortedLocalProviders();
    if (!localProviders.isEmpty()) {
        qInfo() << "Phase 1 — querying" << localProviders.size() << "local provider(s):" << localProviders;
        for (const QString &pname : localProviders) {
            queryProvider(accumulator, pname, hash, name, system, crc32, md5, sha1, serial);
            if (isSufficientlyEnriched(accumulator)) {
                qInfo() << "Metadata complete after local provider:" << pname;
                break;
            }
        }
    }

    // --- Phase 2: Remote (online) providers — only for remaining gaps ---
    if (!isSufficientlyEnriched(accumulator)) {
        const QStringList remoteProviders = getSortedRemoteProviders();
        if (!remoteProviders.isEmpty()) {
            qInfo() << "Phase 2 — querying" << remoteProviders.size()
                    << "remote provider(s) for gaps:" << remoteProviders;
            for (const QString &pname : remoteProviders) {
                queryProvider(accumulator, pname, hash, name, system, crc32, md5, sha1, serial);
                if (isSufficientlyEnriched(accumulator)) {
                    qInfo() << "Metadata complete after remote provider:" << pname;
                    break;
                }
            }
        }
    }

    if (accumulator.title.isEmpty()) {
        qWarning() << "All providers failed for:" << name;
        emit allProvidersFailed();
        return GameMetadata();
    }

    // Store enriched result in cache
    if (!hash.isEmpty() && m_cache)
        m_cache->store(accumulator, hash, system);

    return accumulator;
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
