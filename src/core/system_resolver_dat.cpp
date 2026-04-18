#include "system_resolver.h"

namespace Remus {

using namespace Constants::Systems;

int SystemResolver::systemIdByDatName(const QString &datName)
{
    static const QMap<QString, int> datNameMap = {
        {QStringLiteral("Nintendo - Nintendo Entertainment System"), ID_NES},
        {QStringLiteral("Nintendo - Family Computer Disk System"), ID_NES},
        {QStringLiteral("Nintendo - Super Nintendo Entertainment System"), ID_SNES},
        {QStringLiteral("Nintendo - Nintendo 64"), ID_N64},
        {QStringLiteral("Nintendo - GameCube"), ID_GAMECUBE},
        {QStringLiteral("Nintendo - Wii"), ID_WII},
        {QStringLiteral("Nintendo - Game Boy"), ID_GB},
        {QStringLiteral("Nintendo - Game Boy Color"), ID_GBC},
        {QStringLiteral("Nintendo - Game Boy Advance"), ID_GBA},
        {QStringLiteral("Nintendo - Nintendo DS"), ID_NDS},
        {QStringLiteral("Nintendo - Nintendo 3DS"), ID_3DS},
        {QStringLiteral("Nintendo - Switch"), ID_SWITCH},
        {QStringLiteral("Nintendo - Virtual Boy"), ID_VIRTUAL_BOY},
        {QStringLiteral("Sega - Mega Drive - Genesis"), ID_GENESIS},
        {QStringLiteral("Sega - Master System - Mark III"), ID_MASTER_SYSTEM},
        {QStringLiteral("Sega - Game Gear"), ID_GAME_GEAR},
        {QStringLiteral("Sega - Saturn"), ID_SATURN},
        {QStringLiteral("Sega - Dreamcast"), ID_DREAMCAST},
        {QStringLiteral("Sega - Mega-CD - Sega CD"), ID_SEGA_CD},
        {QStringLiteral("Sega - 32X"), ID_32X},
        {QStringLiteral("Sony - PlayStation"), ID_PSX},
        {QStringLiteral("Sony - PlayStation 2"), ID_PS2},
        {QStringLiteral("Sony - PlayStation Portable"), ID_PSP},
        {QStringLiteral("Sony - PlayStation Vita"), ID_PSVITA},
        {QStringLiteral("Atari - 2600"), ID_ATARI_2600},
        {QStringLiteral("Atari - 7800"), ID_ATARI_7800},
        {QStringLiteral("Atari - Lynx"), ID_LYNX},
        {QStringLiteral("Atari - Jaguar"), ID_ATARI_JAGUAR},
        {QStringLiteral("NEC - PC Engine - TurboGrafx-16"), ID_TURBOGRAFX16},
        {QStringLiteral("NEC - PC Engine CD - TurboGrafx-CD"), ID_TURBOGRAFX_CD},
        {QStringLiteral("NEC - PC Engine SuperGrafx"), ID_SUPERGRAFX},
        {QStringLiteral("SNK - Neo Geo Pocket"), ID_NGP},
        {QStringLiteral("SNK - Neo Geo Pocket Color"), ID_NGP},
        {QStringLiteral("Bandai - WonderSwan"), ID_WONDERSWAN},
        {QStringLiteral("Bandai - WonderSwan Color"), ID_WONDERSWAN},
        {QStringLiteral("Commodore - 64"), ID_C64},
        {QStringLiteral("Commodore - Amiga"), ID_AMIGA},
        {QStringLiteral("Sinclair - ZX Spectrum"), ID_ZX_SPECTRUM},
        {QStringLiteral("Microsoft - Xbox"), ID_XBOX},
        {QStringLiteral("Microsoft - Xbox 360"), ID_XBOX360},
    };

    if (datNameMap.contains(datName)) {
        return datNameMap.value(datName);
    }

    for (auto it = datNameMap.constBegin(); it != datNameMap.constEnd(); ++it) {
        if (it.key().compare(datName, Qt::CaseInsensitive) == 0) {
            return it.value();
        }
    }

    for (auto it = datNameMap.constBegin(); it != datNameMap.constEnd(); ++it) {
        if (datName.contains(it.key(), Qt::CaseInsensitive)) {
            return it.value();
        }
    }

    return 0;
}

} // namespace Remus