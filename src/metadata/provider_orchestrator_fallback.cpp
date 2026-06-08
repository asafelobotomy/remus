#include "provider_orchestrator.h"

#include "filename_normalizer.h"
#include "hasheous_provider.h"
#include "metadata_cache.h"

#include <QDebug>

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

namespace {

    bool isSufficientlyEnriched(const GameMetadata &metadata, bool requireArtwork) {
        const bool hasCoreMetadata = !metadata.title.isEmpty() && !metadata.publisher.isEmpty()
            && !metadata.developer.isEmpty() && !metadata.releaseDate.isEmpty() && !metadata.genres.isEmpty()
            && !metadata.description.isEmpty() && metadata.players != 0;
        return hasCoreMetadata && (!requireArtwork || !metadata.boxArtUrl.isEmpty());
    }

} // namespace

void ProviderOrchestrator::queryProvider(GameMetadata &accumulator, const QString &providerName, const QString &hash,
    const QString &name, const QString &system, const QString &crc32, const QString &md5, const QString &sha1,
    const QString &serial) {
    if (!m_providers.contains(providerName)) {
        return;
    }
    const ProviderInfo &info = m_providers[providerName];
    if (!info.enabled) {
        return;
    }

    GameMetadata result;
    if (!hash.isEmpty() && info.supportsHash) {
        emit tryingProvider(providerName, MatchMethods::HASH);
        try {
            if (providerName.compare(Constants::Providers::HASHEOUS, Qt::CaseInsensitive) == 0) {
                auto *hasheous = qobject_cast<HasheousProvider *>(info.provider);
                if (hasheous && (!crc32.isEmpty() || !md5.isEmpty() || !sha1.isEmpty())) {
                    result = hasheous->getByHashes(crc32, md5, sha1, system);
                } else {
                    result = info.provider->getByHash(hash, system);
                }
            } else {
                result = info.provider->getByHash(hash, system);
            }

            if (!result.title.isEmpty()) {
                result.matchScore = 1.0f;
                result.matchMethod = MatchMethods::HASH;
                qInfo() << "Hash match via" << providerName << ":" << result.title;
                emit providerSucceeded(providerName, MatchMethods::HASH);
            } else {
                emit providerFailed(providerName, "No hash result");
            }
        } catch (const std::exception &error) {
            qWarning() << providerName << "hash error:" << error.what();
            emit providerFailed(providerName, error.what());
        }
    }

    if (result.title.isEmpty() && !serial.isEmpty()) {
        emit tryingProvider(providerName, QStringLiteral("serial"));
        try {
            const GameMetadata serialResult = info.provider->getBySerial(serial, system);
            if (!serialResult.title.isEmpty()) {
                result = serialResult;
                qInfo() << "Serial match via" << providerName << ":" << result.title;
                emit providerSucceeded(providerName, QStringLiteral("serial"));
            } else {
                emit providerFailed(providerName, "No serial result");
            }
        } catch (const std::exception &error) {
            qWarning() << providerName << "serial error:" << error.what();
            emit providerFailed(providerName, error.what());
        }
    }

    if (result.title.isEmpty()) {
        // Always normalize for name searches: strip region/revision parentheticals
        // (e.g. "Streets of Rage (World) (En,Ja)" → "Streets of Rage") so
        // name-only providers like TheGamesDB can match against their own titles.
        const QString rawTerm = accumulator.title.isEmpty() ? name : accumulator.title;
        const QString searchTerm = Metadata::FilenameNormalizer::normalize(rawTerm);
        if (!searchTerm.isEmpty()) {
            emit tryingProvider(providerName, MatchMethods::NAME);
            try {
                const QList<SearchResult> results = info.provider->searchByName(searchTerm, system);
                if (!results.isEmpty()) {
                    const SearchResult &best = results.first();
                    result = info.provider->getById(best.id);
                    if (!result.title.isEmpty()) {
                        result.matchScore = best.matchScore;
                        result.matchMethod = (best.matchScore >= 0.95f) ? MatchMethods::NAME : MatchMethods::FUZZY;
                        qInfo() << "Name match via" << providerName << ":" << result.title
                                << "(score:" << best.matchScore << ")";
                        emit providerSucceeded(providerName, MatchMethods::NAME);
                    } else {
                        const QString detailError
                            = QStringLiteral("Detail fetch failed after search hit: %1 (%2)").arg(best.title, best.id);
                        qWarning() << "✗" << providerName << detailError;
                        emit providerFailed(providerName, detailError);
                    }
                } else {
                    emit providerFailed(providerName, "No name results");
                }
            } catch (const std::exception &error) {
                qWarning() << providerName << "name search error:" << error.what();
                emit providerFailed(providerName, error.what());
            }
        }
    }

    if (!result.title.isEmpty() || !result.publisher.isEmpty() || !result.developer.isEmpty()) {
        mergeMetadata(accumulator, result);
    }
}

GameMetadata ProviderOrchestrator::getByHashWithFallback(
    const QString &hash, const QString &system, const QString &crc32, const QString &md5, const QString &sha1) {
    if (hash.isEmpty()) {
        qWarning() << "Cannot search by hash: hash is empty";
        return GameMetadata();
    }

    if (m_cache) {
        const GameMetadata cached = m_cache->getByHash(hash, system);
        if (cached.id == QLatin1String("__miss__")) {
            qInfo() << "Cached negative miss for hash:" << hash;
            return { };
        }
        if (!cached.title.isEmpty()) {
            qInfo() << "Cache hit for hash:" << hash << "-" << cached.title;
            return cached;
        }
    }

    const QStringList hashProviders = getSortedProviders(true);
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
            }

            emit providerFailed(providerName, "No results");
        } catch (const std::exception &error) {
            qWarning() << providerName << "error:" << error.what();
            emit providerFailed(providerName, error.what());
        }
    }

    qDebug() << "No hash match from" << hashProviders.size() << "providers for:" << hash;
    if (m_cache)
        m_cache->storeNegativeMiss(hash, system);
    emit allProvidersFailed();
    return GameMetadata();
}

