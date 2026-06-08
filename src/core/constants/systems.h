#pragma once

#include "system_ids.h"

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>

namespace Remus {
namespace Constants {
    namespace Systems {

        // ============================================================================
        // System Definition
        // ============================================================================

        /**
         * @brief Complete definition of a gaming system
         */
        struct SystemDef {
            int id; ///< Unique system ID
            QString internalName; ///< Code name: "NES", "PlayStation"
            QString displayName; ///< Full name: "Nintendo Entertainment System"
            QString manufacturer; ///< "Nintendo", "Sony", "Sega"
            int generation; ///< Console generation: 3, 4, 5, etc.
            QStringList extensions; ///< File extensions: [".nes", ".unf"]
            QString preferredHash; ///< "CRC32", "MD5", or "SHA1"
            QStringList regionCodes; ///< Region codes: ["USA", "JPN", "EUR"]
            bool isMultiFile; ///< True for .cue/.bin or multi-disc games
            QString uiColor; ///< Badge color: "#e74c3c"
            int releaseYear; ///< Year first released internationally
        };

        // ============================================================================
        // System Registry
        // ============================================================================

        /**
         * @brief Complete registry of all supported gaming systems
         */
        extern const QMap<int, SystemDef> SYSTEMS;

        // ============================================================================
        // Extension to System Mapping
        // ============================================================================

        /**
         * @brief Reverse lookup: file extension → possible systems
         *
         * Used during file scanning to suggest possible systems.
         * Some extensions are ambiguous (.iso can be PS1, PS2, GameCube, etc.)
         */
        extern const QMap<QString, QList<int>> EXTENSION_TO_SYSTEMS;

        // ============================================================================
        // System Grouping
        // ============================================================================

        /**
         * @brief Nintendo systems (for grouping/organization)
         */
        extern const QList<int> NINTENDO_SYSTEMS;

        /**
         * @brief Sega systems
         */
        extern const QList<int> SEGA_SYSTEMS;

        /**
         * @brief Sony/PlayStation systems
         */
        extern const QList<int> SONY_SYSTEMS;

        /**
         * @brief Microsoft Xbox systems
         */
        extern const QList<int> MICROSOFT_SYSTEMS;

        /**
         * @brief Handheld systems
         */
        extern const QList<int> HANDHELD_SYSTEMS;

        /**
         * @brief Disc-based systems (require special handling)
         */
        extern const QList<int> DISC_SYSTEMS;

        /**
         * @brief Cartridge-based systems (single file, fast hashing)
         */
        extern const QList<int> CARTRIDGE_SYSTEMS;

        /**
         * @brief Home computer systems
         */
        extern const QList<int> COMPUTER_SYSTEMS;

        // ============================================================================
        // Helper Functions
        // ============================================================================

        /**
         * @brief Get system definition by ID
         * @param systemId System ID constant (ID_NES, etc.)
         * @return Pointer to SystemDef, or nullptr if not found
         */
        const SystemDef *getSystem(int systemId);

        /**
         * @brief Get system ID by internal name
         * @param name Internal system name ("NES", "PlayStation", etc.)
         * @return System ID, or 0 if not found
         */
        int getSystemIdByName(const QString &name);

        /**
         * @brief Get system definition by internal name
         * @param name Internal system name ("NES", "PlayStation", etc.)
         * @return Pointer to SystemDef, or nullptr if not found
         */
        const SystemDef *getSystemByName(const QString &name);

        /**
         * @brief Get all system display names
         * @return List of human-readable system names for UI population
         */
        QStringList getSystemDisplayNames();

        /**
         * @brief Get all system internal names
         * @return List of internal system names ("NES", "SNES", etc.)
         */
        QStringList getSystemInternalNames();

        /**
         * @brief Get possible systems for a file extension
         * @param extension File extension (e.g., ".iso", ".nes")
         * @return List of system IDs that use this extension
         */
        QList<int> getSystemsForExtension(const QString &extension);

        /**
         * @brief Check if extension is ambiguous (used by multiple systems)
         * @param extension File extension to check
         * @return True if multiple systems share this extension
         */
        bool isAmbiguousExtension(const QString &extension);

    } // Systems
} // Constants
} // Remus
