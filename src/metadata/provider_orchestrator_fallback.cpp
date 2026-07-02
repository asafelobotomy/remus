#include "provider_orchestrator.h"

#include "compendium_provider.h"
#include "filename_normalizer.h"
#include "gametdb_provider.h"
#include "hasheous_provider.h"
#include "metadata_cache.h"
#include "playmatch_provider.h"
#include "steamgriddb_provider.h"

#include <QDebug>

#include "../core/constants/constants.h"
#include "../core/constants/provider_fields.h"
#include "../core/constants/match_methods.h"
#include "../core/logging_categories.h"
#include "../core/match_utils.h"
#include "../core/system_resolver.h"

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

    QString preferredHashTypeForSystem(const QString &system) {
        const int systemId = SystemResolver::systemIdByName(system);
        if (systemId != 0 && Constants::Systems::SYSTEMS.contains(systemId)) {
            return Constants::Systems::SYSTEMS.value(systemId).preferredHash.toLower();
        }
        return QStringLiteral("md5");
    }

    GameMetadata lookupByHashCascade(MetadataProvider *provider, const QString &system,
        const QString &preferredHashType, const QString &primaryHash, const QString &crc32, const QString &md5,
        const QString &sha1, qint64 fileSize = 0, const QString &sha256 = QString(),
        const QString &contentSha1 = QString()) {
        if (provider == nullptr) {
            return { };
        }

        auto *compendium = qobject_cast<CompendiumProvider *>(provider);
        auto *gametdb = qobject_cast<GameTDBProvider *>(provider);
        if (gametdb != nullptr) {
            return gametdb->getByHashes(crc32, md5, sha1, system, preferredHashType);
        }

        if (!contentSha1.isEmpty() && compendium != nullptr) {
            const GameMetadata contentMatch = compendium->getByHash(contentSha1, system, fileSize);
            if (!contentMatch.title.isEmpty())
                return contentMatch;
        }

        const QStringList candidates = orderedMatchHashValues(preferredHashType, crc32, md5, sha1, sha256);
        QSet<QString> tried;
        for (const QString &candidate : candidates) {
            tried.insert(candidate.toLower());
            const GameMetadata metadata = compendium != nullptr ? compendium->getByHash(candidate, system, fileSize)
                                                                : provider->getByHash(candidate, system);
            if (!metadata.title.isEmpty()) {
                return metadata;
            }
        }

        if (!primaryHash.isEmpty() && !tried.contains(primaryHash.toLower())) {
            return compendium != nullptr ? compendium->getByHash(primaryHash, system, fileSize)
                                         : provider->getByHash(primaryHash, system);
        }

        return { };
    }

    QString sha256FromPrimaryHash(const QString &primaryHash) {
        const QString trimmed = primaryHash.trimmed().toLower();
        if (trimmed.size() == 64)
            return trimmed;
        return QString();
    }

    bool hasHashLookupSignals(const QString &hash, const QString &crc32, const QString &md5, const QString &sha1,
        const QString &contentSha1 = QString()) {
        return !hash.isEmpty() || !crc32.isEmpty() || !md5.isEmpty() || !sha1.isEmpty() || !contentSha1.isEmpty()
            || !sha256FromPrimaryHash(hash).isEmpty();
    }

    bool hasheousEntrySignals(const HasheousHashEntry &entry) {
        return !entry.crc32.isEmpty() || !entry.md5.isEmpty() || !entry.sha1.isEmpty() || !entry.sha256.isEmpty()
            || !entry.chdSha1.isEmpty() || !entry.rvzSha1.isEmpty();
    }

    GameMetadata lookupHasheous(HasheousProvider *hasheous, const QString &hash, const QString &crc32,
        const QString &md5, const QString &sha1, const QString &system) {
        if (hasheous == nullptr)
            return { };

        const QString sha256 = sha256FromPrimaryHash(hash);
        if (!crc32.isEmpty() || !md5.isEmpty() || !sha1.isEmpty() || !sha256.isEmpty())
            return hasheous->getByHashes(crc32, md5, sha1, system, sha256);
        if (!hash.isEmpty())
            return hasheous->getByHash(hash, system);
        return { };
    }

    GameMetadata lookupHasheousEntries(
        HasheousProvider *hasheous, const QList<HasheousHashEntry> &entries, const QString &system) {
        if (hasheous == nullptr || entries.isEmpty())
            return { };

        QList<HasheousHashEntry> usable;
        usable.reserve(entries.size());
        for (const HasheousHashEntry &entry : entries) {
            if (hasheousEntrySignals(entry))
                usable.append(entry);
        }
        if (usable.isEmpty())
            return { };
        if (usable.size() == 1) {
            const HasheousHashEntry &entry = usable.first();
            if (!entry.chdSha1.isEmpty() || !entry.rvzSha1.isEmpty())
                return hasheous->getByHashEntries(usable, system);
            const QString primaryHash = !entry.sha256.isEmpty() ? entry.sha256
                : !entry.sha1.isEmpty()                         ? entry.sha1
                : !entry.md5.isEmpty()                          ? entry.md5
                                                                : entry.crc32;
            return lookupHasheous(hasheous, primaryHash, entry.crc32, entry.md5, entry.sha1, system);
        }
        return hasheous->getByHashEntries(usable, system);
    }

    GameMetadata lookupPlayMatch(PlayMatchProvider *playmatch, const QString &fileName, qint64 fileSize,
        const QString &primaryHash, const QString &crc32, const QString &md5, const QString &sha1,
        const QString &system, const QString &contentSha1 = QString()) {
        if (playmatch == nullptr || fileName.trimmed().isEmpty() || fileSize <= 0)
            return GameMetadata();

        const QString sha1ForLookup = !contentSha1.isEmpty() ? contentSha1 : sha1;
        return playmatch->identifyBySignals(
            fileName, fileSize, crc32, md5, sha1ForLookup, sha256FromPrimaryHash(primaryHash), system);
    }

    GameMetadata lookupRetroAchievements(
        MetadataProvider *provider, const GameMetadata &accumulator, const QString &raMd5, const QString &system) {
        if (provider == nullptr)
            return { };

        const QString raId = accumulator.externalIds.value(Constants::Providers::ExternalId::RETROACHIEVEMENTS);
        if (!raId.isEmpty()) {
            const GameMetadata byId = provider->getById(raId);
            if (!byId.title.isEmpty() || !byId.externalIds.isEmpty())
                return byId;
        }

        if (!raMd5.isEmpty())
            return provider->getByHash(raMd5, system);

        return { };
    }

} // namespace

