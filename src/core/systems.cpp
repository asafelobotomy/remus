#include "constants/systems.h"

namespace Remus {
namespace Constants {
namespace Systems {

const QMap<int, SystemDef> SYSTEMS = {
    {ID_NES, {ID_NES, QStringLiteral("NES"), QStringLiteral("Nintendo Entertainment System"), QStringLiteral("Nintendo"), 3,
              {QStringLiteral(".nes"), QStringLiteral(".nez"), QStringLiteral(".unf"), QStringLiteral(".unif")},
              QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#e74c3c"), 1983}},
    {ID_FDS, {ID_FDS, QStringLiteral("FDS"), QStringLiteral("Nintendo Family Computer Disk System"), QStringLiteral("Nintendo"), 3,
              {QStringLiteral(".fds")},
              QStringLiteral("CRC32"), {QStringLiteral("JPN")}, false, QStringLiteral("#c0392b"), 1986}},
    {ID_MASTER_SYSTEM, {ID_MASTER_SYSTEM, QStringLiteral("Master System"), QStringLiteral("Sega Master System"), QStringLiteral("Sega"), 3,
                        {QStringLiteral(".sms")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR"), QStringLiteral("BRA")}, false, QStringLiteral("#000000"), 1985}},
    {ID_ATARI_2600, {ID_ATARI_2600, QStringLiteral("Atari 2600"), QStringLiteral("Atari 2600"), QStringLiteral("Atari"), 2,
                     {QStringLiteral(".a26"), QStringLiteral(".bin")}, QStringLiteral("CRC32"), {QStringLiteral("USA")}, false, QStringLiteral("#d35400"), 1977}},
    {ID_ATARI_5200, {ID_ATARI_5200, QStringLiteral("Atari 5200"), QStringLiteral("Atari 5200 SuperSystem"), QStringLiteral("Atari"), 2,
                     {QStringLiteral(".a52"), QStringLiteral(".bin"), QStringLiteral(".car")}, QStringLiteral("CRC32"), {QStringLiteral("USA")}, false, QStringLiteral("#e67e22"), 1982}},
    {ID_ATARI_8BIT, {ID_ATARI_8BIT, QStringLiteral("Atari 8-bit"), QStringLiteral("Atari 8-bit Family (400/800/XL/XE)"), QStringLiteral("Atari"), 2,
                     {QStringLiteral(".atr"), QStringLiteral(".xex"), QStringLiteral(".xfd"), QStringLiteral(".atx"), QStringLiteral(".car"), QStringLiteral(".bin")},
                     QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#e67e22"), 1979}},
    {ID_ATARI_ST, {ID_ATARI_ST, QStringLiteral("Atari ST"), QStringLiteral("Atari ST / STE / TT / Falcon"), QStringLiteral("Atari"), 3,
                   {QStringLiteral(".st"), QStringLiteral(".msa"), QStringLiteral(".stx"), QStringLiteral(".dim"), QStringLiteral(".img")},
                   QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#e67e22"), 1985}},
    {ID_ATARI_JAGUAR_CD, {ID_ATARI_JAGUAR_CD, QStringLiteral("Jaguar CD"), QStringLiteral("Atari Jaguar CD"), QStringLiteral("Atari"), 5,
                          {QStringLiteral(".j64"), QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".chd")},
                          QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("EUR")}, true, QStringLiteral("#d35400"), 1995}},
    {ID_ATARI_7800, {ID_ATARI_7800, QStringLiteral("Atari 7800"), QStringLiteral("Atari 7800 ProSystem"), QStringLiteral("Atari"), 3,
                     {QStringLiteral(".a78")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#e67e22"), 1986}},
    {ID_SNES, {ID_SNES, QStringLiteral("SNES"), QStringLiteral("Super Nintendo Entertainment System"), QStringLiteral("Nintendo"), 4,
               {QStringLiteral(".sfc"), QStringLiteral(".smc")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#9b59b6"), 1990}},
    {ID_GENESIS, {ID_GENESIS, QStringLiteral("Genesis"), QStringLiteral("Sega Genesis / Mega Drive"), QStringLiteral("Sega"), 4,
                  {QStringLiteral(".md"), QStringLiteral(".gen"), QStringLiteral(".smd"), QStringLiteral(".32x"), QStringLiteral(".68k")},
                  QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#34495e"), 1988}},
    {ID_TURBOGRAFX16, {ID_TURBOGRAFX16, QStringLiteral("TurboGrafx-16"), QStringLiteral("TurboGrafx-16 / PC Engine"), QStringLiteral("NEC"), 4,
                       {QStringLiteral(".pce")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN")}, false, QStringLiteral("#e74c3c"), 1987}},
    {ID_GB, {ID_GB, QStringLiteral("Game Boy"), QStringLiteral("Nintendo Game Boy"), QStringLiteral("Nintendo"), 4,
             {QStringLiteral(".gb")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#95a5a6"), 1989}},
    {ID_PSX, {ID_PSX, QStringLiteral("PlayStation"), QStringLiteral("Sony PlayStation"), QStringLiteral("Sony"), 5,
              {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), QStringLiteral(".img"), QStringLiteral(".pbp"), QStringLiteral(".chd"), QStringLiteral(".mdf"), QStringLiteral(".mds"), QStringLiteral(".ecm"), QStringLiteral(".ccd"), QStringLiteral(".sub"), QStringLiteral(".m3u")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, true, QStringLiteral("#003087"), 1994}},
    {ID_N64, {ID_N64, QStringLiteral("N64"), QStringLiteral("Nintendo 64"), QStringLiteral("Nintendo"), 5,
              {QStringLiteral(".n64"), QStringLiteral(".z64"), QStringLiteral(".v64")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#c0392b"), 1996}},
    {ID_SATURN, {ID_SATURN, QStringLiteral("Saturn"), QStringLiteral("Sega Saturn"), QStringLiteral("Sega"), 5,
                 {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), QStringLiteral(".chd")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, true, QStringLiteral("#2c3e50"), 1994}},
    {ID_GBC, {ID_GBC, QStringLiteral("Game Boy Color"), QStringLiteral("Nintendo Game Boy Color"), QStringLiteral("Nintendo"), 5,
              {QStringLiteral(".gbc")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#16a085"), 1998}},
    {ID_NEO_GEO, {ID_NEO_GEO, QStringLiteral("Neo Geo"), QStringLiteral("SNK Neo Geo"), QStringLiteral("SNK"), 4,
                  {QStringLiteral(".neo")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN")}, false, QStringLiteral("#f39c12"), 1990}},
    {ID_TURBOGRAFX_CD, {ID_TURBOGRAFX_CD, QStringLiteral("TurboGrafx-CD"), QStringLiteral("TurboGrafx-CD / PC Engine CD"), QStringLiteral("NEC"), 4,
                        {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".chd")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN")}, true, QStringLiteral("#c0392b"), 1988}},
    {ID_SEGA_CD, {ID_SEGA_CD, QStringLiteral("Sega CD"), QStringLiteral("Sega CD / Mega CD"), QStringLiteral("Sega"), 4,
                  {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), QStringLiteral(".chd")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, true, QStringLiteral("#e74c3c"), 1991}},
    {ID_PS2, {ID_PS2, QStringLiteral("PlayStation 2"), QStringLiteral("Sony PlayStation 2"), QStringLiteral("Sony"), 6,
              {QStringLiteral(".iso"), QStringLiteral(".chd"), QStringLiteral(".cso"), QStringLiteral(".gz"), QStringLiteral(".elf"), QStringLiteral(".isz"), QStringLiteral(".bin"), QStringLiteral(".img"), QStringLiteral(".nrg")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#0051ba"), 2000}},
    {ID_GAMECUBE, {ID_GAMECUBE, QStringLiteral("GameCube"), QStringLiteral("Nintendo GameCube"), QStringLiteral("Nintendo"), 6,
                   {QStringLiteral(".iso"), QStringLiteral(".gcm"), QStringLiteral(".gcz"), QStringLiteral(".rvz"), QStringLiteral(".cso"), QStringLiteral(".dol")},
                   QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#6f42c1"), 2001}},
    {ID_DREAMCAST, {ID_DREAMCAST, QStringLiteral("Dreamcast"), QStringLiteral("Sega Dreamcast"), QStringLiteral("Sega"), 6,
                    {QStringLiteral(".cdi"), QStringLiteral(".gdi"), QStringLiteral(".chd"), QStringLiteral(".bin"), QStringLiteral(".cue"), QStringLiteral(".iso"), QStringLiteral(".dat"), QStringLiteral(".lst")},
                    QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, true, QStringLiteral("#f39c12"), 1998}},
    {ID_GBA, {ID_GBA, QStringLiteral("Game Boy Advance"), QStringLiteral("Nintendo Game Boy Advance"), QStringLiteral("Nintendo"), 6,
              {QStringLiteral(".gba"), QStringLiteral(".srl")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#8e44ad"), 2001}},
    {ID_LYNX, {ID_LYNX, QStringLiteral("Lynx"), QStringLiteral("Atari Lynx"), QStringLiteral("Atari"), 4,
               {QStringLiteral(".lnx"), QStringLiteral(".lyx")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#e67e22"), 1989}},
    {ID_WII, {ID_WII, QStringLiteral("Wii"), QStringLiteral("Nintendo Wii"), QStringLiteral("Nintendo"), 7,
              {QStringLiteral(".iso"), QStringLiteral(".wbfs"), QStringLiteral(".rvz"), QStringLiteral(".gcz"), QStringLiteral(".cso"), QStringLiteral(".wad"), QStringLiteral(".dol")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#00a2e8"), 2006}},
    {ID_PSP, {ID_PSP, QStringLiteral("PSP"), QStringLiteral("PlayStation Portable"), QStringLiteral("Sony"), 7,
              {QStringLiteral(".iso"), QStringLiteral(".cso"), QStringLiteral(".pbp"), QStringLiteral(".chd")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#0051ba"), 2004}},
    {ID_NDS, {ID_NDS, QStringLiteral("Nintendo DS"), QStringLiteral("Nintendo DS"), QStringLiteral("Nintendo"), 7,
              {QStringLiteral(".nds"), QStringLiteral(".dsi"), QStringLiteral(".ids"), QStringLiteral(".srl"), QStringLiteral(".app")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#e74c3c"), 2004}},
    {ID_GAME_GEAR, {ID_GAME_GEAR, QStringLiteral("Game Gear"), QStringLiteral("Sega Game Gear"), QStringLiteral("Sega"), 4,
                    {QStringLiteral(".gg")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#000000"), 1990}},
    {ID_32X, {ID_32X, QStringLiteral("Sega 32X"), QStringLiteral("Sega 32X"), QStringLiteral("Sega"), 5,
              {QStringLiteral(".32x")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#333333"), 1994}},
    {ID_ATARI_JAGUAR, {ID_ATARI_JAGUAR, QStringLiteral("Atari Jaguar"), QStringLiteral("Atari Jaguar"), QStringLiteral("Atari"), 5,
                       {QStringLiteral(".j64"), QStringLiteral(".jag")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#d35400"), 1993}},
    {ID_NGP, {ID_NGP, QStringLiteral("Neo Geo Pocket"), QStringLiteral("Neo Geo Pocket / Color"), QStringLiteral("SNK"), 5,
              {QStringLiteral(".ngp"), QStringLiteral(".ngc")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN")}, false, QStringLiteral("#f39c12"), 1998}},
    {ID_WONDERSWAN, {ID_WONDERSWAN, QStringLiteral("WonderSwan"), QStringLiteral("Bandai WonderSwan / Color"), QStringLiteral("Bandai"), 5,
                     {QStringLiteral(".ws"), QStringLiteral(".wsc")}, QStringLiteral("CRC32"), {QStringLiteral("JPN")}, false, QStringLiteral("#3498db"), 1999}},
    {ID_VIRTUAL_BOY, {ID_VIRTUAL_BOY, QStringLiteral("Virtual Boy"), QStringLiteral("Nintendo Virtual Boy"), QStringLiteral("Nintendo"), 5,
                      {QStringLiteral(".vb")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("JPN")}, false, QStringLiteral("#e74c3c"), 1995}},
    {ID_3DS, {ID_3DS, QStringLiteral("Nintendo 3DS"), QStringLiteral("Nintendo 3DS"), QStringLiteral("Nintendo"), 8,
              {QStringLiteral(".3ds"), QStringLiteral(".cia"), QStringLiteral(".cci"), QStringLiteral(".3dz"), QStringLiteral(".cxi"), QStringLiteral(".app")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#e74c3c"), 2011}},
    {ID_SWITCH, {ID_SWITCH, QStringLiteral("Nintendo Switch"), QStringLiteral("Nintendo Switch"), QStringLiteral("Nintendo"), 9,
                 {QStringLiteral(".nsp"), QStringLiteral(".xci"), QStringLiteral(".nsz"), QStringLiteral(".xcz")}, QStringLiteral("SHA1"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#e60012"), 2017}},
    {ID_PSVITA, {ID_PSVITA, QStringLiteral("PlayStation Vita"), QStringLiteral("Sony PlayStation Vita"), QStringLiteral("Sony"), 8,
                 {QStringLiteral(".vpk")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#003087"), 2011}},
    {ID_C64, {ID_C64, QStringLiteral("Commodore 64"), QStringLiteral("Commodore 64"), QStringLiteral("Commodore"), 2,
              {QStringLiteral(".d64"), QStringLiteral(".t64"), QStringLiteral(".tap"), QStringLiteral(".prg"), QStringLiteral(".crt"), QStringLiteral(".g64"), QStringLiteral(".p00"), QStringLiteral(".d71"), QStringLiteral(".d81")},
              QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#8B4513"), 1982}},
    {ID_AMIGA, {ID_AMIGA, QStringLiteral("Amiga"), QStringLiteral("Commodore Amiga"), QStringLiteral("Commodore"), 3,
                {QStringLiteral(".adf"), QStringLiteral(".adz"), QStringLiteral(".dms"), QStringLiteral(".ipf"), QStringLiteral(".hdf")}, QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#27ae60"), 1985}},
    {ID_ZX_SPECTRUM, {ID_ZX_SPECTRUM, QStringLiteral("ZX Spectrum"), QStringLiteral("Sinclair ZX Spectrum"), QStringLiteral("Sinclair"), 2,
                      {QStringLiteral(".z80"), QStringLiteral(".sna"), QStringLiteral(".szx"), QStringLiteral(".tap"), QStringLiteral(".tzx"), QStringLiteral(".dsk"), QStringLiteral(".trd"), QStringLiteral(".scl")},
                      QStringLiteral("CRC32"), {QStringLiteral("EUR")}, false, QStringLiteral("#000000"), 1982}},
    {ID_SUPERGRAFX, {ID_SUPERGRAFX, QStringLiteral("SuperGrafx"), QStringLiteral("NEC PC Engine SuperGrafx"), QStringLiteral("NEC"), 4,
                     {QStringLiteral(".sgx")}, QStringLiteral("CRC32"), {QStringLiteral("JPN")}, false, QStringLiteral("#e74c3c"), 1989}},
    {ID_XBOX, {ID_XBOX, QStringLiteral("Xbox"), QStringLiteral("Microsoft Xbox"), QStringLiteral("Microsoft"), 6,
               {QStringLiteral(".xiso"), QStringLiteral(".iso")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("EUR"), QStringLiteral("JPN")}, false, QStringLiteral("#107c10"), 2001}},
    {ID_XBOX360, {ID_XBOX360, QStringLiteral("Xbox 360"), QStringLiteral("Microsoft Xbox 360"), QStringLiteral("Microsoft"), 7,
                  {QStringLiteral(".xex"), QStringLiteral(".iso")}, QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("EUR"), QStringLiteral("JPN")}, false, QStringLiteral("#107c10"), 2005}},
    {ID_ARCADE, {ID_ARCADE, QStringLiteral("Arcade"), QStringLiteral("Arcade / MAME"), QStringLiteral("Various"), 0,
                 {QStringLiteral(".zip")}, QStringLiteral("CRC32"), {}, true, QStringLiteral("#f1c40f"), 1970}},
    {ID_3DO, {ID_3DO, QStringLiteral("3DO"), QStringLiteral("3DO Interactive Multiplayer"), QStringLiteral("Panasonic"), 5,
              {QStringLiteral(".iso"), QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".chd")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, true, QStringLiteral("#d4a017"), 1993}},
    {ID_NEO_GEO_CD, {ID_NEO_GEO_CD, QStringLiteral("Neo Geo CD"), QStringLiteral("SNK Neo Geo CD"), QStringLiteral("SNK"), 5,
                     {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".chd"), QStringLiteral(".iso")},
                     QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN")}, true, QStringLiteral("#f39c12"), 1994}},
    {ID_COLECOVISION, {ID_COLECOVISION, QStringLiteral("ColecoVision"), QStringLiteral("Coleco ColecoVision"), QStringLiteral("Coleco"), 2,
                       {QStringLiteral(".col"), QStringLiteral(".bin"), QStringLiteral(".rom")},
                       QStringLiteral("CRC32"), {QStringLiteral("USA"), QStringLiteral("EUR")}, false, QStringLiteral("#2c3e50"), 1982}},
    {ID_INTELLIVISION, {ID_INTELLIVISION, QStringLiteral("Intellivision"), QStringLiteral("Mattel Intellivision"), QStringLiteral("Mattel"), 2,
                        {QStringLiteral(".int"), QStringLiteral(".bin"), QStringLiteral(".rom")},
                        QStringLiteral("CRC32"), {QStringLiteral("USA")}, false, QStringLiteral("#3498db"), 1979}},
    {ID_MSX, {ID_MSX, QStringLiteral("MSX"), QStringLiteral("Microsoft MSX"), QStringLiteral("Microsoft"), 3,
              {QStringLiteral(".rom"), QStringLiteral(".mx1"), QStringLiteral(".dsk"), QStringLiteral(".cas"), QStringLiteral(".sg"), QStringLiteral(".sc"), QStringLiteral(".m2")},
              QStringLiteral("CRC32"), {QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#107c10"), 1983}},
    {ID_MSX2, {ID_MSX2, QStringLiteral("MSX2"), QStringLiteral("Microsoft MSX2"), QStringLiteral("Microsoft"), 3,
               {QStringLiteral(".rom"), QStringLiteral(".mx2"), QStringLiteral(".dsk"), QStringLiteral(".cas")},
               QStringLiteral("CRC32"), {QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#107c10"), 1985}},
    {ID_PC_FX, {ID_PC_FX, QStringLiteral("PC-FX"), QStringLiteral("NEC PC-FX"), QStringLiteral("NEC"), 5,
                {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), QStringLiteral(".chd")},
                QStringLiteral("MD5"), {QStringLiteral("JPN")}, true, QStringLiteral("#e74c3c"), 1994}},
    {ID_CDI, {ID_CDI, QStringLiteral("CD-i"), QStringLiteral("Philips CD-i"), QStringLiteral("Philips"), 5,
              {QStringLiteral(".chd"), QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("EUR")}, true, QStringLiteral("#c0392b"), 1991}},
    {ID_CD32, {ID_CD32, QStringLiteral("Amiga CD32"), QStringLiteral("Commodore Amiga CD32"), QStringLiteral("Commodore"), 5,
               {QStringLiteral(".chd"), QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso")},
               QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("EUR")}, true, QStringLiteral("#27ae60"), 1993}},
    {ID_NAOMI, {ID_NAOMI, QStringLiteral("Sega Naomi"), QStringLiteral("Sega Naomi"), QStringLiteral("Sega"), 6,
                {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".chd"), QStringLiteral(".gdi")},
                QStringLiteral("MD5"), {QStringLiteral("JPN")}, true, QStringLiteral("#e74c3c"), 1998}},
    {ID_SG1000, {ID_SG1000, QStringLiteral("SG-1000"), QStringLiteral("Sega SG-1000"), QStringLiteral("Sega"), 2,
                 {QStringLiteral(".sg"), QStringLiteral(".sc"), QStringLiteral(".bin"), QStringLiteral(".rom")},
                 QStringLiteral("CRC32"), {QStringLiteral("JPN")}, false, QStringLiteral("#000000"), 1983}},
    {ID_WIIU, {ID_WIIU, QStringLiteral("Wii U"), QStringLiteral("Nintendo Wii U"), QStringLiteral("Nintendo"), 8,
               {QStringLiteral(".wud"), QStringLiteral(".wux"), QStringLiteral(".rvz")},
               QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#009ac7"), 2012}},
    {ID_PS3, {ID_PS3, QStringLiteral("PlayStation 3"), QStringLiteral("Sony PlayStation 3"), QStringLiteral("Sony"), 7,
              {QStringLiteral(".iso"), QStringLiteral(".pkg")},
              QStringLiteral("MD5"), {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")}, false, QStringLiteral("#003087"), 2006}},
};

const QMap<QString, QList<int>> EXTENSION_TO_SYSTEMS = {
    {QStringLiteral(".nes"), {ID_NES}}, {QStringLiteral(".nez"), {ID_NES}}, {QStringLiteral(".unf"), {ID_NES}}, {QStringLiteral(".unif"), {ID_NES}},
    {QStringLiteral(".fds"), {ID_FDS}},
    {QStringLiteral(".sfc"), {ID_SNES}}, {QStringLiteral(".smc"), {ID_SNES}},
    {QStringLiteral(".n64"), {ID_N64}}, {QStringLiteral(".z64"), {ID_N64}}, {QStringLiteral(".v64"), {ID_N64}}, {QStringLiteral(".ndd"), {ID_N64}},
    {QStringLiteral(".gb"), {ID_GB}}, {QStringLiteral(".gbc"), {ID_GBC}}, {QStringLiteral(".gba"), {ID_GBA}}, {QStringLiteral(".srl"), {ID_GBA, ID_NDS}},
    {QStringLiteral(".nds"), {ID_NDS}}, {QStringLiteral(".dsi"), {ID_NDS}}, {QStringLiteral(".ids"), {ID_NDS}},
    {QStringLiteral(".gcm"), {ID_GAMECUBE}}, {QStringLiteral(".gcz"), {ID_GAMECUBE, ID_WII}}, {QStringLiteral(".rvz"), {ID_GAMECUBE, ID_WII}}, {QStringLiteral(".wbfs"), {ID_WII}}, {QStringLiteral(".wad"), {ID_WII}}, {QStringLiteral(".dol"), {ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".3ds"), {ID_3DS}}, {QStringLiteral(".3dz"), {ID_3DS}}, {QStringLiteral(".cia"), {ID_3DS}}, {QStringLiteral(".cci"), {ID_3DS}}, {QStringLiteral(".cxi"), {ID_3DS}},
    {QStringLiteral(".nsp"), {ID_SWITCH}}, {QStringLiteral(".xci"), {ID_SWITCH}}, {QStringLiteral(".nsz"), {ID_SWITCH}}, {QStringLiteral(".xcz"), {ID_SWITCH}},
    {QStringLiteral(".vb"), {ID_VIRTUAL_BOY}},
    {QStringLiteral(".sms"), {ID_MASTER_SYSTEM}}, {QStringLiteral(".gg"), {ID_GAME_GEAR}},
    {QStringLiteral(".md"), {ID_GENESIS}}, {QStringLiteral(".gen"), {ID_GENESIS}}, {QStringLiteral(".smd"), {ID_GENESIS}}, {QStringLiteral(".32x"), {ID_32X}}, {QStringLiteral(".68k"), {ID_GENESIS}},
    {QStringLiteral(".cdi"), {ID_DREAMCAST}}, {QStringLiteral(".gdi"), {ID_DREAMCAST}},
    {QStringLiteral(".pbp"), {ID_PSX, ID_PSP}}, {QStringLiteral(".ecm"), {ID_PSX}}, {QStringLiteral(".mdf"), {ID_PSX, ID_PS2}}, {QStringLiteral(".mds"), {ID_PSX, ID_PS2}}, {QStringLiteral(".ccd"), {ID_PSX, ID_PS2}}, {QStringLiteral(".sub"), {ID_PSX}},
    {QStringLiteral(".cso"), {ID_PSP, ID_PS2, ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".vpk"), {ID_PSVITA}},
    {QStringLiteral(".a26"), {ID_ATARI_2600}}, {QStringLiteral(".a78"), {ID_ATARI_7800}}, {QStringLiteral(".lnx"), {ID_LYNX}}, {QStringLiteral(".lyx"), {ID_LYNX}}, {QStringLiteral(".j64"), {ID_ATARI_JAGUAR}}, {QStringLiteral(".jag"), {ID_ATARI_JAGUAR}},
    {QStringLiteral(".pce"), {ID_TURBOGRAFX16}}, {QStringLiteral(".sgx"), {ID_SUPERGRAFX}},
    {QStringLiteral(".neo"), {ID_NEO_GEO}}, {QStringLiteral(".ngp"), {ID_NGP}}, {QStringLiteral(".ngc"), {ID_NGP}},
    {QStringLiteral(".ws"), {ID_WONDERSWAN}}, {QStringLiteral(".wsc"), {ID_WONDERSWAN}},
    {QStringLiteral(".xiso"), {ID_XBOX}}, {QStringLiteral(".xex"), {ID_XBOX360}}, {QStringLiteral(".xbe"), {ID_XBOX}},
    {QStringLiteral(".d64"), {ID_C64}}, {QStringLiteral(".d71"), {ID_C64}}, {QStringLiteral(".d81"), {ID_C64}}, {QStringLiteral(".t64"), {ID_C64}}, {QStringLiteral(".prg"), {ID_C64}}, {QStringLiteral(".p00"), {ID_C64}}, {QStringLiteral(".crt"), {ID_C64}}, {QStringLiteral(".g64"), {ID_C64}},
    {QStringLiteral(".adf"), {ID_AMIGA}}, {QStringLiteral(".adz"), {ID_AMIGA}}, {QStringLiteral(".dms"), {ID_AMIGA}}, {QStringLiteral(".ipf"), {ID_AMIGA}}, {QStringLiteral(".hdf"), {ID_AMIGA}},
    {QStringLiteral(".z80"), {ID_ZX_SPECTRUM}}, {QStringLiteral(".sna"), {ID_ZX_SPECTRUM}}, {QStringLiteral(".szx"), {ID_ZX_SPECTRUM}}, {QStringLiteral(".tzx"), {ID_ZX_SPECTRUM}}, {QStringLiteral(".pzx"), {ID_ZX_SPECTRUM}}, {QStringLiteral(".trd"), {ID_ZX_SPECTRUM}}, {QStringLiteral(".scl"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".iso"), {ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII, ID_PSP, ID_SATURN, ID_SEGA_CD, ID_DREAMCAST, ID_XBOX, ID_XBOX360, ID_3DO, ID_NEO_GEO_CD}},
    {QStringLiteral(".cue"), {ID_PSX, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_DREAMCAST, ID_PS2, ID_3DO, ID_NEO_GEO_CD}},
    {QStringLiteral(".bin"), {ID_PSX, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_ATARI_2600, ID_DREAMCAST, ID_PS2, ID_GENESIS, ID_3DO, ID_NEO_GEO_CD}},
    {QStringLiteral(".chd"), {ID_PSX, ID_PS2, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_DREAMCAST, ID_PSP, ID_3DO, ID_NEO_GEO_CD}},
    {QStringLiteral(".img"), {ID_PSX, ID_PS2, ID_SATURN}}, {QStringLiteral(".m3u"), {ID_PSX, ID_PS2, ID_SATURN, ID_SEGA_CD, ID_DREAMCAST}},
    {QStringLiteral(".tap"), {ID_C64, ID_ZX_SPECTRUM}}, {QStringLiteral(".dsk"), {ID_ZX_SPECTRUM, ID_AMIGA}}, {QStringLiteral(".elf"), {ID_PS2, ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".nrg"), {ID_PSX, ID_PS2}}, {QStringLiteral(".isz"), {ID_PS2}}, {QStringLiteral(".app"), {ID_NDS, ID_3DS}},
    // 3DO and Neo Geo CD disc formats
    {QStringLiteral(".3do"), {ID_3DO}},
};

const QList<int> NINTENDO_SYSTEMS = {ID_NES, ID_FDS, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, ID_NDS, ID_GAMECUBE, ID_WII, ID_WIIU, ID_VIRTUAL_BOY, ID_3DS, ID_SWITCH};
const QList<int> SEGA_SYSTEMS = {ID_SG1000, ID_MASTER_SYSTEM, ID_GENESIS, ID_SEGA_CD, ID_SATURN, ID_DREAMCAST, ID_GAME_GEAR, ID_32X, ID_NAOMI};
const QList<int> SONY_SYSTEMS = {ID_PSX, ID_PS2, ID_PS3, ID_PSP, ID_PSVITA};
const QList<int> MICROSOFT_SYSTEMS = {ID_XBOX, ID_XBOX360};
const QList<int> HANDHELD_SYSTEMS = {ID_GB, ID_GBC, ID_GBA, ID_NDS, ID_PSP, ID_LYNX, ID_GAME_GEAR, ID_NGP, ID_WONDERSWAN, ID_VIRTUAL_BOY, ID_3DS, ID_PSVITA, ID_SWITCH};
const QList<int> DISC_SYSTEMS = {ID_PSX, ID_PS2, ID_PS3, ID_GAMECUBE, ID_WII, ID_DREAMCAST, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_3DS, ID_SWITCH, ID_XBOX, ID_XBOX360, ID_3DO, ID_NEO_GEO_CD, ID_PC_FX, ID_CDI, ID_CD32, ID_NAOMI, ID_ATARI_JAGUAR_CD};
const QList<int> CARTRIDGE_SYSTEMS = {ID_NES, ID_FDS, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, ID_NDS, ID_GENESIS, ID_MASTER_SYSTEM, ID_SG1000, ID_ATARI_2600, ID_ATARI_5200, ID_ATARI_7800, ID_ATARI_8BIT, ID_ATARI_JAGUAR, ID_LYNX, ID_TURBOGRAFX16, ID_NEO_GEO, ID_GAME_GEAR, ID_32X, ID_NGP, ID_WONDERSWAN, ID_VIRTUAL_BOY, ID_SUPERGRAFX, ID_COLECOVISION, ID_INTELLIVISION, ID_MSX, ID_MSX2};
const QList<int> COMPUTER_SYSTEMS = {ID_C64, ID_AMIGA, ID_ZX_SPECTRUM, ID_ATARI_ST, ID_ATARI_8BIT, ID_MSX, ID_MSX2};

const SystemDef *getSystem(int systemId)
{
    const auto it = SYSTEMS.find(systemId);
    return (it != SYSTEMS.end()) ? &it.value() : nullptr;
}

int getSystemIdByName(const QString &name)
{
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        if (it.value().internalName == name) {
            return it.key();
        }
    }
    return 0;
}

const SystemDef *getSystemByName(const QString &name)
{
    const int id = getSystemIdByName(name);
    return (id > 0) ? getSystem(id) : nullptr;
}

QStringList getSystemDisplayNames()
{
    QStringList names;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        names << it.value().displayName;
    }
    return names;
}

QStringList getSystemInternalNames()
{
    QStringList names;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        names << it.value().internalName;
    }
    return names;
}

QList<int> getSystemsForExtension(const QString &extension)
{
    const auto it = EXTENSION_TO_SYSTEMS.find(extension.toLower());
    return (it != EXTENSION_TO_SYSTEMS.end()) ? it.value() : QList<int>();
}

bool isAmbiguousExtension(const QString &extension)
{
    return getSystemsForExtension(extension).size() > 1;
}

} // namespace Systems
} // namespace Constants
} // namespace Remus