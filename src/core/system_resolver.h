#pragma once

#include <QString>
#include <QMap>
#include "constants/systems.h"

namespace Remus {

/**
 * @brief Unified system name resolution for database, UI, and metadata providers
 * 
 * This class provides a single source of truth for all system-related name mappings,
 * preventing inconsistencies between database names, display names, and provider-specific IDs.
 * 
 * Usage:
 *   - UI displays: SystemResolver::displayName(systemId)
 *   - Database queries: SystemResolver::internalName(systemId)
 *   - Provider APIs: SystemResolver::providerName(systemId, "thegamesdb")
 */
class SystemResolver
{
public:
    /**
     * @brief Get human-readable display name for UI
     * @param systemId System ID from database (e.g., ID_GENESIS = 10)
     * @return Display name like "Sega Genesis / Mega Drive", or "Unknown" if not found
     */
    static QString displayName(int systemId);
    
    /**
     * @brief Get internal name for database queries
     * @param systemId System ID from database
     * @return Internal name like "Genesis", or "Unknown" if not found
     */
    static QString internalName(int systemId);
    
    /**
     * @brief Get provider-specific platform identifier
     * @param systemId System ID from database
     * @param providerId Provider identifier ("thegamesdb", "screenscraper", "igdb")
     * @return Provider-specific ID, or empty string if not found
     * 
     * Examples:
     *   - SystemResolver::providerName(10, "thegamesdb") → "18" (TheGamesDB platform ID)
     *   - SystemResolver::providerName(10, "screenscraper") → "1" (ScreenScraper platform ID)
     *   - SystemResolver::providerName(10, "igdb") → "genesis" (IGDB platform slug)
     */
    static QString providerName(int systemId, const QString &providerId);
    
    /**
     * @brief Get system ID by internal name (reverse lookup)
     * @param internalName Internal name like "Genesis" or "PlayStation"
     * @return System ID, or 0 if not found
     */
    static int systemIdByName(const QString &internalName);
    
    /**
     * @brief Check if system exists in registry
     * @param systemId System ID to check
     * @return True if system is defined
     */
    static bool isValidSystem(int systemId);

private:
    /**
     * @brief Provider platform ID mappings
     * 
     * Structure: { systemId: { providerId: platformId } }
     * 
     * TheGamesDB uses numeric string IDs
     * ScreenScraper uses numeric IDs  
     * IGDB uses lowercase slugs
     * Hasheous uses generic system names (same as internal)
     */
    static QMap<int, QMap<QString, QString>> providerMappings();
};

} // namespace Remus
