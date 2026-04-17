#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>

namespace Remus {
namespace Constants {
namespace Systems {

// ============================================================================
// System ID Constants
// ============================================================================

/// Nintendo Entertainment System
inline constexpr int ID_NES = 1;

/// Super Nintendo Entertainment System
inline constexpr int ID_SNES = 2;

/// Nintendo 64
inline constexpr int ID_N64 = 3;

/// Nintendo GameCube
inline constexpr int ID_GAMECUBE = 4;

/// Nintendo Wii
inline constexpr int ID_WII = 5;

/// Game Boy
inline constexpr int ID_GB = 6;

/// Game Boy Color
inline constexpr int ID_GBC = 7;

/// Game Boy Advance
inline constexpr int ID_GBA = 8;

/// Nintendo DS
inline constexpr int ID_NDS = 9;

/// Sega Genesis / Mega Drive
inline constexpr int ID_GENESIS = 10;

/// Sega Master System
inline constexpr int ID_MASTER_SYSTEM = 11;

/// Sega Saturn
inline constexpr int ID_SATURN = 12;

/// Sega Dreamcast
inline constexpr int ID_DREAMCAST = 13;

/// Sony PlayStation (original)
inline constexpr int ID_PSX = 14;

/// Sony PlayStation 2
inline constexpr int ID_PS2 = 15;

/// Sony PlayStation Portable
inline constexpr int ID_PSP = 16;

/// Atari 2600
inline constexpr int ID_ATARI_2600 = 17;

/// Atari 7800
inline constexpr int ID_ATARI_7800 = 18;

/// Atari Lynx
inline constexpr int ID_LYNX = 19;

/// TurboGrafx-16 / PC Engine
inline constexpr int ID_TURBOGRAFX16 = 20;

/// TurboGrafx-CD / PC Engine CD
inline constexpr int ID_TURBOGRAFX_CD = 21;

/// SNK Neo Geo
inline constexpr int ID_NEO_GEO = 22;

/// Sega CD / Mega CD
inline constexpr int ID_SEGA_CD = 23;

/// Sega Game Gear
inline constexpr int ID_GAME_GEAR = 24;

/// Sega 32X
inline constexpr int ID_32X = 25;

/// Atari Jaguar
inline constexpr int ID_ATARI_JAGUAR = 26;

/// Neo Geo Pocket / Color
inline constexpr int ID_NGP = 27;

/// WonderSwan / Color
inline constexpr int ID_WONDERSWAN = 28;

/// Virtual Boy
inline constexpr int ID_VIRTUAL_BOY = 29;

/// Nintendo 3DS
inline constexpr int ID_3DS = 30;

/// Nintendo Switch
inline constexpr int ID_SWITCH = 31;

/// PlayStation Vita
inline constexpr int ID_PSVITA = 32;

/// Commodore 64
inline constexpr int ID_C64 = 33;

/// Amiga
inline constexpr int ID_AMIGA = 34;

/// ZX Spectrum
inline constexpr int ID_ZX_SPECTRUM = 35;

/// PC Engine SuperGrafx
inline constexpr int ID_SUPERGRAFX = 36;

/// Xbox
inline constexpr int ID_XBOX = 37;

/// Xbox 360
inline constexpr int ID_XBOX360 = 38;

/// Arcade / MAME
inline constexpr int ID_ARCADE = 39;

/// 3DO Interactive Multiplayer
inline constexpr int ID_3DO = 40;

/// Neo Geo CD
inline constexpr int ID_NEO_GEO_CD = 41;

// ============================================================================
// System Definition
// ============================================================================

/**
 * @brief Complete definition of a gaming system
 */
struct SystemDef {
    int id;                          ///< Unique system ID
    QString internalName;            ///< Code name: "NES", "PlayStation"
    QString displayName;             ///< Full name: "Nintendo Entertainment System"
    QString manufacturer;            ///< "Nintendo", "Sony", "Sega"
    int generation;                  ///< Console generation: 3, 4, 5, etc.
    QStringList extensions;          ///< File extensions: [".nes", ".unf"]
    QString preferredHash;           ///< "CRC32", "MD5", or "SHA1"
    QStringList regionCodes;         ///< Region codes: ["USA", "JPN", "EUR"]
    bool isMultiFile;                ///< True for .cue/.bin or multi-disc games
    QString uiColor;                 ///< Badge color: "#e74c3c"
    int releaseYear;                 ///< Year first released internationally
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
const SystemDef* getSystem(int systemId);

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
const SystemDef* getSystemByName(const QString &name);

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
