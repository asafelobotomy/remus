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
    m_sortCacheDirty = true;

    qInfo() << "Added provider:" << name 
            << "| Priority:" << priority 
            << "| Hash support:" << (info.supportsHash ? "YES" : "NO");
}

void ProviderOrchestrator::removeProvider(const QString &name)
{
    if (m_providers.contains(name)) {
        ProviderInfo info = m_providers.take(name);
        delete info.provider;
        m_sortCacheDirty = true;
        qInfo() << "Removed provider:" << name;
    }
}

void ProviderOrchestrator::setProviderEnabled(const QString &name, bool enabled)
{
    if (m_providers.contains(name)) {
        m_providers[name].enabled = enabled;
        m_sortCacheDirty = true;
        qInfo() << "Provider" << name << (enabled ? "enabled" : "disabled");
    }
}

bool ProviderOrchestrator::detectHashSupport(const QString &name) const
{
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

bool ProviderOrchestrator::detectLocalProvider(const QString &name) const
{
    // Providers that work entirely offline (no network required)
    const QString lower = name.toLower();
    return lower == QStringLiteral("localdatabase")
    || lower == QStringLiteral("compendium")
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

        auto byPriority = [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
            return a.second > b.second;
        };
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

} // namespace Remus
