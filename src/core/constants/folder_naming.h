#pragma once

#include <QString>
#include <QStringList>

#include "systems.h"

namespace Remus {
namespace Constants {
namespace FolderNaming {

// ============================================================================
// Folder Naming Schemes
// ============================================================================

/**
 * @brief Predefined folder naming schemes matching popular emulation frontends
 *
 * Each scheme maps system IDs to the exact directory names expected by the
 * corresponding frontend. "Default" uses ES-DE conventions (widest compatibility).
 */
enum class Scheme {
    None,       ///< No system subfolder — flat output (legacy behaviour)
    Default,    ///< ES-DE / majority vote across frontends
    Batocera,   ///< Batocera Linux
    RetroPie,   ///< RetroPie
    EmuDeck,    ///< EmuDeck (Steam Deck)
    RomM        ///< RomM server
};

inline const QStringList SCHEME_NAMES = {
    QStringLiteral("none"),
    QStringLiteral("default"),
    QStringLiteral("batocera"),
    QStringLiteral("retropie"),
    QStringLiteral("emudeck"),
    QStringLiteral("romm")
};

/**
 * @brief Parse a scheme name string to the enum
 * @param name Scheme name (case-insensitive)
 * @return Scheme enum value, or Scheme::None if unrecognised
 */
inline Scheme schemeFromString(const QString &name)
{
    const QString lower = name.trimmed().toLower();
    if (lower == QLatin1String("default") || lower == QLatin1String("es-de") || lower == QLatin1String("esde"))
        return Scheme::Default;
    if (lower == QLatin1String("batocera"))
        return Scheme::Batocera;
    if (lower == QLatin1String("retropie"))
        return Scheme::RetroPie;
    if (lower == QLatin1String("emudeck"))
        return Scheme::EmuDeck;
    if (lower == QLatin1String("romm"))
        return Scheme::RomM;
    // RetroDeck embeds ES-DE, so it uses the Default (ES-DE) naming scheme
    if (lower == QLatin1String("retrodeck"))
        return Scheme::Default;
    return Scheme::None;
}

/**
 * @brief Get the display name for a scheme
 */
inline QString schemeDisplayName(Scheme scheme)
{
    switch (scheme) {
    case Scheme::None:     return QStringLiteral("None (flat)");
    case Scheme::Default:  return QStringLiteral("Default (ES-DE)");
    case Scheme::Batocera: return QStringLiteral("Batocera");
    case Scheme::RetroPie: return QStringLiteral("RetroPie");
    case Scheme::EmuDeck:  return QStringLiteral("EmuDeck");
    case Scheme::RomM:     return QStringLiteral("RomM");
    }
    return QStringLiteral("Unknown");
}

// ============================================================================
// System Folder Name Lookup
// ============================================================================

/**
 * @brief Get the folder name for a system under the given naming scheme
 * @param systemId System ID constant (ID_NES, etc.)
 * @param scheme Folder naming scheme to use
 * @return Folder name (e.g. "nes", "megadrive", "psx"), or empty string if unknown
 *
 * Source-verified from:
 * - ES-DE: es_systems.xml <name> tags
 * - Batocera: /userdata/roms/ folder names
 * - RetroPie: ~/RetroPie/roms/ folder names
 * - EmuDeck: Emulation/roms/ folder names
 * - RomM: UniversalPlatformSlug enum values
 */
inline QString folderNameForSystemId(int systemId, Scheme scheme)
{
    using namespace Systems;

    // For Scheme::None, return empty — caller handles flat output
    if (scheme == Scheme::None)
        return {};

    // --- Default / ES-DE (majority-vote baseline) ---
    // Other schemes only override where they diverge.

    switch (systemId) {

    // ── Nintendo ────────────────────────────────────────
    case ID_NES:
        return QStringLiteral("nes");

    case ID_SNES:
        return QStringLiteral("snes");

    case ID_N64:
        return QStringLiteral("n64");

    case ID_GB:
        return QStringLiteral("gb");

    case ID_GBC:
        return QStringLiteral("gbc");

    case ID_GBA:
        return QStringLiteral("gba");

    case ID_NDS:
        return QStringLiteral("nds");

    case ID_GAMECUBE:
        switch (scheme) {
        case Scheme::Batocera: return QStringLiteral("gamecube");
        case Scheme::RomM:     return QStringLiteral("ngc");
        default:               return QStringLiteral("gc");
        }

    case ID_WII:
        return QStringLiteral("wii");

    case ID_3DS:
        switch (scheme) {
        case Scheme::Default:  return QStringLiteral("n3ds");
        default:               return QStringLiteral("3ds");
        }

    case ID_VIRTUAL_BOY:
        return QStringLiteral("virtualboy");

    case ID_SWITCH:
        return QStringLiteral("switch");

    // ── Sega ────────────────────────────────────────────
    case ID_GENESIS:
        switch (scheme) {
        case Scheme::Default:
        case Scheme::EmuDeck:
        case Scheme::RomM:     return QStringLiteral("genesis");
        default:               return QStringLiteral("megadrive");
        }

    case ID_MASTER_SYSTEM:
        switch (scheme) {
        case Scheme::RomM: return QStringLiteral("sms");
        default:           return QStringLiteral("mastersystem");
        }

    case ID_SATURN:
        return QStringLiteral("saturn");

    case ID_DREAMCAST:
        switch (scheme) {
        case Scheme::RomM: return QStringLiteral("dc");
        default:           return QStringLiteral("dreamcast");
        }

    case ID_GAME_GEAR:
        return QStringLiteral("gamegear");

    case ID_SEGA_CD:
        return QStringLiteral("segacd");

    case ID_32X:
        switch (scheme) {
        case Scheme::RomM: return QStringLiteral("sega32");
        default:           return QStringLiteral("sega32x");
        }

    // ── Sony ────────────────────────────────────────────
    case ID_PSX:
        return QStringLiteral("psx");

    case ID_PS2:
        return QStringLiteral("ps2");

    case ID_PSP:
        return QStringLiteral("psp");

    case ID_PSVITA:
        return QStringLiteral("psvita");

    // ── Atari ───────────────────────────────────────────
    case ID_ATARI_2600:
        return QStringLiteral("atari2600");

    case ID_ATARI_7800:
        return QStringLiteral("atari7800");

    case ID_LYNX:
        switch (scheme) {
        case Scheme::Batocera:
        case Scheme::EmuDeck:
        case Scheme::RomM:     return QStringLiteral("lynx");
        default:               return QStringLiteral("atarilynx");
        }

    case ID_ATARI_JAGUAR:
        switch (scheme) {
        case Scheme::Batocera:
        case Scheme::RomM:     return QStringLiteral("jaguar");
        default:               return QStringLiteral("atarijaguar");
        }

    // ── NEC ─────────────────────────────────────────────
    case ID_TURBOGRAFX16:
        switch (scheme) {
        case Scheme::EmuDeck:
        case Scheme::RomM:     return QStringLiteral("tg16");
        default:               return QStringLiteral("pcengine");
        }

    case ID_TURBOGRAFX_CD:
        switch (scheme) {
        case Scheme::EmuDeck:  return QStringLiteral("tg-cd");
        case Scheme::RomM:     return QStringLiteral("turbografx-cd");
        default:               return QStringLiteral("pcenginecd");
        }

    case ID_SUPERGRAFX:
        return QStringLiteral("supergrafx");

    // ── SNK ─────────────────────────────────────────────
    case ID_NEO_GEO:
        switch (scheme) {
        case Scheme::EmuDeck:  return QStringLiteral("fbneo");
        case Scheme::RomM:     return QStringLiteral("neogeoaes");
        default:               return QStringLiteral("neogeo");
        }

    case ID_NGP:
        switch (scheme) {
        case Scheme::RomM: return QStringLiteral("neo-geo-pocket");
        default:           return QStringLiteral("ngp");
        }

    // ── Microsoft ───────────────────────────────────────
    case ID_XBOX:
        return QStringLiteral("xbox");

    case ID_XBOX360:
        return QStringLiteral("xbox360");

    // ── Other ───────────────────────────────────────────
    case ID_C64:
        return QStringLiteral("c64");

    case ID_AMIGA:
        return QStringLiteral("amiga");

    case ID_ZX_SPECTRUM:
        switch (scheme) {
        case Scheme::RomM: return QStringLiteral("zxs");
        default:           return QStringLiteral("zxspectrum");
        }

    case ID_WONDERSWAN:
        switch (scheme) {
        case Scheme::Batocera: return QStringLiteral("wswan");
        default:               return QStringLiteral("wonderswan");
        }

    case ID_ARCADE:
        switch (scheme) {
        case Scheme::Batocera: return QStringLiteral("mame");
        default:               return QStringLiteral("arcade");
        }

    default:
        return {};
    }
}

/**
 * @brief Get all folder names for a given scheme (for documentation/UI)
 * @param scheme Folder naming scheme
 * @return Map of system ID -> folder name
 */
inline QMap<int, QString> allFolderNames(Scheme scheme)
{
    QMap<int, QString> result;
    for (auto it = Systems::SYSTEMS.constBegin(); it != Systems::SYSTEMS.constEnd(); ++it) {
        QString folder = folderNameForSystemId(it.key(), scheme);
        if (!folder.isEmpty())
            result.insert(it.key(), folder);
    }
    return result;
}

} // FolderNaming
} // Constants
} // Remus
