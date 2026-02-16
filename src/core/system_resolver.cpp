#include "system_resolver.h"
#include "constants/providers.h"

namespace Remus {

using namespace Constants;
using namespace Constants::Providers;

QString SystemResolver::displayName(int systemId)
{
    const Constants::Systems::SystemDef* system = Constants::Systems::getSystem(systemId);
    return system ? system->displayName : QStringLiteral("Unknown");
}

QString SystemResolver::internalName(int systemId)
{
    const Constants::Systems::SystemDef* system = Constants::Systems::getSystem(systemId);
    return system ? system->internalName : QStringLiteral("Unknown");
}

QString SystemResolver::providerName(int systemId, const QString &providerId)
{
    static QMap<int, QMap<QString, QString>> mappings = providerMappings();
    
    // Check if we have a mapping for this system
    if (!mappings.contains(systemId)) {
        return QString();
    }
    
    // Check if we have a mapping for this provider
    const QMap<QString, QString> &providerMap = mappings[systemId];
    if (!providerMap.contains(providerId)) {
        // Fallback: for providers like Hasheous, use internal name
        return internalName(systemId);
    }
    
    return providerMap[providerId];
}

int SystemResolver::systemIdByName(const QString &internalName)
{
    return Constants::Systems::getSystemIdByName(internalName);
}

bool SystemResolver::isValidSystem(int systemId)
{
    return Constants::Systems::getSystem(systemId) != nullptr;
}

QMap<int, QMap<QString, QString>> SystemResolver::providerMappings()
{
    // Provider-specific platform IDs (lazy-initialized static)
    // This is the SINGLE SOURCE OF TRUTH for provider mappings
    
    using namespace Constants::Systems;
    
    return {
        // ====================================================================
        // Nintendo Systems
        // ====================================================================
        {ID_NES, {
            {THEGAMESDB, QStringLiteral("7")},
            {SCREENSCRAPER, QStringLiteral("3")},
            {IGDB, QStringLiteral("nes")},
        }},
        
        {ID_SNES, {
            {THEGAMESDB, QStringLiteral("6")},
            {SCREENSCRAPER, QStringLiteral("4")},
            {IGDB, QStringLiteral("snes")},
        }},
        
        {ID_N64, {
            {THEGAMESDB, QStringLiteral("3")},
            {SCREENSCRAPER, QStringLiteral("14")},
            {IGDB, QStringLiteral("n64")},
        }},
        
        {ID_GAMECUBE, {
            {THEGAMESDB, QStringLiteral("2")},
            {SCREENSCRAPER, QStringLiteral("13")},
            {IGDB, QStringLiteral("gamecube")},
        }},
        
        {ID_WII, {
            {THEGAMESDB, QStringLiteral("9")},
            {SCREENSCRAPER, QStringLiteral("16")},
            {IGDB, QStringLiteral("wii")},
        }},
        
        {ID_GB, {
            {THEGAMESDB, QStringLiteral("4")},
            {SCREENSCRAPER, QStringLiteral("9")},
            {IGDB, QStringLiteral("gameboy")},
        }},
        
        {ID_GBC, {
            {THEGAMESDB, QStringLiteral("41")},
            {SCREENSCRAPER, QStringLiteral("10")},
            {IGDB, QStringLiteral("gbc")},
        }},
        
        {ID_GBA, {
            {THEGAMESDB, QStringLiteral("5")},
            {SCREENSCRAPER, QStringLiteral("12")},
            {IGDB, QStringLiteral("gba")},
        }},
        
        {ID_NDS, {
            {THEGAMESDB, QStringLiteral("8")},
            {SCREENSCRAPER, QStringLiteral("15")},
            {IGDB, QStringLiteral("nds")},
        }},
        
        {ID_3DS, {
            {THEGAMESDB, QStringLiteral("4912")},
            {SCREENSCRAPER, QStringLiteral("17")},
            {IGDB, QStringLiteral("3ds")},
        }},
        
        {ID_SWITCH, {
            {THEGAMESDB, QStringLiteral("4971")},
            {SCREENSCRAPER, QStringLiteral("225")},
            {IGDB, QStringLiteral("switch")},
        }},
        
        {ID_VIRTUAL_BOY, {
            {THEGAMESDB, QStringLiteral("28")},
            {SCREENSCRAPER, QStringLiteral("11")},
            {IGDB, QStringLiteral("virtualboy")},
        }},
        
        // ====================================================================
        // Sega Systems
        // ====================================================================
        {ID_GENESIS, {
            {THEGAMESDB, QStringLiteral("18")},
            {SCREENSCRAPER, QStringLiteral("1")},
            {IGDB, QStringLiteral("genesis")},
        }},
        
        {ID_MASTER_SYSTEM, {
            {THEGAMESDB, QStringLiteral("35")},
            {SCREENSCRAPER, QStringLiteral("2")},
            {IGDB, QStringLiteral("sms")},
        }},
        
        {ID_GAME_GEAR, {
            {THEGAMESDB, QStringLiteral("20")},
            {SCREENSCRAPER, QStringLiteral("8")},
            {IGDB, QStringLiteral("gamegear")},
        }},
        
        {ID_SATURN, {
            {THEGAMESDB, QStringLiteral("17")},
            {SCREENSCRAPER, QStringLiteral("22")},
            {IGDB, QStringLiteral("saturn")},
        }},
        
        {ID_DREAMCAST, {
            {THEGAMESDB, QStringLiteral("16")},
            {SCREENSCRAPER, QStringLiteral("23")},
            {IGDB, QStringLiteral("dreamcast")},
        }},
        
        {ID_SEGA_CD, {
            {THEGAMESDB, QStringLiteral("21")},
            {SCREENSCRAPER, QStringLiteral("20")},
            {IGDB, QStringLiteral("segacd")},
        }},
        
        {ID_32X, {
            {THEGAMESDB, QStringLiteral("33")},
            {SCREENSCRAPER, QStringLiteral("19")},
            {IGDB, QStringLiteral("sega32x")},
        }},
        
        // ====================================================================
        // Sony Systems
        // ====================================================================
        {ID_PSX, {
            {THEGAMESDB, QStringLiteral("10")},
            {SCREENSCRAPER, QStringLiteral("57")},
            {IGDB, QStringLiteral("playstation")},
        }},
        
        {ID_PS2, {
            {THEGAMESDB, QStringLiteral("11")},
            {SCREENSCRAPER, QStringLiteral("58")},
            {IGDB, QStringLiteral("ps2")},
        }},
        
        {ID_PSP, {
            {THEGAMESDB, QStringLiteral("13")},
            {SCREENSCRAPER, QStringLiteral("61")},
            {IGDB, QStringLiteral("psp")},
        }},
        
        {ID_PSVITA, {
            {THEGAMESDB, QStringLiteral("39")},
            {SCREENSCRAPER, QStringLiteral("62")},
            {IGDB, QStringLiteral("psvita")},
        }},
        
        // ====================================================================
        // Atari Systems
        // ====================================================================
        {ID_ATARI_2600, {
            {THEGAMESDB, QStringLiteral("22")},
            {SCREENSCRAPER, QStringLiteral("26")},
            {IGDB, QStringLiteral("atari2600")},
        }},
        
        {ID_ATARI_7800, {
            {THEGAMESDB, QStringLiteral("27")},
            {SCREENSCRAPER, QStringLiteral("43")},
            {IGDB, QStringLiteral("atari7800")},
        }},
        
        {ID_LYNX, {
            {THEGAMESDB, QStringLiteral("28")},
            {SCREENSCRAPER, QStringLiteral("28")},
            {IGDB, QStringLiteral("lynx")},
        }},
        
        {ID_ATARI_JAGUAR, {
            {THEGAMESDB, QStringLiteral("29")},
            {SCREENSCRAPER, QStringLiteral("27")},
            {IGDB, QStringLiteral("jaguar")},
        }},
        
        // ====================================================================
        // Other Systems
        // ====================================================================
        {ID_TURBOGRAFX16, {
            {THEGAMESDB, QStringLiteral("34")},
            {SCREENSCRAPER, QStringLiteral("31")},
            {IGDB, QStringLiteral("turbografx16")},
        }},
        
        {ID_TURBOGRAFX_CD, {
            {THEGAMESDB, QStringLiteral("34")},  // Same as TG-16
            {SCREENSCRAPER, QStringLiteral("114")},
            {IGDB, QStringLiteral("turbografxcd")},
        }},
        
        {ID_NEO_GEO, {
            {THEGAMESDB, QStringLiteral("24")},
            {SCREENSCRAPER, QStringLiteral("142")},
            {IGDB, QStringLiteral("neogeo")},
        }},
        
        {ID_NGP, {
            {THEGAMESDB, QStringLiteral("4922")},
            {SCREENSCRAPER, QStringLiteral("25")},
            {IGDB, QStringLiteral("ngp")},
        }},
        
        {ID_WONDERSWAN, {
            {THEGAMESDB, QStringLiteral("45")},
            {SCREENSCRAPER, QStringLiteral("45")},
            {IGDB, QStringLiteral("wonderswan")},
        }},
        
        {ID_XBOX, {
            {THEGAMESDB, QStringLiteral("14")},
            {SCREENSCRAPER, QStringLiteral("32")},
            {IGDB, QStringLiteral("xbox")},
        }},
        
        {ID_XBOX360, {
            {THEGAMESDB, QStringLiteral("15")},
            {SCREENSCRAPER, QStringLiteral("33")},
            {IGDB, QStringLiteral("xbox360")},
        }},
        
        {ID_ARCADE, {
            {THEGAMESDB, QStringLiteral("23")},
            {SCREENSCRAPER, QStringLiteral("75")},
            {IGDB, QStringLiteral("arcade")},
        }},
        
        {ID_C64, {
            {THEGAMESDB, QStringLiteral("40")},
            {SCREENSCRAPER, QStringLiteral("66")},
            {IGDB, QStringLiteral("c64")},
        }},
        
        {ID_AMIGA, {
            {THEGAMESDB, QStringLiteral("4911")},
            {SCREENSCRAPER, QStringLiteral("64")},
            {IGDB, QStringLiteral("amiga")},
        }},
        
        {ID_ZX_SPECTRUM, {
            {THEGAMESDB, QStringLiteral("4913")},
            {SCREENSCRAPER, QStringLiteral("76")},
            {IGDB, QStringLiteral("zxspectrum")},
        }},
        
        {ID_SUPERGRAFX, {
            {THEGAMESDB, QStringLiteral("34")},  // Same as TG-16
            {SCREENSCRAPER, QStringLiteral("105")},
            {IGDB, QStringLiteral("supergrafx")},
        }},
    };
}

} // namespace Remus
