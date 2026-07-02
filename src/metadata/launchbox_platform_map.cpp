#include "launchbox_platform_map.h"

#include "constants/system_ids.h"

#include <QHash>
#include <QRegularExpression>

namespace Remus {
namespace LaunchBoxPlatformMap {

    using namespace Constants::Systems;

    namespace {

        QHash<QString, QList<int>> buildPlatformMap() {
            QHash<QString, QList<int>> map;

            auto add = [&](const QString &platform, std::initializer_list<int> ids) {
                map.insert(platform, QList<int>(ids));
            };

            // Nintendo
            add(QStringLiteral("Nintendo Entertainment System"), { ID_NES });
            add(QStringLiteral("Super Nintendo Entertainment System"), { ID_SNES });
            add(QStringLiteral("Nintendo 64"), { ID_N64 });
            add(QStringLiteral("Nintendo GameCube"), { ID_GAMECUBE });
            add(QStringLiteral("Nintendo Wii"), { ID_WII });
            add(QStringLiteral("Nintendo Wii U"), { ID_WIIU });
            add(QStringLiteral("Nintendo Game Boy"), { ID_GB });
            add(QStringLiteral("Nintendo Game Boy Color"), { ID_GBC });
            add(QStringLiteral("Nintendo Game Boy Advance"), { ID_GBA });
            add(QStringLiteral("Nintendo DS"), { ID_NDS });
            add(QStringLiteral("Nintendo 3DS"), { ID_3DS });
            add(QStringLiteral("Nintendo Switch"), { ID_SWITCH });
            add(QStringLiteral("Nintendo Virtual Boy"), { ID_VIRTUAL_BOY });
            add(QStringLiteral("Nintendo Famicom Disk System"), { ID_FDS });
            add(QStringLiteral("Nintendo Pokemon Mini"), { ID_POKEMON_MINI });

            // Sony (LaunchBox uses "Playstation" spelling)
            add(QStringLiteral("Sony Playstation"), { ID_PSX });
            add(QStringLiteral("Sony Playstation 2"), { ID_PS2 });
            add(QStringLiteral("Sony Playstation 3"), { ID_PS3 });
            add(QStringLiteral("Sony Playstation 4"), { ID_PS4 });
            add(QStringLiteral("Sony PSP"), { ID_PSP });
            add(QStringLiteral("Sony Playstation Vita"), { ID_PSVITA });

            // Sega
            add(QStringLiteral("Sega Genesis"), { ID_GENESIS });
            add(QStringLiteral("Sega Master System"), { ID_MASTER_SYSTEM });
            add(QStringLiteral("Sega Saturn"), { ID_SATURN });
            add(QStringLiteral("Sega Dreamcast"), { ID_DREAMCAST });
            add(QStringLiteral("Sega CD"), { ID_SEGA_CD });
            add(QStringLiteral("Sega 32X"), { ID_32X });
            add(QStringLiteral("Sega Game Gear"), { ID_GAME_GEAR });
            add(QStringLiteral("Sega SG-1000"), { ID_SG1000 });
            add(QStringLiteral("Sega Pico"), { ID_SEGA_PICO });

            // Atari
            add(QStringLiteral("Atari 2600"), { ID_ATARI_2600 });
            add(QStringLiteral("Atari 5200"), { ID_ATARI_5200 });
            add(QStringLiteral("Atari 7800"), { ID_ATARI_7800 });
            add(QStringLiteral("Atari Lynx"), { ID_LYNX });
            add(QStringLiteral("Atari Jaguar"), { ID_ATARI_JAGUAR });
            add(QStringLiteral("Atari Jaguar CD"), { ID_ATARI_JAGUAR_CD });
            add(QStringLiteral("Atari ST"), { ID_ATARI_ST });
            add(QStringLiteral("Atari 800"), { ID_ATARI_8BIT });

            // Commodore / Amiga / Sinclair
            add(QStringLiteral("Commodore 64"), { ID_C64 });
            add(QStringLiteral("Commodore Amiga"), { ID_AMIGA });
            add(QStringLiteral("Commodore Amiga CD32"), { ID_CD32 });
            add(QStringLiteral("Commodore VIC-20"), { ID_VIC20 });
            add(QStringLiteral("Sinclair ZX Spectrum"), { ID_ZX_SPECTRUM });
            add(QStringLiteral("ZX Spectrum"), { ID_ZX_SPECTRUM });
            add(QStringLiteral("Sinclair ZX-81"), { ID_ZX81 });

            // Microsoft / PC
            add(QStringLiteral("MS-DOS"), { ID_IBM_PC });
            add(QStringLiteral("Windows"), { ID_IBM_PC });
            add(QStringLiteral("Windows 3.X"), { ID_IBM_PC });
            add(QStringLiteral("Steam"), { ID_IBM_PC });
            add(QStringLiteral("Linux"), { ID_IBM_PC });
            add(QStringLiteral("Microsoft Xbox"), { ID_XBOX });
            add(QStringLiteral("Microsoft Xbox 360"), { ID_XBOX360 });
            add(QStringLiteral("Microsoft Xbox One"), { ID_XBOX_ONE });
            add(QStringLiteral("Microsoft MSX"), { ID_MSX });
            add(QStringLiteral("Microsoft MSX2"), { ID_MSX2 });

            // NEC / TurboGrafx
            add(QStringLiteral("NEC TurboGrafx-16"), { ID_TURBOGRAFX16 });
            add(QStringLiteral("NEC TurboGrafx-CD"), { ID_TURBOGRAFX_CD });
            add(QStringLiteral("PC Engine SuperGrafx"), { ID_SUPERGRAFX });

            // Other common platforms
            add(QStringLiteral("3DO Interactive Multiplayer"), { ID_3DO });
            add(QStringLiteral("Neo Geo AES"), { ID_NEO_GEO });
            add(QStringLiteral("Neo Geo CD"), { ID_NEO_GEO_CD });
            add(QStringLiteral("SNK Neo Geo AES"), { ID_NEO_GEO });
            add(QStringLiteral("SNK Neo Geo CD"), { ID_NEO_GEO_CD });
            add(QStringLiteral("Arcade"), { ID_ARCADE });
            add(QStringLiteral("Amstrad CPC"), { ID_AMSTRAD_CPC });
            add(QStringLiteral("ColecoVision"), { ID_COLECOVISION });
            add(QStringLiteral("Mattel Intellivision"), { ID_INTELLIVISION });
            add(QStringLiteral("Magnavox Odyssey 2"), { ID_ODYSSEY2 });
            add(QStringLiteral("GCE Vectrex"), { ID_VECTREX });
            add(QStringLiteral("Fairchild Channel F"), { ID_CHANNEL_F });
            add(QStringLiteral("Casio Loopy"), { ID_CASIO_LOOPY });
            add(QStringLiteral("Funtech Super Acan"), { ID_SUPER_ACAN });
            add(QStringLiteral("WonderSwan"), { ID_WONDERSWAN });
            add(QStringLiteral("WonderSwan Color"), { ID_WONDERSWAN });
            add(QStringLiteral("Philips CD-i"), { ID_CDI });
            add(QStringLiteral("Enterprise"), { ID_ENTERPRISE_128 });

            return map;
        }

