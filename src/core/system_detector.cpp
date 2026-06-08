#include "system_detector.h"
#include "disc_magic_detector.h"
#include "constants/constants.h"
#include <QtEndian>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QFile>

namespace Remus {

SystemDetector::SystemDetector() {
    initializeDefaultSystems();
}

void SystemDetector::loadSystems(const QList<SystemInfo> &systems) {
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

QString SystemDetector::detectSystem(const QString &extension, const QString &path) const {
    const QString ext = extension.toLower();

    QStringList candidates = getCandidatesForExtension(ext);
    if (candidates.isEmpty()) {
        return QString();
    }

    // For ambiguous disc image extensions, probe magic bytes first
    if (candidates.size() > 1 && !path.isEmpty() && DiscMagicDetector::isDiscImageExtension(ext)) {
        DiscHeaderInfo discInfo = DiscMagicDetector::detect(path);
        if (discInfo.detected && !discInfo.systemName.isEmpty()) {
            // Verify this system is in our candidate list
            if (candidates.contains(discInfo.systemName)) {
                return discInfo.systemName;
            }
        }
    }

    if (candidates.size() > 1 && !path.isEmpty()) {
        const QString byPath = detectFromPath(path, candidates);
        if (!byPath.isEmpty()) {
            return byPath;
        }
    }

    return candidates.first();
}

QString SystemDetector::detectFromPath(const QString &path, const QStringList &candidates) const {
    const QString lowerPath = path.toLower();

    // For ambiguous ISO files, inspect headers first when possible.
    const QFileInfo info(path);
    const QString ext = QStringLiteral(".") + info.suffix().toLower();
    if (ext == QStringLiteral(".iso")) {
        const QString byHeader = detectFromIsoHeader(path, candidates);
        if (!byHeader.isEmpty()) {
            return byHeader;
        }
    }

    // Check for system name in path
    for (const QString &candidate : candidates) {
        if (lowerPath.contains(candidate.toLower())) {
            return candidate;
        }

        // Check for common folder name patterns
        if (candidate == "PlayStation" && (lowerPath.contains("psx") || lowerPath.contains("ps1"))) {
            return candidate;
        }
        if (candidate == "PlayStation 2"
            && (lowerPath.contains("ps2") || lowerPath.contains("pcsx2") || lowerPath.contains("slus")
                || lowerPath.contains("scus") || lowerPath.contains("sles") || lowerPath.contains("slps")
                || lowerPath.contains("scps"))) {
            return candidate;
        }
        if (candidate == "GameCube"
            && (lowerPath.contains("gamecube") || lowerPath.contains("gcn") || lowerPath.contains("ngc")
                || lowerPath.contains("dolphin") || lowerPath.endsWith(QStringLiteral(".gcm"))
                || lowerPath.endsWith(QStringLiteral(".gcz")) || lowerPath.endsWith(QStringLiteral(".rvz")))) {
            return candidate;
        }
        if (candidate == "Wii"
            && (lowerPath.contains("wii") || lowerPath.contains("wbfs")
                || lowerPath.endsWith(QStringLiteral(".rvz")))) {
            return candidate;
        }
        if (candidate == "PSP"
            && (lowerPath.contains("psp") || lowerPath.contains("ppsspp") || lowerPath.contains("ulus")
                || lowerPath.contains("ules") || lowerPath.contains("uljm") || lowerPath.contains("ucus")
                || lowerPath.contains("npuh") || lowerPath.contains("npjh"))) {
            return candidate;
        }
    }

    // Default to first candidate
    return candidates.first();
}

QString SystemDetector::detectFromIsoHeader(const QString &path, const QStringList &candidates) const {
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    // Wii and GameCube magic values in disc header.
    // Wii:      0x5D1C9EA3 at 0x18
    // GameCube: 0xC2339F3D at 0x1C
    if (file.size() > 0x20) {
        if (file.seek(0x18)) {
            const QByteArray wiiMagicBytes = file.read(4);
            if (wiiMagicBytes.size() == 4) {
                const quint32 wiiMagic
                    = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(wiiMagicBytes.constData()));
                if (wiiMagic == 0x5D1C9EA3u && candidates.contains(QStringLiteral("Wii"))) {
                    return QStringLiteral("Wii");
                }
            }
        }
        if (file.seek(0x1C)) {
            const QByteArray gcMagicBytes = file.read(4);
            if (gcMagicBytes.size() == 4) {
                const quint32 gcMagic
                    = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(gcMagicBytes.constData()));
                if (gcMagic == 0xC2339F3Du && candidates.contains(QStringLiteral("GameCube"))) {
                    return QStringLiteral("GameCube");
                }
            }
        }
    }

    // Light signature scan for PS2/PSP markers in first chunk.
    if (file.seek(0)) {
        const QByteArray head = file.read(4 * 1024 * 1024);
        if (!head.isEmpty()) {
            const QByteArray upper = head.toUpper();
            if (candidates.contains(QStringLiteral("PlayStation 2"))
                && (upper.contains("BOOT2 = CDROM0:\\\\SL") || upper.contains("SYSTEM.CNF"))) {
                return QStringLiteral("PlayStation 2");
            }
            if (candidates.contains(QStringLiteral("PSP"))
                && (upper.contains("PSP_GAME") || upper.contains("UMD_DATA.BIN"))) {
                return QStringLiteral("PSP");
            }
            if (candidates.contains(QStringLiteral("PlayStation")) && upper.contains("PLAYSTATION")) {
                return QStringLiteral("PlayStation");
            }
        }
    }

    return QString();
}

SystemInfo SystemDetector::getSystemInfo(const QString &systemName) const {
    return m_systems.value(systemName, SystemInfo());
}

QString SystemDetector::getPreferredHash(const QString &systemName) const {
    if (m_systems.contains(systemName)) {
        return m_systems[systemName].preferredHash;
    }
    return "MD5"; // Default fallback
}

QStringList SystemDetector::getAllExtensions() const {
    return m_extensionMap.keys();
}

void SystemDetector::initializeDefaultSystems() {
    using namespace Constants::Systems;

    QList<SystemInfo> systems;

    // Load all systems from the constants registry
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        const auto &def = it.value();
        systems.append({ def.id, def.internalName, def.displayName, def.manufacturer, def.generation, def.extensions,
            def.preferredHash });
    }

    // Note: The old hardcoded system list has been replaced with the constants registry.
    // If additional systems are needed that aren't in the registry, add them to
    // src/core/constants/systems.h instead of here.

    loadSystems(systems);
}

QStringList SystemDetector::getCandidatesForExtension(const QString &extension) const {
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
