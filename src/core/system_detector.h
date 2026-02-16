#ifndef REMUS_SYSTEM_DETECTOR_H
#define REMUS_SYSTEM_DETECTOR_H

#include <QString>
#include <QMap>
#include <QStringList>

namespace Remus {

/**
 * @brief System information
 */
struct SystemInfo {
    int id = 0;
    QString name;
    QString displayName;
    QString manufacturer;
    int generation = 0;
    QStringList extensions;
    QString preferredHash;  // "CRC32", "MD5", or "SHA1"
};

/**
 * @brief Detects gaming system from file extension and path heuristics
 */
class SystemDetector {
public:
    SystemDetector();

    /**
     * @brief Initialize with systems from database
     * @param systems List of supported systems
     */
    void loadSystems(const QList<SystemInfo> &systems);

    /**
     * @brief Detect system from file extension
     * @param extension File extension (e.g., ".nes")
     * @param path Full file path (optional, for ambiguous extensions)
     * @return System name or empty string if not detected
     */
    QString detectSystem(const QString &extension, const QString &path = QString()) const;

    /**
     * @brief Get system info by name
     */
    SystemInfo getSystemInfo(const QString &systemName) const;

    /**
     * @brief Get preferred hash algorithm for system
     */
    QString getPreferredHash(const QString &systemName) const;

    /**
     * @brief Get all supported extensions
     */
    QStringList getAllExtensions() const;

private:
    void initializeDefaultSystems();
    QString detectFromPath(const QString &path, const QStringList &candidates) const;

    QMap<QString, QString> m_extensionMap;  // extension -> system name
    QMap<QString, SystemInfo> m_systems;     // system name -> info
};

} // namespace Remus

#endif // REMUS_SYSTEM_DETECTOR_H
