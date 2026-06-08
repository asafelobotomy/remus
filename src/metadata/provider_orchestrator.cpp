#include "provider_orchestrator.h"

#include <QDebug>
#include <algorithm>

#include "../core/constants/constants.h"
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
// Static shared helper
// ---------------------------------------------------------------------------

namespace {

    // Decode the five XML/HTML named character references that appear most
    // frequently in ScreenScraper synopsis text.
    QString decodeHtmlEntities(QString s) {
        s.replace(QLatin1String("&amp;"), QLatin1String("&"));
        s.replace(QLatin1String("&quot;"), QLatin1String("\""));
        s.replace(QLatin1String("&apos;"), QLatin1String("'"));
        s.replace(QLatin1String("&lt;"), QLatin1String("<"));
        s.replace(QLatin1String("&gt;"), QLatin1String(">"));
        return s;
    }

    // Minimum number of characters a description must have after sanitization
    // to be considered meaningful content (not just a field artifact or stub).
    constexpr int MIN_DESCRIPTION_LENGTH = 10;

} // namespace

void ProviderOrchestrator::mergeMetadata(GameMetadata &target, const GameMetadata &source) {
    if (target.title.isEmpty() && !source.title.isEmpty())
        target.title = source.title;
    if (target.system.isEmpty() && !source.system.isEmpty())
        target.system = source.system;
    if (target.region.isEmpty() && !source.region.isEmpty())
        target.region = source.region;
    if (target.publisher.isEmpty() && !source.publisher.isEmpty())
        target.publisher = source.publisher;
    if (target.developer.isEmpty() && !source.developer.isEmpty())
        target.developer = source.developer;
    if (target.genres.isEmpty() && !source.genres.isEmpty())
        target.genres = source.genres;
    if (target.releaseDate.isEmpty() && !source.releaseDate.isEmpty())
        target.releaseDate = source.releaseDate;
    if (target.description.isEmpty() && !source.description.isEmpty()) {
        const QString sanitized = decodeHtmlEntities(source.description.trimmed());
        if (sanitized.length() >= MIN_DESCRIPTION_LENGTH) {
            target.description = sanitized;
        }
    }
    if (target.players == 0 && source.players != 0)
        target.players = source.players;
    if (target.rating == 0.0f && source.rating != 0.0f)
        target.rating = source.rating;
    if (target.ratingSource.isEmpty() && !source.ratingSource.isEmpty())
        target.ratingSource = source.ratingSource;
    if (target.id.isEmpty() && !source.id.isEmpty())
        target.id = source.id;
    if (target.boxArtUrl.isEmpty() && !source.boxArtUrl.isEmpty())
        target.boxArtUrl = source.boxArtUrl;
    if (target.screenshotUrls.isEmpty() && !source.screenshotUrls.isEmpty())
        target.screenshotUrls = source.screenshotUrls;
    for (auto it = source.externalIds.constBegin(); it != source.externalIds.constEnd(); ++it) {
        if (!target.externalIds.contains(it.key())) {
            target.externalIds[it.key()] = it.value();
        }
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

ProviderOrchestrator::ProviderOrchestrator(QObject *parent)
    : QObject(parent) { }

void ProviderOrchestrator::setCache(MetadataCache *cache) {
    m_cache = cache;
}

void ProviderOrchestrator::addProvider(const QString &name, MetadataProvider *provider, int priority) {
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
    m_sortCacheDirty = true;

    qInfo() << "Added provider:" << name << "| Priority:" << priority
            << "| Hash support:" << (info.supportsHash ? "YES" : "NO");
}

void ProviderOrchestrator::removeProvider(const QString &name) {
    if (m_providers.contains(name)) {
        ProviderInfo info = m_providers.take(name);
        // Provider is Qt-parented to the orchestrator; Qt parent-child cleanup handles
        // deletion at orchestrator destruction. Manual delete here would be a double-free.
        info.provider->setParent(nullptr);
        info.provider->deleteLater();
        m_sortCacheDirty = true;
        qInfo() << "Removed provider:" << name;
    }
}

void ProviderOrchestrator::setProviderEnabled(const QString &name, bool enabled) {
    if (m_providers.contains(name)) {
        m_providers[name].enabled = enabled;
        m_sortCacheDirty = true;
        qInfo() << "Provider" << name << (enabled ? "enabled" : "disabled");
    }
}

bool ProviderOrchestrator::detectHashSupport(const QString &name) const {
    static const QSet<QString> hashProviders = []() {
        QSet<QString> s;
        for (const QString &p : Constants::Providers::getHashSupportingProviders())
            s.insert(p);
        s.insert(QStringLiteral("retroachievements"));
        s.insert(QStringLiteral("playmatch"));
        s.insert(QStringLiteral("localdatabase"));
        return s;
    }();
    return hashProviders.contains(name.toLower());
}

bool ProviderOrchestrator::detectLocalProvider(const QString &name) const {
    // Providers that work entirely offline (no network required)
    const QString lower = name.toLower();
    return lower == QStringLiteral("localdatabase") || lower == QStringLiteral("compendium")
        || lower == QStringLiteral("gametdb");
}

bool ProviderOrchestrator::providerSupportsHash(const QString &name) const {
    if (m_providers.contains(name)) {
        return m_providers[name].supportsHash;
    }
    return false;
}

QStringList ProviderOrchestrator::getSortedProviders(bool hashOnly) const {
    if (m_sortCacheDirty) {
        QList<QPair<QString, int>> allPairs;
        QList<QPair<QString, int>> hashPairs;

        for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
            const ProviderInfo &info = it.value();
            if (!info.enabled)
                continue;
            allPairs.append(qMakePair(it.key(), info.priority));
            if (info.supportsHash)
                hashPairs.append(qMakePair(it.key(), info.priority));
        }

        auto byPriority
            = [](const QPair<QString, int> &a, const QPair<QString, int> &b) { return a.second > b.second; };
        std::sort(allPairs.begin(), allPairs.end(), byPriority);
        std::sort(hashPairs.begin(), hashPairs.end(), byPriority);

        m_cachedSortedAll.clear();
        for (const auto &p : allPairs)
            m_cachedSortedAll.append(p.first);

        m_cachedSortedHash.clear();
        for (const auto &p : hashPairs)
            m_cachedSortedHash.append(p.first);

        m_sortCacheDirty = false;
    }

    return hashOnly ? m_cachedSortedHash : m_cachedSortedAll;
}

QStringList ProviderOrchestrator::getEnabledProviders() const {
    return getSortedProviders(false);
}

MetadataProvider *ProviderOrchestrator::getProvider(const QString &name) const {
    auto it = m_providers.constFind(name);
    return (it != m_providers.constEnd()) ? it->provider : nullptr;
}

QStringList ProviderOrchestrator::getSortedLocalProviders() const {
    QList<QPair<QString, int>> result;
    for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
        if (it.value().enabled && it.value().isLocal)
            result.append(qMakePair(it.key(), it.value().priority));
    }
    std::sort(result.begin(), result.end(),
        [](const QPair<QString, int> &a, const QPair<QString, int> &b) { return a.second > b.second; });
    QStringList names;
    for (const auto &p : result)
        names.append(p.first);
    return names;
}

QStringList ProviderOrchestrator::getSortedRemoteProviders() const {
    QList<QPair<QString, int>> result;
    for (auto it = m_providers.constBegin(); it != m_providers.constEnd(); ++it) {
        if (it.value().enabled && !it.value().isLocal)
            result.append(qMakePair(it.key(), it.value().priority));
    }
    std::sort(result.begin(), result.end(),
        [](const QPair<QString, int> &a, const QPair<QString, int> &b) { return a.second > b.second; });
    QStringList names;
    for (const auto &p : result)
        names.append(p.first);
    return names;
}

} // namespace Remus
