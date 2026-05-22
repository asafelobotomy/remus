#include "system_resolver.h"

#include <QRegularExpression>

namespace Remus {

using namespace Constants::Systems;

int SystemResolver::systemIdByDatName(const QString &datName)
{
    auto normalizeDatName = [](QString value) {
        static const QRegularExpression parenRe(QStringLiteral("\\s*\\([^)]*\\)"));
        static const QRegularExpression dashSpacingRe(QStringLiteral("\\s*-\\s*"));
        static const QRegularExpression whitespaceRe(QStringLiteral("\\s+"));

        // Remove common suffix qualifiers that do not change the platform.
        value.remove(parenRe);

        // Normalize Unicode dash variants to ASCII '-' without regex escapes.
        for (qsizetype i = 0; i < value.size(); ++i) {
            const ushort code = value.at(i).unicode();
            if (code >= 0x2010 && code <= 0x2015) {
                value[i] = QLatin1Char('-');
            }
        }

        // Normalize dash spacing and collapse whitespace.
        value.replace(dashSpacingRe, QStringLiteral(" - "));
        value.replace(whitespaceRe, QStringLiteral(" "));
        return value.trimmed().toLower();
    };

    static const QMap<QString, int> datNameMap = {
        {QStringLiteral("Nintendo - Nintendo Entertainment System"), ID_NES},
        {QStringLiteral("Nintendo - Family Computer Disk System"), ID_FDS},
        {QStringLiteral("Nintendo - Super Nintendo Entertainment System"), ID_SNES},
        {QStringLiteral("Nintendo - Nintendo 64"), ID_N64},
        {QStringLiteral("Nintendo - GameCube"), ID_GAMECUBE},
        {QStringLiteral("Nintendo - Nintendo GameCube"), ID_GAMECUBE},
        {QStringLiteral("Nintendo - Wii"), ID_WII},
        {QStringLiteral("Nintendo - Nintendo Wii"), ID_WII},
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
        {QStringLiteral("SNK - Neo Geo"), ID_NEO_GEO},
        {QStringLiteral("SNK - Neo Geo CD"), ID_NEO_GEO_CD},
        {QStringLiteral("SNK - Neo Geo Pocket"), ID_NGP},
        {QStringLiteral("SNK - Neo Geo Pocket Color"), ID_NGP},
        {QStringLiteral("Bandai - WonderSwan"), ID_WONDERSWAN},
        {QStringLiteral("Bandai - WonderSwan Color"), ID_WONDERSWAN},
        {QStringLiteral("Bandai - Playdia Quick Interactive System"), ID_PLAYDIA},
        {QStringLiteral("Commodore - 64"), ID_C64},
        {QStringLiteral("Commodore - Amiga"), ID_AMIGA},
        {QStringLiteral("Commodore - CD32"), ID_CD32},
        {QStringLiteral("Commodore - CDTV"), ID_CDTV},
        {QStringLiteral("Commodore - Plus-4"), ID_C16},
        {QStringLiteral("Sinclair - ZX Spectrum"), ID_ZX_SPECTRUM},
        {QStringLiteral("Sinclair - ZX Spectrum +3"), ID_ZX_SPECTRUM},
        {QStringLiteral("Microsoft - Xbox"), ID_XBOX},
        {QStringLiteral("Microsoft - Xbox 360"), ID_XBOX360},
        {QStringLiteral("Microsoft - MSX"), ID_MSX},
        {QStringLiteral("Microsoft - MSX2"), ID_MSX2},
        {QStringLiteral("The 3DO Company - 3DO"), ID_3DO},
        // Atari expanded systems
        {QStringLiteral("Atari - 5200"), ID_ATARI_5200},
        {QStringLiteral("Atari - 8-bit Family"), ID_ATARI_8BIT},
        {QStringLiteral("Atari - ST"), ID_ATARI_ST},
        {QStringLiteral("Atari - Jaguar CD"), ID_ATARI_JAGUAR_CD},
        // NEC expanded systems
        {QStringLiteral("NEC - PC-FX"), ID_PC_FX},
        {QStringLiteral("NEC - PC Engine - TurboGrafx 16"), ID_TURBOGRAFX16},
        // Philips
        {QStringLiteral("Philips - CD-i"), ID_CDI},
        // Sega expanded systems
        {QStringLiteral("Sega - SG-1000"), ID_SG1000},
        {QStringLiteral("Sega - Naomi"), ID_NAOMI},
        {QStringLiteral("Sega - Naomi 2"), ID_NAOMI},
        // Coleco / Mattel
        {QStringLiteral("Coleco - ColecoVision"), ID_COLECOVISION},
        {QStringLiteral("Mattel - Intellivision"), ID_INTELLIVISION},
        // Sony expanded
        {QStringLiteral("Sony - PlayStation 3"), ID_PS3},
        // Nintendo expanded
        {QStringLiteral("Nintendo - Wii U"), ID_WIIU},
        {QStringLiteral("Nintendo - Nintendo Wii U"), ID_WIIU},
        // Digital / download / accessory variants that map to the same system
        {QStringLiteral("Nintendo - Wii (Digital)"), ID_WII},
        {QStringLiteral("Nintendo - Wii U (Digital)"), ID_WIIU},
        {QStringLiteral("Nintendo - New Nintendo 3DS"), ID_3DS},
        {QStringLiteral("Nintendo - New Nintendo 3DS (Digital)"), ID_3DS},
        {QStringLiteral("Nintendo - Nintendo DSi"), ID_NDS},
        {QStringLiteral("Nintendo - Nintendo DS (Download Play)"), ID_NDS},
        {QStringLiteral("Nintendo - Nintendo 64DD"), ID_N64},
        {QStringLiteral("Nintendo - Satellaview"), ID_SNES},
        {QStringLiteral("Nintendo - Sufami Turbo"), ID_SNES},
        {QStringLiteral("Nintendo - e-Reader"), ID_GBA},
        {QStringLiteral("Microsoft - XBOX 360 (Games on Demand)"), ID_XBOX360},
        {QStringLiteral("Microsoft - XBOX 360 (Title Updates)"), ID_XBOX360},
        {QStringLiteral("Microsoft - Xbox 360 (Digital)"), ID_XBOX360},
        {QStringLiteral("Sony - PlayStation 3 (PSN)"), ID_PS3},
        {QStringLiteral("Sony - PlayStation Portable (PSN)"), ID_PSP},
        {QStringLiteral("Sony - PlayStation Portable (PSX2PSP)"), ID_PSP},
        {QStringLiteral("Sony - PlayStation Vita (PSN)"), ID_PSVITA},
        {QStringLiteral("Commodore - Amiga - WHDLoad"), ID_AMIGA},
        {QStringLiteral("Commodore - Amiga CD"), ID_AMIGA_CD},
        // New systems (IDs 58-82)
        {QStringLiteral("Amstrad - CPC"), ID_AMSTRAD_CPC},
        {QStringLiteral("Amstrad - CPC - clean-cpc-db"), ID_AMSTRAD_CPC},
        {QStringLiteral("Amstrad - GX4000"), ID_AMSTRAD_CPC},
        {QStringLiteral("Enterprise - 128"), ID_ENTERPRISE_128},
        {QStringLiteral("Enterprise - 64"), ID_ENTERPRISE_128},
        {QStringLiteral("Sinclair - ZX 81"), ID_ZX81},
        {QStringLiteral("Videoton - TV-Computer"), ID_VIDEOTON_TVC},
        {QStringLiteral("Sega - PICO"), ID_SEGA_PICO},
        {QStringLiteral("Commodore - VIC-20"), ID_VIC20},
        {QStringLiteral("Magnavox - Odyssey2"), ID_ODYSSEY2},
        {QStringLiteral("Philips - Videopac+"), ID_ODYSSEY2},
        {QStringLiteral("Philips - Videopac"), ID_ODYSSEY2},
        {QStringLiteral("Watara - Supervision"), ID_SUPERVISION},
        {QStringLiteral("Benesse - Pocket Challenge V2"), ID_POCKET_CHALLENGE_V2},
        {QStringLiteral("NEC - PC-98"), ID_PC98},
        {QStringLiteral("NEC - PC98"), ID_PC98},
        {QStringLiteral("NEC - PC-88 series"), ID_PC88},
        {QStringLiteral("Interton - VC 4000"), ID_INTERTON_VC4000},
        {QStringLiteral("Emerson - Arcadia 2001"), ID_ARCADIA_2001},
        {QStringLiteral("GCE - Vectrex"), ID_VECTREX},
        {QStringLiteral("Nintendo - Pokemon Mini"), ID_POKEMON_MINI},
        {QStringLiteral("Fairchild - Channel F"), ID_CHANNEL_F},
        {QStringLiteral("Epoch - Super Cassette Vision"), ID_SCV},
        {QStringLiteral("GamePark - GP32"), ID_GP32},
        {QStringLiteral("Tiger - Game.com"), ID_GAMECOM},
        {QStringLiteral("RCA - Studio II"), ID_STUDIO_II},
        {QStringLiteral("Atomiswave"), ID_ATOMISWAVE},
        {QStringLiteral("Sammy - Atomiswave"), ID_ATOMISWAVE},
        {QStringLiteral("Arcade - Sega - Chihiro"), ID_CHIHIRO},
        {QStringLiteral("Arcade - Sega - Lindbergh"), ID_LINDBERGH},
        {QStringLiteral("Arcade - Namco - Sega - Nintendo - Triforce"), ID_TRIFORCE},
        // MAME — any DAT whose name starts with / contains "MAME" maps to Arcade.
        // Covers the official mamedev release XML ("MAME 0.287") as well as
        // Pleasuredome merged/non-merged/split sets.
        {QStringLiteral("MAME"), ID_ARCADE},
        {QStringLiteral("MAME ROMs (merged)"), ID_ARCADE},
        {QStringLiteral("MAME ROMs (non-merged)"), ID_ARCADE},
        {QStringLiteral("MAME ROMs (split)"), ID_ARCADE},
        {QStringLiteral("Casio - PV-1000"), ID_CASIO_PV1000},
        {QStringLiteral("Funtech - Super Acan"), ID_SUPER_ACAN},
        {QStringLiteral("Casio - Loopy"), ID_CASIO_LOOPY},
        {QStringLiteral("Sharp - X1"), ID_SHARP_X1},
        {QStringLiteral("Sharp - X68000"), ID_X68000},
        // Fujitsu
        {QStringLiteral("Fujitsu - FM-Towns"), ID_FM_TOWNS},
        // Acorn
        {QStringLiteral("Acorn - Archimedes"), ID_ARCHIMEDES},
        // Apple
        {QStringLiteral("Apple - Macintosh"), ID_MAC},
        // Tandy / Memorex
        {QStringLiteral("Memorex - Visual Information System"), ID_TANDY_VIS},
        // IBM PC / DOS
        {QStringLiteral("DOS"), ID_IBM_PC},
        // Mobile
        {QStringLiteral("Mobile - Palm OS"), ID_PALM_OS},
        // Educational / children's platforms
        {QStringLiteral("VTech - V.Smile"), ID_VSMILE},
        {QStringLiteral("LeapFrog - Leapster Learning Game System"), ID_LEAPSTER},
        {QStringLiteral("LeapFrog - LeapPad"), ID_LEAPPAD},
        {QStringLiteral("Sega - Beena"), ID_SEGA_BEENA},
        {QStringLiteral("VTech - CreatiVision"), ID_CREATIVISION},
        {QStringLiteral("Konami - Picno"), ID_PICNO},
        {QStringLiteral("Hartung - Game Master"), ID_GAME_MASTER},
        {QStringLiteral("Mobile - Symbian"), ID_SYMBIAN},
        {QStringLiteral("Entex - Adventure Vision"), ID_ADVENTURE_VISION},
    };

    static QMap<QString, int> normalizedDatNameMap;
    if (normalizedDatNameMap.isEmpty()) {
        for (auto it = datNameMap.constBegin(); it != datNameMap.constEnd(); ++it) {
            normalizedDatNameMap.insert(normalizeDatName(it.key()), it.value());
        }
        // Common alias where upstream alternates between -16 and 16.
        normalizedDatNameMap.insert(normalizeDatName(QStringLiteral("NEC - PC Engine - TurboGrafx 16")), ID_TURBOGRAFX16);
    }

    if (datNameMap.contains(datName)) {
        return datNameMap.value(datName);
    }

    const QString normalizedInput = normalizeDatName(datName);
    if (normalizedDatNameMap.contains(normalizedInput)) {
        return normalizedDatNameMap.value(normalizedInput);
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