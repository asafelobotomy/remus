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
inline const QMap<int, SystemDef> SYSTEMS = {
    // Generation 3 (8-bit era)
    {ID_NES, {
        ID_NES,
        QStringLiteral("NES"),
        QStringLiteral("Nintendo Entertainment System"),
        QStringLiteral("Nintendo"),
        3,
        {QStringLiteral(".nes"), QStringLiteral(".nez"), QStringLiteral(".unf"), 
         QStringLiteral(".unif"), QStringLiteral(".fds")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,  // Single-file
        QStringLiteral("#e74c3c"),  // Red
        1983
    }},
    
    {ID_MASTER_SYSTEM, {
        ID_MASTER_SYSTEM,
        QStringLiteral("Master System"),
        QStringLiteral("Sega Master System"),
        QStringLiteral("Sega"),
        3,
        {QStringLiteral(".sms")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR"), QStringLiteral("BRA")},
        false,
        QStringLiteral("#000000"),  // Black
        1985
    }},
    
    {ID_ATARI_2600, {
        ID_ATARI_2600,
        QStringLiteral("Atari 2600"),
        QStringLiteral("Atari 2600"),
        QStringLiteral("Atari"),
        2,
        {QStringLiteral(".a26"), QStringLiteral(".bin")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA")},
        false,
        QStringLiteral("#d35400"),  // Orange/Brown
        1977
    }},
    
    {ID_ATARI_7800, {
        ID_ATARI_7800,
        QStringLiteral("Atari 7800"),
        QStringLiteral("Atari 7800 ProSystem"),
        QStringLiteral("Atari"),
        3,
        {QStringLiteral(".a78")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#e67e22"),  // Orange
        1986
    }},
    
    // Generation 4 (16-bit era)
    {ID_SNES, {
        ID_SNES,
        QStringLiteral("SNES"),
        QStringLiteral("Super Nintendo Entertainment System"),
        QStringLiteral("Nintendo"),
        4,
        {QStringLiteral(".sfc"), QStringLiteral(".smc")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#9b59b6"),  // Purple
        1990
    }},
    
    {ID_GENESIS, {
        ID_GENESIS,
        QStringLiteral("Genesis"),
        QStringLiteral("Sega Genesis / Mega Drive"),
        QStringLiteral("Sega"),
        4,
        {QStringLiteral(".md"), QStringLiteral(".gen"), QStringLiteral(".smd"), 
         QStringLiteral(".32x"), QStringLiteral(".68k")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#34495e"),  // Dark Gray
        1988
    }},
    
    {ID_TURBOGRAFX16, {
        ID_TURBOGRAFX16,
        QStringLiteral("TurboGrafx-16"),
        QStringLiteral("TurboGrafx-16 / PC Engine"),
        QStringLiteral("NEC"),
        4,
        {QStringLiteral(".pce")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN")},
        false,
        QStringLiteral("#e74c3c"),  // Red
        1987
    }},
    
    {ID_GB, {
        ID_GB,
        QStringLiteral("Game Boy"),
        QStringLiteral("Nintendo Game Boy"),
        QStringLiteral("Nintendo"),
        4,
        {QStringLiteral(".gb")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#95a5a6"),  // Gray
        1989
    }},
    
    // Generation 5 (32/64-bit era)
    {ID_PSX, {
        ID_PSX,
        QStringLiteral("PlayStation"),
        QStringLiteral("Sony PlayStation"),
        QStringLiteral("Sony"),
        5,
        {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), 
         QStringLiteral(".img"), QStringLiteral(".pbp"), QStringLiteral(".chd"),
         QStringLiteral(".mdf"), QStringLiteral(".mds"), QStringLiteral(".ecm"),
         QStringLiteral(".ccd"), QStringLiteral(".sub"), QStringLiteral(".m3u")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        true,  // Multi-file (.cue + .bin sets)
        QStringLiteral("#003087"),  // PlayStation Blue
        1994
    }},
    
    {ID_N64, {
        ID_N64,
        QStringLiteral("N64"),
        QStringLiteral("Nintendo 64"),
        QStringLiteral("Nintendo"),
        5,
        {QStringLiteral(".n64"), QStringLiteral(".z64"), QStringLiteral(".v64")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#c0392b"),  // Dark Red
        1996
    }},
    
    {ID_SATURN, {
        ID_SATURN,
        QStringLiteral("Saturn"),
        QStringLiteral("Sega Saturn"),
        QStringLiteral("Sega"),
        5,
        {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), QStringLiteral(".chd")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        true,  // Multi-file
        QStringLiteral("#2c3e50"),  // Dark Blue/Gray
        1994
    }},
    
    {ID_GBC, {
        ID_GBC,
        QStringLiteral("Game Boy Color"),
        QStringLiteral("Nintendo Game Boy Color"),
        QStringLiteral("Nintendo"),
        5,
        {QStringLiteral(".gbc")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#16a085"),  // Teal
        1998
    }},
    
    {ID_NEO_GEO, {
        ID_NEO_GEO,
        QStringLiteral("Neo Geo"),
        QStringLiteral("SNK Neo Geo"),
        QStringLiteral("SNK"),
        4,
        {QStringLiteral(".neo")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN")},
        false,
        QStringLiteral("#f39c12"),  // Gold
        1990
    }},
    
    {ID_TURBOGRAFX_CD, {
        ID_TURBOGRAFX_CD,
        QStringLiteral("TurboGrafx-CD"),
        QStringLiteral("TurboGrafx-CD / PC Engine CD"),
        QStringLiteral("NEC"),
        4,
        {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".chd")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN")},
        true,  // Multi-file
        QStringLiteral("#c0392b"),  // Red
        1988
    }},
    
    {ID_SEGA_CD, {
        ID_SEGA_CD,
        QStringLiteral("Sega CD"),
        QStringLiteral("Sega CD / Mega CD"),
        QStringLiteral("Sega"),
        4,
        {QStringLiteral(".cue"), QStringLiteral(".bin"), QStringLiteral(".iso"), QStringLiteral(".chd")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        true,  // Multi-file
        QStringLiteral("#e74c3c"),  // Red
        1991
    }},
    
    // Generation 6 (128-bit era)
    {ID_PS2, {
        ID_PS2,
        QStringLiteral("PlayStation 2"),
        QStringLiteral("Sony PlayStation 2"),
        QStringLiteral("Sony"),
        6,
        {QStringLiteral(".iso"), QStringLiteral(".chd"), QStringLiteral(".cso"),
         QStringLiteral(".gz"), QStringLiteral(".elf"), QStringLiteral(".isz"),
         QStringLiteral(".bin"), QStringLiteral(".img"), QStringLiteral(".nrg")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#0051ba"),  // Blue
        2000
    }},
    
    {ID_GAMECUBE, {
        ID_GAMECUBE,
        QStringLiteral("GameCube"),
        QStringLiteral("Nintendo GameCube"),
        QStringLiteral("Nintendo"),
        6,
        {QStringLiteral(".iso"), QStringLiteral(".gcm"), QStringLiteral(".gcz"), 
         QStringLiteral(".rvz"), QStringLiteral(".cso"), QStringLiteral(".dol")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#6f42c1"),  // Purple
        2001
    }},
    
    {ID_DREAMCAST, {
        ID_DREAMCAST,
        QStringLiteral("Dreamcast"),
        QStringLiteral("Sega Dreamcast"),
        QStringLiteral("Sega"),
        6,
        {QStringLiteral(".cdi"), QStringLiteral(".gdi"), QStringLiteral(".chd"),
         QStringLiteral(".bin"), QStringLiteral(".cue"), QStringLiteral(".iso"),
         QStringLiteral(".dat"), QStringLiteral(".lst")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        true,  // Multi-file (.gdi + tracks)
        QStringLiteral("#f39c12"),  // Orange
        1998
    }},
    
    {ID_GBA, {
        ID_GBA,
        QStringLiteral("Game Boy Advance"),
        QStringLiteral("Nintendo Game Boy Advance"),
        QStringLiteral("Nintendo"),
        6,
        {QStringLiteral(".gba"), QStringLiteral(".srl")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#8e44ad"),  // Purple
        2001
    }},
    
    {ID_LYNX, {
        ID_LYNX,
        QStringLiteral("Lynx"),
        QStringLiteral("Atari Lynx"),
        QStringLiteral("Atari"),
        4,
        {QStringLiteral(".lnx"), QStringLiteral(".lyx")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#e67e22"),  // Orange
        1989
    }},
    
    // Generation 7 (HD era)
    {ID_WII, {
        ID_WII,
        QStringLiteral("Wii"),
        QStringLiteral("Nintendo Wii"),
        QStringLiteral("Nintendo"),
        7,
        {QStringLiteral(".iso"), QStringLiteral(".wbfs"), QStringLiteral(".rvz"), 
         QStringLiteral(".gcz"), QStringLiteral(".cso"), QStringLiteral(".wad"), 
         QStringLiteral(".dol")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#00a2e8"),  // Light Blue
        2006
    }},
    
    {ID_PSP, {
        ID_PSP,
        QStringLiteral("PSP"),
        QStringLiteral("PlayStation Portable"),
        QStringLiteral("Sony"),
        7,
        {QStringLiteral(".iso"), QStringLiteral(".cso"), QStringLiteral(".pbp"),
         QStringLiteral(".chd")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#0051ba"),  // PlayStation Blue
        2004
    }},
    
    {ID_NDS, {
        ID_NDS,
        QStringLiteral("Nintendo DS"),
        QStringLiteral("Nintendo DS"),
        QStringLiteral("Nintendo"),
        7,
        {QStringLiteral(".nds"), QStringLiteral(".dsi"), QStringLiteral(".ids"), 
         QStringLiteral(".srl"), QStringLiteral(".app")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#e74c3c"),  // Red
        2004
    }},
    
    // Additional Systems
    {ID_GAME_GEAR, {
        ID_GAME_GEAR,
        QStringLiteral("Game Gear"),
        QStringLiteral("Sega Game Gear"),
        QStringLiteral("Sega"),
        4,
        {QStringLiteral(".gg")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#000000"),  // Black
        1990
    }},
    
    {ID_32X, {
        ID_32X,
        QStringLiteral("Sega 32X"),
        QStringLiteral("Sega 32X"),
        QStringLiteral("Sega"),
        5,
        {QStringLiteral(".32x")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#333333"),  // Dark Gray
        1994
    }},
    
    {ID_ATARI_JAGUAR, {
        ID_ATARI_JAGUAR,
        QStringLiteral("Atari Jaguar"),
        QStringLiteral("Atari Jaguar"),
        QStringLiteral("Atari"),
        5,
        {QStringLiteral(".j64"), QStringLiteral(".jag")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#d35400"),  // Orange/Brown
        1993
    }},
    
    {ID_NGP, {
        ID_NGP,
        QStringLiteral("Neo Geo Pocket"),
        QStringLiteral("Neo Geo Pocket / Color"),
        QStringLiteral("SNK"),
        5,
        {QStringLiteral(".ngp"), QStringLiteral(".ngc")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN")},
        false,
        QStringLiteral("#f39c12"),  // Gold
        1998
    }},
    
    {ID_WONDERSWAN, {
        ID_WONDERSWAN,
        QStringLiteral("WonderSwan"),
        QStringLiteral("Bandai WonderSwan / Color"),
        QStringLiteral("Bandai"),
        5,
        {QStringLiteral(".ws"), QStringLiteral(".wsc")},
        QStringLiteral("CRC32"),
        {QStringLiteral("JPN")},
        false,
        QStringLiteral("#3498db"),  // Blue
        1999
    }},
    
    {ID_VIRTUAL_BOY, {
        ID_VIRTUAL_BOY,
        QStringLiteral("Virtual Boy"),
        QStringLiteral("Nintendo Virtual Boy"),
        QStringLiteral("Nintendo"),
        5,
        {QStringLiteral(".vb")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("JPN")},
        false,
        QStringLiteral("#e74c3c"),  // Red
        1995
    }},
    
    {ID_3DS, {
        ID_3DS,
        QStringLiteral("Nintendo 3DS"),
        QStringLiteral("Nintendo 3DS"),
        QStringLiteral("Nintendo"),
        8,
        {QStringLiteral(".3ds"), QStringLiteral(".cia"), QStringLiteral(".cci"),
         QStringLiteral(".3dz"), QStringLiteral(".cxi"), QStringLiteral(".app")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#e74c3c"),  // Red
        2011
    }},
    
    {ID_SWITCH, {
        ID_SWITCH,
        QStringLiteral("Nintendo Switch"),
        QStringLiteral("Nintendo Switch"),
        QStringLiteral("Nintendo"),
        9,
        {QStringLiteral(".nsp"), QStringLiteral(".xci"), QStringLiteral(".nsz"),
         QStringLiteral(".xcz")}  ,
        QStringLiteral("SHA1"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#e60012"),  // Nintendo Red
        2017
    }},
    
    {ID_PSVITA, {
        ID_PSVITA,
        QStringLiteral("PlayStation Vita"),
        QStringLiteral("Sony PlayStation Vita"),
        QStringLiteral("Sony"),
        8,
        {QStringLiteral(".vpk")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("JPN"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#003087"),  // PlayStation Blue
        2011
    }},
    
    {ID_C64, {
        ID_C64,
        QStringLiteral("Commodore 64"),
        QStringLiteral("Commodore 64"),
        QStringLiteral("Commodore"),
        2,
        {QStringLiteral(".d64"), QStringLiteral(".t64"), QStringLiteral(".tap"),
         QStringLiteral(".prg"), QStringLiteral(".crt"), QStringLiteral(".g64"),
         QStringLiteral(".p00"), QStringLiteral(".d71"), QStringLiteral(".d81")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#8B4513"),  // Brown
        1982
    }},
    
    {ID_AMIGA, {
        ID_AMIGA,
        QStringLiteral("Amiga"),
        QStringLiteral("Commodore Amiga"),
        QStringLiteral("Commodore"),
        3,
        {QStringLiteral(".adf"), QStringLiteral(".adz"), QStringLiteral(".dms"),
         QStringLiteral(".ipf"), QStringLiteral(".hdf")},
        QStringLiteral("CRC32"),
        {QStringLiteral("USA"), QStringLiteral("EUR")},
        false,
        QStringLiteral("#27ae60"),  // Green
        1985
    }},
    
    {ID_ZX_SPECTRUM, {
        ID_ZX_SPECTRUM,
        QStringLiteral("ZX Spectrum"),
        QStringLiteral("Sinclair ZX Spectrum"),
        QStringLiteral("Sinclair"),
        2,
        {QStringLiteral(".z80"), QStringLiteral(".sna"), QStringLiteral(".szx"),
         QStringLiteral(".tap"), QStringLiteral(".tzx"), QStringLiteral(".dsk"),
         QStringLiteral(".trd"), QStringLiteral(".scl")},
        QStringLiteral("CRC32"),
        {QStringLiteral("EUR")},
        false,
        QStringLiteral("#000000"),  // Black
        1982
    }},
    
    {ID_SUPERGRAFX, {
        ID_SUPERGRAFX,
        QStringLiteral("SuperGrafx"),
        QStringLiteral("NEC PC Engine SuperGrafx"),
        QStringLiteral("NEC"),
        4,
        {QStringLiteral(".sgx")},
        QStringLiteral("CRC32"),
        {QStringLiteral("JPN")},
        false,
        QStringLiteral("#e74c3c"),  // Red
        1989
    }},
    
    {ID_XBOX, {
        ID_XBOX,
        QStringLiteral("Xbox"),
        QStringLiteral("Microsoft Xbox"),
        QStringLiteral("Microsoft"),
        6,
        {QStringLiteral(".xiso"), QStringLiteral(".iso")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("EUR"), QStringLiteral("JPN")},
        false,
        QStringLiteral("#107c10"),  // Xbox Green
        2001
    }},
    
    {ID_XBOX360, {
        ID_XBOX360,
        QStringLiteral("Xbox 360"),
        QStringLiteral("Microsoft Xbox 360"),
        QStringLiteral("Microsoft"),
        7,
        {QStringLiteral(".xex"), QStringLiteral(".iso")},
        QStringLiteral("MD5"),
        {QStringLiteral("USA"), QStringLiteral("EUR"), QStringLiteral("JPN")},
        false,
        QStringLiteral("#107c10"),  // Xbox Green
        2005
    }},
    
    {ID_ARCADE, {
        ID_ARCADE,
        QStringLiteral("Arcade"),
        QStringLiteral("Arcade / MAME"),
        QStringLiteral("Various"),
        0,  // Arcade spans all generations
        {QStringLiteral(".zip")},  // MAME uses zipped ROM sets
        QStringLiteral("CRC32"),
        {},
        true,  // Multi-file ROM sets
        QStringLiteral("#f1c40f"),  // Yellow/Gold
        1970
    }},
};

// ============================================================================
// Extension to System Mapping
// ============================================================================

/**
 * @brief Reverse lookup: file extension → possible systems
 * 
 * Used during file scanning to suggest possible systems.
 * Some extensions are ambiguous (.iso can be PS1, PS2, GameCube, etc.)
 */
inline const QMap<QString, QList<int>> EXTENSION_TO_SYSTEMS = {
    // Nintendo systems - NES/Famicom
    {QStringLiteral(".nes"), {ID_NES}},
    {QStringLiteral(".nez"), {ID_NES}},
    {QStringLiteral(".unf"), {ID_NES}},
    {QStringLiteral(".unif"), {ID_NES}},
    {QStringLiteral(".fds"), {ID_NES}},
    
    // Nintendo - SNES
    {QStringLiteral(".sfc"), {ID_SNES}},
    {QStringLiteral(".smc"), {ID_SNES}},
    
    // Nintendo - N64
    {QStringLiteral(".n64"), {ID_N64}},
    {QStringLiteral(".z64"), {ID_N64}},
    {QStringLiteral(".v64"), {ID_N64}},
    {QStringLiteral(".ndd"), {ID_N64}},  // N64DD
    
    // Nintendo - Game Boy family
    {QStringLiteral(".gb"), {ID_GB}},
    {QStringLiteral(".gbc"), {ID_GBC}},
    {QStringLiteral(".gba"), {ID_GBA}},
    {QStringLiteral(".srl"), {ID_GBA, ID_NDS}},
    
    // Nintendo - DS
    {QStringLiteral(".nds"), {ID_NDS}},
    {QStringLiteral(".dsi"), {ID_NDS}},
    {QStringLiteral(".ids"), {ID_NDS}},
    
    // Nintendo - GameCube/Wii
    {QStringLiteral(".gcm"), {ID_GAMECUBE}},
    {QStringLiteral(".gcz"), {ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".rvz"), {ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".wbfs"), {ID_WII}},
    {QStringLiteral(".wad"), {ID_WII}},
    {QStringLiteral(".dol"), {ID_GAMECUBE, ID_WII}},
    
    // Nintendo - 3DS
    {QStringLiteral(".3ds"), {ID_3DS}},
    {QStringLiteral(".3dz"), {ID_3DS}},
    {QStringLiteral(".cia"), {ID_3DS}},
    {QStringLiteral(".cci"), {ID_3DS}},
    {QStringLiteral(".cxi"), {ID_3DS}},
    
    // Nintendo - Switch
    {QStringLiteral(".nsp"), {ID_SWITCH}},
    {QStringLiteral(".xci"), {ID_SWITCH}},
    {QStringLiteral(".nsz"), {ID_SWITCH}},
    {QStringLiteral(".xcz"), {ID_SWITCH}},
    
    // Nintendo - Virtual Boy
    {QStringLiteral(".vb"), {ID_VIRTUAL_BOY}},
    
    // Sega - Master System / Game Gear
    {QStringLiteral(".sms"), {ID_MASTER_SYSTEM}},
    {QStringLiteral(".gg"), {ID_GAME_GEAR}},
    
    // Sega - Genesis / Mega Drive
    {QStringLiteral(".md"), {ID_GENESIS}},
    {QStringLiteral(".gen"), {ID_GENESIS}},
    {QStringLiteral(".smd"), {ID_GENESIS}},
    {QStringLiteral(".32x"), {ID_32X}},
    {QStringLiteral(".68k"), {ID_GENESIS}},
    
    // Sega - Dreamcast
    {QStringLiteral(".cdi"), {ID_DREAMCAST}},
    {QStringLiteral(".gdi"), {ID_DREAMCAST}},
    
    // Sony - PlayStation
    {QStringLiteral(".pbp"), {ID_PSX, ID_PSP}},
    {QStringLiteral(".ecm"), {ID_PSX}},
    {QStringLiteral(".mdf"), {ID_PSX, ID_PS2}},
    {QStringLiteral(".mds"), {ID_PSX, ID_PS2}},
    {QStringLiteral(".ccd"), {ID_PSX, ID_PS2}},
    {QStringLiteral(".sub"), {ID_PSX}},
    
    // Sony - PSP
    {QStringLiteral(".cso"), {ID_PSP, ID_PS2, ID_GAMECUBE, ID_WII}},
    
    // Sony - PS Vita
    {QStringLiteral(".vpk"), {ID_PSVITA}},
    
    // Atari systems
    {QStringLiteral(".a26"), {ID_ATARI_2600}},
    {QStringLiteral(".a78"), {ID_ATARI_7800}},
    {QStringLiteral(".lnx"), {ID_LYNX}},
    {QStringLiteral(".lyx"), {ID_LYNX}},
    {QStringLiteral(".j64"), {ID_ATARI_JAGUAR}},
    {QStringLiteral(".jag"), {ID_ATARI_JAGUAR}},
    
    // NEC systems
    {QStringLiteral(".pce"), {ID_TURBOGRAFX16}},
    {QStringLiteral(".sgx"), {ID_SUPERGRAFX}},
    
    // SNK systems
    {QStringLiteral(".neo"), {ID_NEO_GEO}},
    {QStringLiteral(".ngp"), {ID_NGP}},
    {QStringLiteral(".ngc"), {ID_NGP}},
    
    // Bandai
    {QStringLiteral(".ws"), {ID_WONDERSWAN}},
    {QStringLiteral(".wsc"), {ID_WONDERSWAN}},
    
    // Microsoft - Xbox
    {QStringLiteral(".xiso"), {ID_XBOX}},
    {QStringLiteral(".xex"), {ID_XBOX360}},
    {QStringLiteral(".xbe"), {ID_XBOX}},
    
    // Commodore 64
    {QStringLiteral(".d64"), {ID_C64}},
    {QStringLiteral(".d71"), {ID_C64}},
    {QStringLiteral(".d81"), {ID_C64}},
    {QStringLiteral(".t64"), {ID_C64}},
    {QStringLiteral(".prg"), {ID_C64}},
    {QStringLiteral(".p00"), {ID_C64}},
    {QStringLiteral(".crt"), {ID_C64}},
    {QStringLiteral(".g64"), {ID_C64}},
    
    // Amiga
    {QStringLiteral(".adf"), {ID_AMIGA}},
    {QStringLiteral(".adz"), {ID_AMIGA}},
    {QStringLiteral(".dms"), {ID_AMIGA}},
    {QStringLiteral(".ipf"), {ID_AMIGA}},
    {QStringLiteral(".hdf"), {ID_AMIGA}},
    
    // ZX Spectrum
    {QStringLiteral(".z80"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".sna"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".szx"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".tzx"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".pzx"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".trd"), {ID_ZX_SPECTRUM}},
    {QStringLiteral(".scl"), {ID_ZX_SPECTRUM}},
    
    // Ambiguous multi-system extensions (need path heuristics or user selection)
    {QStringLiteral(".iso"), {ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII, ID_PSP, ID_SATURN, ID_SEGA_CD, ID_DREAMCAST, ID_XBOX, ID_XBOX360}},
    {QStringLiteral(".cue"), {ID_PSX, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_DREAMCAST, ID_PS2}},
    {QStringLiteral(".bin"), {ID_PSX, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_ATARI_2600, ID_DREAMCAST, ID_PS2, ID_GENESIS}},
    {QStringLiteral(".chd"), {ID_PSX, ID_PS2, ID_SATURN, ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_DREAMCAST, ID_PSP}},
    {QStringLiteral(".img"), {ID_PSX, ID_PS2, ID_SATURN}},
    {QStringLiteral(".m3u"), {ID_PSX, ID_PS2, ID_SATURN, ID_SEGA_CD, ID_DREAMCAST}},
    {QStringLiteral(".tap"), {ID_C64, ID_ZX_SPECTRUM}},
    {QStringLiteral(".dsk"), {ID_ZX_SPECTRUM, ID_AMIGA}},
    {QStringLiteral(".elf"), {ID_PS2, ID_GAMECUBE, ID_WII}},
    {QStringLiteral(".nrg"), {ID_PSX, ID_PS2}},
    {QStringLiteral(".isz"), {ID_PS2}},
    {QStringLiteral(".app"), {ID_NDS, ID_3DS}},
};

// ============================================================================
// System Grouping
// ============================================================================

/**
 * @brief Nintendo systems (for grouping/organization)
 */
inline const QList<int> NINTENDO_SYSTEMS = {
    ID_NES, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, 
    ID_NDS, ID_GAMECUBE, ID_WII, ID_VIRTUAL_BOY, ID_3DS, ID_SWITCH
};

/**
 * @brief Sega systems
 */
inline const QList<int> SEGA_SYSTEMS = {
    ID_MASTER_SYSTEM, ID_GENESIS, ID_SEGA_CD, ID_SATURN, ID_DREAMCAST,
    ID_GAME_GEAR, ID_32X
};

/**
 * @brief Sony/PlayStation systems
 */
inline const QList<int> SONY_SYSTEMS = {
    ID_PSX, ID_PS2, ID_PSP, ID_PSVITA
};

/**
 * @brief Microsoft Xbox systems
 */
inline const QList<int> MICROSOFT_SYSTEMS = {
    ID_XBOX, ID_XBOX360
};

/**
 * @brief Handheld systems
 */
inline const QList<int> HANDHELD_SYSTEMS = {
    ID_GB, ID_GBC, ID_GBA, ID_NDS, ID_PSP, ID_LYNX, ID_GAME_GEAR, 
    ID_NGP, ID_WONDERSWAN, ID_VIRTUAL_BOY, ID_3DS, ID_PSVITA, ID_SWITCH
};

/**
 * @brief Disc-based systems (require special handling)
 */
inline const QList<int> DISC_SYSTEMS = {
    ID_PSX, ID_PS2, ID_GAMECUBE, ID_WII, ID_DREAMCAST, ID_SATURN, 
    ID_SEGA_CD, ID_TURBOGRAFX_CD, ID_3DS, ID_SWITCH, ID_XBOX, ID_XBOX360
};

/**
 * @brief Cartridge-based systems (single file, fast hashing)
 */
inline const QList<int> CARTRIDGE_SYSTEMS = {
    ID_NES, ID_SNES, ID_N64, ID_GB, ID_GBC, ID_GBA, ID_NDS,
    ID_GENESIS, ID_MASTER_SYSTEM, ID_ATARI_2600, ID_ATARI_7800,
    ID_LYNX, ID_TURBOGRAFX16, ID_NEO_GEO, ID_GAME_GEAR, ID_32X,
    ID_ATARI_JAGUAR, ID_NGP, ID_WONDERSWAN, ID_VIRTUAL_BOY, ID_SUPERGRAFX
};

/**
 * @brief Home computer systems
 */
inline const QList<int> COMPUTER_SYSTEMS = {
    ID_C64, ID_AMIGA, ID_ZX_SPECTRUM
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get system definition by ID
 * @param systemId System ID constant (ID_NES, etc.)
 * @return Pointer to SystemDef, or nullptr if not found
 */
inline const SystemDef* getSystem(int systemId) {
    auto it = SYSTEMS.find(systemId);
    return (it != SYSTEMS.end()) ? &it.value() : nullptr;
}

/**
 * @brief Get system ID by internal name
 * @param name Internal system name ("NES", "PlayStation", etc.)
 * @return System ID, or 0 if not found
 */
inline int getSystemIdByName(const QString &name) {
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        if (it.value().internalName == name) {
            return it.key();
        }
    }
    return 0;
}

/**
 * @brief Get system definition by internal name
 * @param name Internal system name ("NES", "PlayStation", etc.)
 * @return Pointer to SystemDef, or nullptr if not found
 */
inline const SystemDef* getSystemByName(const QString &name) {
    int id = getSystemIdByName(name);
    return (id > 0) ? getSystem(id) : nullptr;
}

/**
 * @brief Get all system display names
 * @return List of human-readable system names for UI population
 */
inline QStringList getSystemDisplayNames() {
    QStringList names;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        names << it.value().displayName;
    }
    return names;
}

/**
 * @brief Get all system internal names
 * @return List of internal system names ("NES", "SNES", etc.)
 */
inline QStringList getSystemInternalNames() {
    QStringList names;
    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        names << it.value().internalName;
    }
    return names;
}

/**
 * @brief Get possible systems for a file extension
 * @param extension File extension (e.g., ".iso", ".nes")
 * @return List of system IDs that use this extension
 */
inline QList<int> getSystemsForExtension(const QString &extension) {
    auto it = EXTENSION_TO_SYSTEMS.find(extension.toLower());
    return (it != EXTENSION_TO_SYSTEMS.end()) ? it.value() : QList<int>();
}

/**
 * @brief Check if extension is ambiguous (used by multiple systems)
 * @param extension File extension to check
 * @return True if multiple systems share this extension
 */
inline bool isAmbiguousExtension(const QString &extension) {
    auto systems = getSystemsForExtension(extension);
    return systems.size() > 1;
}

} // Systems
} // Constants
} // Remus