void ProviderOrchestrator::queryProvider(GameMetadata &accumulator, const QString &providerName, const QString &hash,
    const QString &name, const QString &system, const QString &crc32, const QString &md5, const QString &sha1,
    const QString &serial, qint64 fileSize, const QString &raMd5, const QString &contentSha1) {
    if (!m_providers.contains(providerName)) {
        return;
    }
    const ProviderInfo &info = m_providers[providerName];
    if (!info.enabled) {
        return;
    }

    GameMetadata result;
    const bool hasAnyHash = hasHashLookupSignals(hash, crc32, md5, sha1, contentSha1);
    if (hasAnyHash && info.supportsHash) {
        emit tryingProvider(providerName, MatchMethods::HASH);
        try {
            const QString preferredHashType = preferredHashTypeForSystem(system);
            if (providerName.compare(Constants::Providers::HASHEOUS, Qt::CaseInsensitive) == 0) {
                auto *hasheous = qobject_cast<HasheousProvider *>(info.provider);
                if (!contentSha1.isEmpty()) {
                    HasheousHashEntry entry;
                    entry.crc32 = crc32;
                    entry.md5 = md5;
                    entry.sha1 = sha1;
                    if (contentSha1.size() == 64)
                        entry.sha256 = contentSha1;
                    else if (contentSha1.size() == 40)
                        entry.chdSha1 = contentSha1;
                    result = lookupHasheousEntries(hasheous, { entry }, system);
                } else if (hasheous != nullptr) {
                    result = lookupHasheous(hasheous, hash, crc32, md5, sha1, system);
                } else {
                    result = info.provider->getByHash(hash, system);
                }
            } else if (providerName.compare(Constants::Providers::PLAYMATCH, Qt::CaseInsensitive) == 0) {
                result = lookupPlayMatch(qobject_cast<PlayMatchProvider *>(info.provider), name, fileSize, hash, crc32,
                    md5, sha1, system, contentSha1);
            } else if (providerName.compare(Constants::Providers::RETROACHIEVEMENTS, Qt::CaseInsensitive) == 0) {
                result = lookupRetroAchievements(info.provider, accumulator, raMd5, system);
            } else if (providerName.compare(Constants::Providers::COMPENDIUM, Qt::CaseInsensitive) == 0
                || providerName.compare(Constants::Providers::GAMETDB, Qt::CaseInsensitive) == 0) {
                result = lookupByHashCascade(info.provider, system, preferredHashType, hash, crc32, md5, sha1, fileSize,
                    sha256FromPrimaryHash(hash), contentSha1);
            } else {
                result = info.provider->getByHash(hash, system);
            }

            if (!result.title.isEmpty()) {
                if (result.matchScore <= 0.0f) {
                    result.matchScore = 1.0f;
                    result.matchMethod = MatchMethods::HASH;
                } else if (result.matchMethod.isEmpty()) {
                    result.matchMethod = MatchMethods::HASH;
                }
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

    if (result.title.isEmpty() && !serial.isEmpty()
        && Constants::ProviderFields::providerSupportsSerialLookup(providerName)) {
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
        const bool canMultiSignal = providerName.compare(Constants::Providers::COMPENDIUM, Qt::CaseInsensitive) == 0
            && (!name.isEmpty() || fileSize > 0 || !serial.isEmpty() || !crc32.isEmpty() || !md5.isEmpty()
                || !sha1.isEmpty() || !contentSha1.isEmpty());
        if (canMultiSignal) {
            auto *compendium = qobject_cast<CompendiumProvider *>(info.provider);
            if (compendium != nullptr) {
                emit tryingProvider(providerName, QStringLiteral("multi_signal"));
                try {
                    ROMSignals romSignals;
                    romSignals.crc32 = crc32;
                    romSignals.md5 = md5;
                    romSignals.sha1 = sha1;
                    romSignals.sha256 = sha256FromPrimaryHash(hash);
                    romSignals.contentSha1 = contentSha1;
                    romSignals.filename = name;
                    romSignals.fileSize = fileSize;
                    romSignals.serial = serial;
                    const QList<CompendiumMultiSignalMatch> msMatches = compendium->matchROM(romSignals, system);
                    if (!msMatches.isEmpty()) {
                        result = compendium->metadataFromMatch(msMatches.first(), system);
                        if (!result.title.isEmpty()) {
                            qInfo() << "Multi-signal match via" << providerName << ":" << result.title << "("
                                    << msMatches.first().confidencePercent() << "% confidence)";
                            emit providerSucceeded(providerName, QStringLiteral("multi_signal"));
                        } else {
                            emit providerFailed(providerName, QStringLiteral("Multi-signal metadata empty"));
                        }
                    } else {
                        emit providerFailed(providerName, QStringLiteral("No multi-signal results"));
                    }
                } catch (const std::exception &error) {
                    qWarning() << providerName << "multi-signal error:" << error.what();
                    emit providerFailed(providerName, error.what());
                }
            }
        }
    }

    if (result.title.isEmpty()) {
        // Always normalize for name searches: strip region/revision parentheticals
        // (e.g. "Streets of Rage (World) (En,Ja)" → "Streets of Rage") so
        // name-only providers like TheGamesDB can match against their own titles.
        const QString rawTerm = accumulator.title.isEmpty() ? name : accumulator.title;
        const QString searchTerm = Metadata::FilenameNormalizer::normalize(rawTerm);
        if (!searchTerm.isEmpty() && Constants::ProviderFields::providerSupportsNameLookup(providerName)) {
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

    if (!result.title.isEmpty() || !result.publisher.isEmpty() || !result.developer.isEmpty()
        || !result.boxArtUrl.isEmpty() || !result.screenshotUrls.isEmpty() || result.rating != 0.0f
        || !result.externalIds.isEmpty()) {
        mergeMetadata(accumulator, result);
    }
}

GameMetadata ProviderOrchestrator::getByHashWithFallback(const QString &hash, const QString &system,
    const QString &crc32, const QString &md5, const QString &sha1, const QString &raMd5) {
    HasheousHashEntry entry;
    entry.crc32 = crc32;
    entry.md5 = md5;
    entry.sha1 = sha1;
    const QString sha256 = sha256FromPrimaryHash(hash);
    if (!sha256.isEmpty())
        entry.sha256 = sha256;
    return getByHashWithFallback({ entry }, system, raMd5, hash);
}

GameMetadata ProviderOrchestrator::getByHashWithFallback(
    const QList<HasheousHashEntry> &entries, const QString &system, const QString &raMd5, const QString &primaryHash) {
    if (entries.isEmpty()) {
        qWarning() << "Cannot search by hash: no digest entries available";
        return GameMetadata();
    }

    const HasheousHashEntry &first = entries.first();
    QString hash = primaryHash.trimmed();
    if (hash.isEmpty()) {
        if (!first.sha256.isEmpty())
            hash = first.sha256;
        else if (!first.sha1.isEmpty())
            hash = first.sha1;
        else if (!first.md5.isEmpty())
            hash = first.md5;
        else
            hash = first.crc32;
    }

    const QString crc32 = first.crc32;
    const QString md5 = first.md5;
    const QString sha1 = first.sha1;
    const QString contentSha1
        = !first.chdSha1.isEmpty() ? first.chdSha1.trimmed().toLower() : first.rvzSha1.trimmed().toLower();

    if (hash.isEmpty() && crc32.isEmpty() && md5.isEmpty() && sha1.isEmpty() && contentSha1.isEmpty()) {
        qWarning() << "Cannot search by hash: no digests available";
        return GameMetadata();
    }

    const QString cacheKey = !contentSha1.isEmpty() ? contentSha1
        : !hash.isEmpty()                           ? hash
        : !md5.isEmpty()                            ? md5
        : !sha1.isEmpty()                           ? sha1
                                                    : crc32;

    if (m_cache) {
        const GameMetadata cached = m_cache->getByHash(cacheKey, system);
        if (cached.id == QLatin1String("__miss__")) {
            qInfo() << "Cached negative miss for hash:" << cacheKey;
            return { };
        }
        if (!cached.title.isEmpty()) {
            qInfo() << "Cache hit for hash:" << cacheKey << "-" << cached.title;
            return cached;
        }
    }

    const QString preferredHashType = preferredHashTypeForSystem(system);
    const QStringList hashProviders = getSortedProviders(true);
    if (hashProviders.isEmpty()) {
        qWarning() << "No hash-capable providers enabled";
        emit allProvidersFailed();
        return GameMetadata();
    }

    const bool multiHasheousPayload = entries.size() > 1;

    for (const QString &providerName : hashProviders) {
        const ProviderInfo &info = m_providers[providerName];
        emit tryingProvider(providerName, MatchMethods::HASH);

        try {
            GameMetadata metadata;
            if (providerName.compare(Constants::Providers::HASHEOUS, Qt::CaseInsensitive) == 0) {
                auto *hasheous = qobject_cast<HasheousProvider *>(info.provider);
                if (hasheous != nullptr) {
                    metadata = (multiHasheousPayload || !contentSha1.isEmpty())
                        ? lookupHasheousEntries(hasheous, entries, system)
                        : lookupHasheous(hasheous, hash, crc32, md5, sha1, system);
                } else {
                    metadata = info.provider->getByHash(hash, system);
                }
            } else if (providerName.compare(Constants::Providers::PLAYMATCH, Qt::CaseInsensitive) == 0) {
                emit providerFailed(providerName, QStringLiteral("Skipped (needs fileName and fileSize)"));
                continue;
            } else if (providerName.compare(Constants::Providers::RETROACHIEVEMENTS, Qt::CaseInsensitive) == 0) {
                if (!raMd5.isEmpty()) {
                    metadata = info.provider->getByHash(raMd5, system);
                }
            } else if (providerName.compare(Constants::Providers::COMPENDIUM, Qt::CaseInsensitive) == 0
                || providerName.compare(Constants::Providers::GAMETDB, Qt::CaseInsensitive) == 0) {
                metadata = lookupByHashCascade(info.provider, system, preferredHashType, hash, crc32, md5, sha1, 0,
                    sha256FromPrimaryHash(hash), contentSha1);
            } else {
                metadata = info.provider->getByHash(hash, system);
            }

            if (!metadata.title.isEmpty()) {
                qInfo() << "Hash match:" << metadata.title << "via" << providerName;
                emit providerSucceeded(providerName, MatchMethods::HASH);
                if (m_cache) {
                    m_cache->store(metadata, cacheKey, system);
                }
                return metadata;
            }

            emit providerFailed(providerName, "No results");
        } catch (const std::exception &error) {
            qWarning() << providerName << "error:" << error.what();
            emit providerFailed(providerName, error.what());
        }
    }

    qDebug() << "No hash match from" << hashProviders.size() << "providers for:" << cacheKey;
    if (m_cache)
        m_cache->storeNegativeMiss(cacheKey, system);
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

QList<SearchResult> ProviderOrchestrator::searchProvider(
    const QString &providerName, const QString &name, const QString &system) {
    if (name.isEmpty()) {
        qWarning() << "Cannot search: name is empty";
        return { };
    }

    if (providerName.isEmpty())
        return searchAllProviders(name, system);

    const ProviderInfo *info = m_providers.contains(providerName) ? &m_providers[providerName] : nullptr;
    if (info == nullptr || !info->enabled || info->provider == nullptr) {
        qWarning() << "Provider not available:" << providerName;
        return { };
    }

    emit tryingProvider(providerName, MatchMethods::NAME);
    try {
        QList<SearchResult> results = info->provider->searchByName(name, system);
        for (SearchResult &result : results)
            result.provider = providerName;
        if (results.isEmpty()) {
            emit providerFailed(providerName, QStringLiteral("No results"));
        } else {
            emit providerSucceeded(providerName, MatchMethods::NAME);
        }
        return results;
    } catch (const std::exception &error) {
        qWarning() << providerName << "search error:" << error.what();
        emit providerFailed(providerName, error.what());
        return { };
    }
}

GameMetadata ProviderOrchestrator::fetchProviderMetadata(
    const QString &providerName, const QString &id, const QString &system) {
    if (providerName.isEmpty() || id.isEmpty())
        return { };

    const ProviderInfo *info = m_providers.contains(providerName) ? &m_providers[providerName] : nullptr;
    if (info == nullptr || !info->enabled || info->provider == nullptr)
        return { };

    GameMetadata metadata = info->provider->getById(id);
    if (metadata.title.isEmpty())
        return { };

    if (metadata.system.isEmpty())
        metadata.system = system;
    if (metadata.providerId.isEmpty())
        metadata.providerId = providerName;
    return metadata;
}

GameMetadata ProviderOrchestrator::getHashFromProvider(const QString &providerName, const QString &hash,
    const QString &system, const QString &crc32, const QString &md5, const QString &sha1, const QString &raMd5) {
    if (providerName.isEmpty() || !hasHashLookupSignals(hash, crc32, md5, sha1)) {
        return { };
    }

    const ProviderInfo *info = m_providers.contains(providerName) ? &m_providers[providerName] : nullptr;
    if (info == nullptr || !info->enabled || !info->supportsHash || info->provider == nullptr)
        return { };

    emit tryingProvider(providerName, MatchMethods::HASH);
    GameMetadata metadata;
    if (providerName.compare(Constants::Providers::HASHEOUS, Qt::CaseInsensitive) == 0) {
        auto *hasheous = qobject_cast<HasheousProvider *>(info->provider);
        if (hasheous != nullptr) {
            metadata = lookupHasheous(hasheous, hash, crc32, md5, sha1, system);
        } else {
            metadata = info->provider->getByHash(hash, system);
        }
    } else if (providerName.compare(Constants::Providers::PLAYMATCH, Qt::CaseInsensitive) == 0) {
        emit providerFailed(providerName, QStringLiteral("Requires fileName and fileSize"));
        return { };
    } else if (providerName.compare(Constants::Providers::RETROACHIEVEMENTS, Qt::CaseInsensitive) == 0) {
        if (!raMd5.isEmpty()) {
            metadata = info->provider->getByHash(raMd5, system);
        }
    } else if (providerName.compare(Constants::Providers::COMPENDIUM, Qt::CaseInsensitive) == 0
        || providerName.compare(Constants::Providers::GAMETDB, Qt::CaseInsensitive) == 0) {
        metadata = lookupByHashCascade(info->provider, system, preferredHashTypeForSystem(system), hash, crc32, md5,
            sha1, 0, sha256FromPrimaryHash(hash));
    } else if (!hash.isEmpty()) {
        metadata = info->provider->getByHash(hash, system);
    }

    if (metadata.title.isEmpty()) {
        emit providerFailed(providerName, QStringLiteral("No hash match"));
        return { };
    }

    if (metadata.matchScore <= 0.0f) {
        metadata.matchScore = 1.0f;
        metadata.matchMethod = QString::fromLatin1(MatchMethods::HASH);
    }
    if (metadata.providerId.isEmpty())
        metadata.providerId = providerName;
    emit providerSucceeded(providerName, MatchMethods::HASH);
    return metadata;
}

GameMetadata ProviderOrchestrator::searchWithFallback(const QString &hash, const QString &name, const QString &system,
    const QString &crc32, const QString &md5, const QString &sha1, const QString &serial, qint64 fileSize,
    const QString &raMd5, bool requireArtwork, const QString &contentSha1) {
    GameMetadata accumulator;
    const QString cacheKey = !contentSha1.isEmpty() ? contentSha1 : hash;
    if (!cacheKey.isEmpty() && m_cache) {
        const GameMetadata cached = m_cache->getByHash(cacheKey, system);
        if (!cached.title.isEmpty()) {
            if (!requireArtwork || !cached.boxArtUrl.isEmpty()) {
                qInfo() << "Cache hit for hash:" << cacheKey << "-" << cached.title;
                return cached;
            }

            qInfo() << "Cache hit for hash:" << cacheKey << "-" << cached.title
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

    // Pass 1 — compendium establishes identity; other providers fill gaps only.
    const QString compendiumId = QString::fromLatin1(Constants::Providers::COMPENDIUM);
    if (m_providers.contains(compendiumId) && m_providers[compendiumId].enabled) {
        queryProvider(
            accumulator, compendiumId, hash, name, system, crc32, md5, sha1, serial, fileSize, raMd5, contentSha1);
        if (!accumulator.title.isEmpty()) {
            const FieldSet gaps = computeFieldGap(accumulator);
            if (!gaps.isEmpty()) {
                const QSet<QString> exclude = { compendiumId };
                accumulator = enrichMissingFields(gaps, accumulator, hash, accumulator.title, system, crc32, md5, sha1,
                    serial, exclude, raMd5, fileSize, contentSha1);
            }
            if (!cacheKey.isEmpty() && m_cache) {
                m_cache->store(accumulator, cacheKey, system);
            }
            return accumulator;
        }
    }

    // Pass 2 — legacy waterfall (compendium excluded; already tried in Pass 1).
    QStringList localProviders = getSortedLocalProviders();
    localProviders.removeAll(compendiumId);
    for (const QString &providerName : localProviders) {
        if (!Constants::ProviderFields::providerSupportsMetadataLookup(providerName))
            continue;
        queryProvider(
            accumulator, providerName, hash, name, system, crc32, md5, sha1, serial, fileSize, raMd5, contentSha1);
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
            if (!Constants::ProviderFields::providerSupportsMetadataLookup(providerName))
                continue;
            queryProvider(
                accumulator, providerName, hash, name, system, crc32, md5, sha1, serial, fileSize, raMd5, contentSha1);
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
    } else if (!cacheKey.isEmpty() && m_cache) {
        m_cache->store(accumulator, cacheKey, system);
    }
    return accumulator;
}

ArtworkUrls ProviderOrchestrator::getArtworkWithFallback(
    const QString &id, const QString &system, const QString &providerName, const QMap<QString, QString> &externalIds) {
    Q_UNUSED(system);

    if (m_cache) {
        const ArtworkUrls cached = m_cache->getArtwork(id);
        if (!cached.boxFront.isEmpty()) {
            qInfo() << "Cache hit for artwork ID:" << id;
            return cached;
        }
    }

    auto artworkLookupId = [&](const QString &providerKey, const QString &baseId) -> QString {
        if (providerKey == Constants::Providers::STEAMGRIDDB)
            return SteamGridDBProvider::resolveArtworkLookupId(baseId, externalIds);
        return baseId;
    };

    if (!providerName.isEmpty() && m_providers.contains(providerName)) {
        const ProviderInfo &info = m_providers[providerName];
        if (info.enabled) {
            qInfo() << "Fetching artwork from preferred provider:" << providerName;
            const ArtworkUrls artwork = info.provider->getArtwork(artworkLookupId(providerName, id));
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
        if (!Constants::ProviderFields::providerSupportsArtworkLookup(name))
            continue;
        qInfo() << "Trying artwork from:" << name;
        const ArtworkUrls artwork = info.provider->getArtwork(artworkLookupId(name, id));
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
