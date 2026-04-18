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

} // namespace Remus