        const QHash<QString, QList<int>> &platformMap() {
            static const QHash<QString, QList<int>> map = buildPlatformMap();
            return map;
        }

    } // namespace

    QString normalizePlatformKey(const QString &platform) {
        QString key = platform.toLower();
        key.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
        return key;
    }

    QList<int> resolveSystemIds(const QString &launchBoxPlatform) {
        const auto it = platformMap().constFind(launchBoxPlatform.trimmed());
        if (it != platformMap().constEnd())
            return *it;
        return { };
    }

    bool platformKeysCompatible(const QString &remusPlatform, const QString &launchBoxPlatform) {
        const QString remusKey = normalizePlatformKey(remusPlatform);
        const QString lbKey = normalizePlatformKey(launchBoxPlatform);
        if (remusKey.isEmpty() || lbKey.isEmpty())
            return false;
        if (remusKey == lbKey)
            return true;

        const QStringList parts = remusPlatform.split(QStringLiteral(" / "));
        for (const QString &part : parts) {
            const QString partKey = normalizePlatformKey(part);
            if (!partKey.isEmpty() && (lbKey.contains(partKey) || partKey.contains(lbKey)))
                return true;
        }
        return remusKey.contains(lbKey) || lbKey.contains(remusKey);
    }

} // namespace LaunchBoxPlatformMap
} // namespace Remus
