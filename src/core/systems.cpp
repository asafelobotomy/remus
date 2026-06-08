// System group lists and utility function implementations.
// Data tables are split into:
//   systems_defs.cpp       — SYSTEMS QMap
//   systems_extensions.cpp — EXTENSION_TO_SYSTEMS QMap
#include "constants/systems.h"

namespace Remus {
namespace Constants {
    namespace Systems {

        const QList<int> NINTENDO_SYSTEMS = { ID_NES, ID_FDS, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, ID_NDS,
            ID_GAMECUBE, ID_WII, ID_WIIU, ID_VIRTUAL_BOY, ID_3DS, ID_SWITCH, ID_POKEMON_MINI };
        const QList<int> SEGA_SYSTEMS = { ID_SG1000, ID_MASTER_SYSTEM, ID_GENESIS, ID_SEGA_CD, ID_SATURN, ID_DREAMCAST,
            ID_GAME_GEAR, ID_32X, ID_NAOMI, ID_SEGA_PICO, ID_ATOMISWAVE, ID_TRIFORCE, ID_CHIHIRO, ID_LINDBERGH };
        const QList<int> SONY_SYSTEMS = { ID_PSX, ID_PS2, ID_PS3, ID_PS4, ID_PSP, ID_PSVITA };
        const QList<int> MICROSOFT_SYSTEMS = { ID_XBOX, ID_XBOX360, ID_XBOX_ONE };
        const QList<int> HANDHELD_SYSTEMS = { ID_GB, ID_GBC, ID_GBA, ID_NDS, ID_PSP, ID_LYNX, ID_GAME_GEAR, ID_NGP,
            ID_WONDERSWAN, ID_VIRTUAL_BOY, ID_3DS, ID_PSVITA, ID_SWITCH, ID_SUPERVISION, ID_POCKET_CHALLENGE_V2,
            ID_POKEMON_MINI, ID_GP32, ID_GAMECOM, ID_PALM_OS, ID_MEGA_DUCK, ID_MICROVISION };
        const QList<int> DISC_SYSTEMS = { ID_PSX, ID_PS2, ID_PS3, ID_PS4, ID_GAMECUBE, ID_WII, ID_DREAMCAST, ID_SATURN,
            ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_3DS, ID_SWITCH, ID_XBOX, ID_XBOX360, ID_XBOX_ONE, ID_3DO, ID_NEO_GEO_CD,
            ID_PC_FX, ID_CDI, ID_CD32, ID_NAOMI, ID_ATARI_JAGUAR_CD, ID_IBM_PC, ID_MAC, ID_FM_TOWNS, ID_CDTV,
            ID_AMIGA_CD, ID_ARCHIMEDES, ID_TRIFORCE, ID_CHIHIRO, ID_TANDY_VIS, ID_LINDBERGH, ID_PLAYDIA, ID_NUON };
        const QList<int> CARTRIDGE_SYSTEMS = { ID_NES, ID_FDS, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, ID_NDS,
            ID_GENESIS, ID_MASTER_SYSTEM, ID_SG1000, ID_ATARI_2600, ID_ATARI_5200, ID_ATARI_7800, ID_ATARI_8BIT,
            ID_ATARI_JAGUAR, ID_LYNX, ID_TURBOGRAFX16, ID_NEO_GEO, ID_GAME_GEAR, ID_32X, ID_NGP, ID_WONDERSWAN,
            ID_VIRTUAL_BOY, ID_SUPERGRAFX, ID_COLECOVISION, ID_INTELLIVISION, ID_MSX, ID_MSX2, ID_ODYSSEY2,
            ID_INTERTON_VC4000, ID_ARCADIA_2001, ID_VECTREX, ID_POKEMON_MINI, ID_CHANNEL_F, ID_SCV, ID_STUDIO_II,
            ID_CASIO_PV1000, ID_SUPER_ACAN, ID_CASIO_LOOPY, ID_SUPERVISION, ID_POCKET_CHALLENGE_V2, ID_GP32, ID_GAMECOM,
            ID_SEGA_PICO, ID_APPLE_II, ID_BBC_MICRO, ID_C16, ID_MEGA_DUCK, ID_MICROVISION };
        const QList<int> COMPUTER_SYSTEMS = { ID_C64, ID_AMIGA, ID_ZX_SPECTRUM, ID_ATARI_ST, ID_ATARI_8BIT, ID_MSX,
            ID_MSX2, ID_AMSTRAD_CPC, ID_ENTERPRISE_128, ID_ZX81, ID_VIDEOTON_TVC, ID_VIC20, ID_PC98, ID_SHARP_X1,
            ID_X68000, ID_IBM_PC, ID_MAC, ID_FM_TOWNS, ID_PC88, ID_APPLE_II, ID_BBC_MICRO, ID_C16 };

        const SystemDef *getSystem(int systemId) {
            const auto it = SYSTEMS.find(systemId);
            return (it != SYSTEMS.end()) ? &it.value() : nullptr;
        }

        int getSystemIdByName(const QString &name) {
            for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
                if (it.value().internalName == name) {
                    return it.key();
                }
            }
            return 0;
        }

        const SystemDef *getSystemByName(const QString &name) {
            const int id = getSystemIdByName(name);
            return (id > 0) ? getSystem(id) : nullptr;
        }

        QStringList getSystemDisplayNames() {
            QStringList names;
            for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
                names << it.value().displayName;
            }
            return names;
        }

        QStringList getSystemInternalNames() {
            QStringList names;
            for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
                names << it.value().internalName;
            }
            return names;
        }

        QList<int> getSystemsForExtension(const QString &extension) {
            const auto it = EXTENSION_TO_SYSTEMS.find(extension.toLower());
            return (it != EXTENSION_TO_SYSTEMS.end()) ? it.value() : QList<int>();
        }

        bool isAmbiguousExtension(const QString &extension) {
            return getSystemsForExtension(extension).size() > 1;
        }

    } // namespace Systems
} // namespace Constants
} // namespace Remus
