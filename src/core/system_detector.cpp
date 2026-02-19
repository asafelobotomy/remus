#include "system_detector.h"
#include "constants/constants.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Remus {

SystemDetector::SystemDetector()
{
    initializeDefaultSystems();
}

void SystemDetector::loadSystems(const QList<SystemInfo> &systems)
{
    m_systems.clear();
    m_extensionMap.clear();

    for (const auto &system : systems) {
        m_systems[system.name] = system;
        
        for (const QString &ext : system.extensions) {
            // Handle ambiguous extensions (ISO, BIN, etc.)
            if (m_extensionMap.contains(ext)) {
                // Mark as ambiguous by appending
                m_extensionMap[ext] += "|" + system.name;
            } else {
                m_extensionMap[ext] = system.name;
            }
        }
    }
}

QString SystemDetector::detectSystem(const QString &extension, const QString &path) const
{
    const QString ext = extension.toLower();

    QStringList candidates = getCandidatesForExtension(ext);
    if (candidates.isEmpty()) {
        return QString();
    }

    if (candidates.size() > 1 && !path.isEmpty()) {
        const QString byPath = detectFromPath(path, candidates);
        if (!byPath.isEmpty()) {
            return byPath;
        }
    }

    return candidates.first();
}

QString SystemDetector::detectFromPath(const QString &path, const QStringList &candidates) const
{
    QString lowerPath = path.toLower();
    
    // Check for system name in path
    for (const QString &candidate : candidates) {
        if (lowerPath.contains(candidate.toLower())) {
            return candidate;
        }
        
        // Check for common folder name patterns
        if (candidate == "PlayStation" && 
            (lowerPath.contains("psx") || lowerPath.contains("ps1"))) {
            return candidate;
        }
        if (candidate == "PlayStation 2" && 
            (lowerPath.contains("ps2"))) {
            return candidate;
        }
        if (candidate == "GameCube" && 
            (lowerPath.contains("gamecube") || lowerPath.contains("gc"))) {
            return candidate;
        }
    }

    // Default to first candidate
    return candidates.first();
}

SystemInfo SystemDetector::getSystemInfo(const QString &systemName) const
{
    return m_systems.value(systemName, SystemInfo());
}

QString SystemDetector::getPreferredHash(const QString &systemName) const
{
    if (m_systems.contains(systemName)) {
        return m_systems[systemName].preferredHash;
    }
    return "MD5";  // Default fallback
}

QStringList SystemDetector::getAllExtensions() const
{
    return m_extensionMap.keys();
}

void SystemDetector::initializeDefaultSystems()
{
    using namespace Constants::Systems;
    
    QList<SystemInfo> systems;

    // Load all systems from the constants registry
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        const auto &def = it.value();
        systems.append({
            def.id,
            def.internalName,
            def.displayName,
            def.manufacturer,
            def.generation,
            def.extensions,
            def.preferredHash
        });
    }

    // Note: The old hardcoded system list has been replaced with the constants registry.
    // If additional systems are needed that aren't in the registry, add them to
    // src/core/constants/systems.h instead of here.
    
    loadSystems(systems);
}

QStringList SystemDetector::getCandidatesForExtension(const QString &extension) const
{
    using namespace Constants::Systems;

    QStringList candidates;

    // 1) Use the curated extension → systems map to preserve intentional priority
    auto extIt = EXTENSION_TO_SYSTEMS.find(extension);
    if (extIt != EXTENSION_TO_SYSTEMS.end()) {
        for (int systemId : extIt.value()) {
            const auto defIt = SYSTEMS.find(systemId);
            if (defIt != SYSTEMS.end()) {
                const QString &name = defIt.value().internalName;
                if (m_systems.contains(name) && !candidates.contains(name)) {
                    candidates.append(name);
                }
            }
        }
    }

    // 2) Fall back to the loaded system extension map (e.g., DB-provided systems)
    if (candidates.isEmpty() && m_extensionMap.contains(extension)) {
        candidates = m_extensionMap.value(extension).split('|', Qt::SkipEmptyParts);
    }

    return candidates;
}

} // namespace Remus
