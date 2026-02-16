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
    QString ext = extension.toLower();
    
    if (!m_extensionMap.contains(ext)) {
        return QString();  // Unknown extension
    }

    QString systemName = m_extensionMap[ext];
    
    // Check for ambiguous extension (contains |)
    if (systemName.contains('|')) {
        QStringList candidates = systemName.split('|');
        if (!path.isEmpty()) {
            return detectFromPath(path, candidates);
        }
        // Return first candidate if path not provided
        return candidates.first();
    }

    return systemName;
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

} // namespace Remus
