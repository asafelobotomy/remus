#pragma once

#include <QString>

#include "systems.h"

namespace Remus {
namespace Constants {
namespace Exports {

namespace Formats {
inline const QString RETROARCH = QStringLiteral("retroarch");
inline const QString EMUSTATION = QStringLiteral("emustation");
inline const QString LAUNCHBOX = QStringLiteral("launchbox");
inline const QString CSV = QStringLiteral("csv");
inline const QString JSON = QStringLiteral("json");
}

namespace DisplayNames {
inline const QString RETROARCH = QStringLiteral("RetroArch");
inline const QString EMULATION_STATION = QStringLiteral("EmulationStation");
inline const QString LAUNCHBOX = QStringLiteral("LaunchBox");
inline const QString CSV = QStringLiteral("CSV");
inline const QString JSON = QStringLiteral("JSON");
}

namespace Files {
inline const QString PLAYLIST_EXTENSION = QStringLiteral(".lpl");
inline const QString ES_GAMELIST = QStringLiteral("gamelist.xml");
inline const QString DEFAULT_RETROARCH_EXPORT = QStringLiteral("remus.lpl");
inline const QString DEFAULT_LAUNCHBOX_EXPORT = QStringLiteral("launchbox-games.xml");
inline const QString DEFAULT_CSV_EXPORT = QStringLiteral("remus-export.csv");
inline const QString DEFAULT_JSON_EXPORT = QStringLiteral("remus-export.json");
}

namespace RetroArch {
inline const QString PLAYLIST_VERSION = QStringLiteral("1.5");
inline const QString CORE_DETECT = QStringLiteral("DETECT");
inline const QString BOXARTS_DIR = QStringLiteral("Named_Boxarts");
inline const QString SNAPS_DIR = QStringLiteral("Named_Snaps");
inline const QString TITLES_DIR = QStringLiteral("Named_Titles");
}

inline QString retroArchPlaylistNameForSystemId(int systemId)
{
    using namespace Systems;

    switch (systemId) {
    case ID_NES: return QStringLiteral("Nintendo - Nintendo Entertainment System");
    case ID_SNES: return QStringLiteral("Nintendo - Super Nintendo Entertainment System");
    case ID_N64: return QStringLiteral("Nintendo - Nintendo 64");
    case ID_GB: return QStringLiteral("Nintendo - Game Boy");
    case ID_GBC: return QStringLiteral("Nintendo - Game Boy Color");
    case ID_GBA: return QStringLiteral("Nintendo - Game Boy Advance");
    case ID_NDS: return QStringLiteral("Nintendo - Nintendo DS");
    case ID_GAMECUBE: return QStringLiteral("Nintendo - GameCube");
    case ID_WII: return QStringLiteral("Nintendo - Wii");
    case ID_GENESIS: return QStringLiteral("Sega - Mega Drive - Genesis");
    case ID_MASTER_SYSTEM: return QStringLiteral("Sega - Master System - Mark III");
    case ID_GAME_GEAR: return QStringLiteral("Sega - Game Gear");
    case ID_SATURN: return QStringLiteral("Sega - Saturn");
    case ID_DREAMCAST: return QStringLiteral("Sega - Dreamcast");
    case ID_SEGA_CD: return QStringLiteral("Sega - Mega-CD - Sega CD");
    case ID_32X: return QStringLiteral("Sega - 32X");
    case ID_PSX: return QStringLiteral("Sony - PlayStation");
    case ID_PS2: return QStringLiteral("Sony - PlayStation 2");
    case ID_PSP: return QStringLiteral("Sony - PlayStation Portable");
    case ID_PSVITA: return QStringLiteral("Sony - PlayStation Vita");
    case ID_TURBOGRAFX16: return QStringLiteral("NEC - PC Engine - TurboGrafx 16");
    case ID_TURBOGRAFX_CD: return QStringLiteral("NEC - PC Engine CD - TurboGrafx-CD");
    case ID_NEO_GEO: return QStringLiteral("SNK - Neo Geo");
    case ID_NGP: return QStringLiteral("SNK - Neo Geo Pocket");
    case ID_ARCADE: return QStringLiteral("MAME");
    case ID_ATARI_2600: return QStringLiteral("Atari - 2600");
    case ID_ATARI_7800: return QStringLiteral("Atari - 7800");
    case ID_LYNX: return QStringLiteral("Atari - Lynx");
    case ID_ATARI_JAGUAR: return QStringLiteral("Atari - Jaguar");
    case ID_WONDERSWAN: return QStringLiteral("Bandai - WonderSwan");
    case ID_VIRTUAL_BOY: return QStringLiteral("Nintendo - Virtual Boy");
    default: return QString();
    }
}

inline QString launchBoxPlatformNameForSystemId(int systemId)
{
    using namespace Systems;

    switch (systemId) {
    case ID_NES: return QStringLiteral("Nintendo Entertainment System");
    case ID_SNES: return QStringLiteral("Super Nintendo Entertainment System");
    case ID_N64: return QStringLiteral("Nintendo 64");
    case ID_GB: return QStringLiteral("Nintendo Game Boy");
    case ID_GBC: return QStringLiteral("Nintendo Game Boy Color");
    case ID_GBA: return QStringLiteral("Nintendo Game Boy Advance");
    case ID_NDS: return QStringLiteral("Nintendo DS");
    case ID_GAMECUBE: return QStringLiteral("Nintendo GameCube");
    case ID_WII: return QStringLiteral("Nintendo Wii");
    case ID_GENESIS: return QStringLiteral("Sega Genesis");
    case ID_MASTER_SYSTEM: return QStringLiteral("Sega Master System");
    case ID_GAME_GEAR: return QStringLiteral("Sega Game Gear");
    case ID_SATURN: return QStringLiteral("Sega Saturn");
    case ID_DREAMCAST: return QStringLiteral("Sega Dreamcast");
    case ID_SEGA_CD: return QStringLiteral("Sega CD");
    case ID_32X: return QStringLiteral("Sega 32X");
    case ID_PSX: return QStringLiteral("Sony PlayStation");
    case ID_PS2: return QStringLiteral("Sony PlayStation 2");
    case ID_PSP: return QStringLiteral("Sony PSP");
    case ID_PSVITA: return QStringLiteral("Sony PlayStation Vita");
    case ID_TURBOGRAFX16: return QStringLiteral("TurboGrafx-16");
    case ID_TURBOGRAFX_CD: return QStringLiteral("TurboGrafx-CD");
    case ID_NEO_GEO: return QStringLiteral("SNK Neo Geo");
    case ID_NGP: return QStringLiteral("SNK Neo Geo Pocket");
    case ID_ARCADE: return QStringLiteral("Arcade");
    case ID_ATARI_2600: return QStringLiteral("Atari 2600");
    case ID_ATARI_7800: return QStringLiteral("Atari 7800");
    case ID_LYNX: return QStringLiteral("Atari Lynx");
    case ID_ATARI_JAGUAR: return QStringLiteral("Atari Jaguar");
    case ID_WONDERSWAN: return QStringLiteral("Bandai WonderSwan");
    default: return QString();
    }
}

inline QString retroArchThumbnailDirectory(const QString &type)
{
    const QString normalized = type.trimmed().toLower();
    if (normalized == QStringLiteral("boxart") || normalized == QStringLiteral("cover")) {
        return RetroArch::BOXARTS_DIR;
    }
    if (normalized == QStringLiteral("screenshot") || normalized == QStringLiteral("snap")) {
        return RetroArch::SNAPS_DIR;
    }
    if (normalized == QStringLiteral("title") || normalized == QStringLiteral("titlescreen")) {
        return RetroArch::TITLES_DIR;
    }
    return RetroArch::BOXARTS_DIR;
}

} // Exports
} // Constants
} // Remus