QList<SearchResult> ProviderOrchestrator::searchAllProviders(const QString &name, const QString &system) {
    if (name.isEmpty()) {
        qWarning() << "Cannot search: name is empty";
        return { };
    }

    const QStringList providers = getSortedProviders(false);
    QList<SearchResult> allResults;
    for (const QString &providerName : providers) {
        const ProviderInfo &info = m_providers[providerName];
        emit tryingProvider(providerName, MatchMethods::NAME);
        try {
            QList<SearchResult> results = info.provider->searchByName(name, system);
            if (!results.isEmpty()) {
                qDebug() << providerName << "found" << results.size() << "results for:" << name;
                for (SearchResult &result : results) {
                    result.provider = providerName;
                }
                allResults.append(results);
                emit providerSucceeded(providerName, MatchMethods::NAME);
            } else {
                emit providerFailed(providerName, "No results");
            }
        } catch (const std::exception &error) {
            qWarning() << providerName << "search error:" << error.what();
            emit providerFailed(providerName, error.what());
        }
    }

    if (allResults.isEmpty()) {
        qDebug() << "No name matches from" << providers.size() << "providers for:" << name;
        emit allProvidersFailed();
    }

    return allResults;
}

GameMetadata ProviderOrchestrator::searchWithFallback(const QString &hash, const QString &name, const QString &system,
    const QString &crc32, const QString &md5, const QString &sha1, const QString &serial, bool requireArtwork) {
    GameMetadata accumulator;
    if (!hash.isEmpty() && m_cache) {
        const GameMetadata cached = m_cache->getByHash(hash, system);
        if (!cached.title.isEmpty()) {
            if (!requireArtwork || !cached.boxArtUrl.isEmpty()) {
                qInfo() << "Cache hit for hash:" << hash << "-" << cached.title;
                return cached;
            }

            qInfo() << "Cache hit for hash:" << hash << "-" << cached.title
                    << "(missing artwork; continuing provider lookup)";
            accumulator = cached;
        }
    }

    // A perfect hash match definitively identifies the ROM. No further provider
    // queries are needed for identity — only continue when the caller has
    // explicitly requested artwork and it is still missing.
    const auto identityResolved = [&](const GameMetadata &m) -> bool {
        if (m.matchScore >= 1.0f && m.matchMethod == QLatin1String(Constants::MatchMethods::HASH)) {
            return !(requireArtwork && m.boxArtUrl.isEmpty());
        }
        return false;
    };

    const QStringList localProviders = getSortedLocalProviders();
    for (const QString &providerName : localProviders) {
        queryProvider(accumulator, providerName, hash, name, system, crc32, md5, sha1, serial);
        if (isSufficientlyEnriched(accumulator, requireArtwork)) {
            qInfo() << "Metadata complete after local provider:" << providerName;
            break;
        }
        if (identityResolved(accumulator)) {
            qInfo() << "Hash match — identity resolved, skipping remaining providers for:" << name;
            break;
        }
    }

    if (!isSufficientlyEnriched(accumulator, requireArtwork) && !identityResolved(accumulator)) {
        const QStringList remoteProviders = getSortedRemoteProviders();
        for (const QString &providerName : remoteProviders) {
            queryProvider(accumulator, providerName, hash, name, system, crc32, md5, sha1, serial);
            if (isSufficientlyEnriched(accumulator, requireArtwork)) {
                qInfo() << "Metadata complete after remote provider:" << providerName;
                break;
            }
            if (identityResolved(accumulator)) {
                qInfo() << "Hash match — identity resolved, skipping remaining providers for:" << name;
                break;
            }
        }
    }

    if (accumulator.title.isEmpty()) {
        qWarning() << "All providers failed for:" << name;
        emit allProvidersFailed();
        return GameMetadata();
    }

    if (!hash.isEmpty() && m_cache) {
        m_cache->store(accumulator, hash, system);
    }
    return accumulator;
}

ArtworkUrls ProviderOrchestrator::getArtworkWithFallback(
    const QString &id, const QString &system, const QString &providerName) {
    Q_UNUSED(system);

    if (m_cache) {
        const ArtworkUrls cached = m_cache->getArtwork(id);
        if (!cached.boxFront.isEmpty()) {
            qInfo() << "Cache hit for artwork ID:" << id;
            return cached;
        }
    }

    if (!providerName.isEmpty() && m_providers.contains(providerName)) {
        const ProviderInfo &info = m_providers[providerName];
        if (info.enabled) {
            qInfo() << "Fetching artwork from preferred provider:" << providerName;
            const ArtworkUrls artwork = info.provider->getArtwork(id);
            if (!artwork.boxFront.isEmpty() && m_cache) {
                m_cache->storeArtwork(id, artwork);
            }
            return artwork;
        }
    }

    const QStringList providers = getSortedProviders(false);
    for (const QString &name : providers) {
        const ProviderInfo &info = m_providers[name];
        if (!info.enabled)
            continue;
        qInfo() << "Trying artwork from:" << name;
        const ArtworkUrls artwork = info.provider->getArtwork(id);
        if (!artwork.boxFront.isEmpty()) {
            qInfo() << "✓ Got artwork from:" << name;
            if (m_cache) {
                m_cache->storeArtwork(id, artwork);
            }
            return artwork;
        }
    }

    qWarning() << "No providers returned artwork for ID:" << id;
    return { };
}

} // namespace Remus