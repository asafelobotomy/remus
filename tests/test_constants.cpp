#include <QtTest/QtTest>
#include "constants/constants.h"

using namespace Remus::Constants;

/**
 * @brief Unit tests for the constants library
 * 
 * Tests provider and system lookup functions, ensuring
 * the constants library provides correct data.
 */
class ConstantsTest : public QObject {
    Q_OBJECT

private slots:
    // Provider tests
    void testProviderRegistry();
    void testProviderLookup();
    void testProviderDisplayNames();
    void testProviderPriority();
    void testProviderCapabilities();
    
    // System tests
    void testSystemRegistry();
    void testSystemLookup();
    void testSystemByName();
    void testSystemExtensions();
    void testAmbiguousExtensions();
    void testSystemGrouping();

    // Template and settings tests
    void testTemplateDefaults();
    void testTemplateVariables();
    void testSettingsDefaults();
};

// ============================================================================
// Provider Tests
// ============================================================================

void ConstantsTest::testProviderRegistry() {
    // Verify all expected providers are registered
    QVERIFY(Providers::PROVIDER_REGISTRY.contains(Providers::HASHEOUS));
    QVERIFY(Providers::PROVIDER_REGISTRY.contains(Providers::SCREENSCRAPER));
    QVERIFY(Providers::PROVIDER_REGISTRY.contains(Providers::THEGAMESDB));
    QVERIFY(Providers::PROVIDER_REGISTRY.contains(Providers::IGDB));
    
    // Verify registry size
    QCOMPARE(Providers::PROVIDER_REGISTRY.size(), 4);
}

void ConstantsTest::testProviderLookup() {
    // Test valid provider lookup
    auto info = Providers::getProviderInfo(Providers::SCREENSCRAPER);
    QVERIFY(info != nullptr);
    QCOMPARE(info->id, QString(Providers::SCREENSCRAPER));
    QCOMPARE(info->displayName, Providers::DISPLAY_SCREENSCRAPER);
    QVERIFY(info->requiresAuth);
    
    // Test invalid provider lookup
    auto invalid = Providers::getProviderInfo("nonexistent");
    QVERIFY(invalid == nullptr);
}

void ConstantsTest::testProviderDisplayNames() {
    // Test display name retrieval
    QCOMPARE(Providers::getProviderDisplayName(Providers::SCREENSCRAPER), 
             Providers::DISPLAY_SCREENSCRAPER);
    QCOMPARE(Providers::getProviderDisplayName(Providers::IGDB), 
             Providers::DISPLAY_IGDB);
    
    // Test unknown provider returns "Unknown"
    QCOMPARE(Providers::getProviderDisplayName("invalid"), 
             QStringLiteral("Unknown"));
}

void ConstantsTest::testProviderPriority() {
    // Get providers sorted by priority
    auto providers = Providers::getProvidersByPriority();
    
    // Verify order (highest priority first)
    QVERIFY(providers.size() >= 4);
    QCOMPARE(providers[0], QString(Providers::HASHEOUS));  // Priority 100
    QCOMPARE(providers[1], QString(Providers::SCREENSCRAPER));  // Priority 90
    
    // Verify priorities are descending
    for (int i = 0; i < providers.size() - 1; ++i) {
        auto current = Providers::getProviderInfo(providers[i]);
        auto next = Providers::getProviderInfo(providers[i + 1]);
        QVERIFY(current->priority >= next->priority);
    }
}

void ConstantsTest::testProviderCapabilities() {
    // Test hash-supporting providers
    auto hashProviders = Providers::getHashSupportingProviders();
    QVERIFY(hashProviders.contains(Providers::HASHEOUS));
    QVERIFY(hashProviders.contains(Providers::SCREENSCRAPER));
    QVERIFY(!hashProviders.contains(Providers::IGDB));  // IGDB doesn't support hash
    
    // Test name-supporting providers
    auto nameProviders = Providers::getNameSupportingProviders();
    QVERIFY(nameProviders.contains(Providers::SCREENSCRAPER));
    QVERIFY(nameProviders.contains(Providers::THEGAMESDB));
    QVERIFY(nameProviders.contains(Providers::IGDB));
    QVERIFY(!nameProviders.contains(Providers::HASHEOUS));  // Hasheous is hash-only
}

// ============================================================================
// System Tests
// ============================================================================

void ConstantsTest::testSystemRegistry() {
    // Verify registry is not empty
    QVERIFY(Systems::SYSTEMS.size() > 0);
    
    // Verify expected systems exist
    QVERIFY(Systems::SYSTEMS.contains(Systems::ID_NES));
    QVERIFY(Systems::SYSTEMS.contains(Systems::ID_SNES));
    QVERIFY(Systems::SYSTEMS.contains(Systems::ID_PSX));
    QVERIFY(Systems::SYSTEMS.contains(Systems::ID_N64));
    
    // Verify registry has reasonable size (20+ systems)
    QVERIFY(Systems::SYSTEMS.size() >= 20);
}

void ConstantsTest::testSystemLookup() {
    // Test valid system lookup by ID
    auto nes = Systems::getSystem(Systems::ID_NES);
    QVERIFY(nes != nullptr);
    QCOMPARE(nes->id, Systems::ID_NES);
    QCOMPARE(nes->internalName, QStringLiteral("NES"));
    QCOMPARE(nes->displayName, QStringLiteral("Nintendo Entertainment System"));
    QCOMPARE(nes->manufacturer, QStringLiteral("Nintendo"));
    QCOMPARE(nes->generation, 3);
    QCOMPARE(nes->preferredHash, QStringLiteral("CRC32"));
    QVERIFY(!nes->isMultiFile);
    
    // Test PlayStation (multi-file system)
    auto psx = Systems::getSystem(Systems::ID_PSX);
    QVERIFY(psx != nullptr);
    QCOMPARE(psx->internalName, QStringLiteral("PlayStation"));
    QCOMPARE(psx->preferredHash, QStringLiteral("MD5"));
    QVERIFY(psx->isMultiFile);
    
    // Test invalid system lookup
    auto invalid = Systems::getSystem(9999);
    QVERIFY(invalid == nullptr);
}

void ConstantsTest::testSystemByName() {
    // Test lookup by internal name
    auto nesId = Systems::getSystemIdByName("NES");
    QCOMPARE(nesId, Systems::ID_NES);
    
    auto snesId = Systems::getSystemIdByName("SNES");
    QCOMPARE(snesId, Systems::ID_SNES);
    
    auto psxDef = Systems::getSystemByName("PlayStation");
    QVERIFY(psxDef != nullptr);
    QCOMPARE(psxDef->id, Systems::ID_PSX);
    
    // Test invalid lookup
    auto invalid = Systems::getSystemIdByName("NonExistentSystem");
    QCOMPARE(invalid, 0);
    
    auto invalidDef = Systems::getSystemByName("Invalid");
    QVERIFY(invalidDef == nullptr);
}

void ConstantsTest::testSystemExtensions() {
    // Test extension to system mapping
    auto nesSystems = Systems::getSystemsForExtension(".nes");
    QCOMPARE(nesSystems.size(), 1);
    QCOMPARE(nesSystems[0], Systems::ID_NES);
    
    auto snesSystems = Systems::getSystemsForExtension(".sfc");
    QCOMPARE(snesSystems.size(), 1);
    QCOMPARE(snesSystems[0], Systems::ID_SNES);
    
    // Test case insensitivity
    auto nesUpperSystems = Systems::getSystemsForExtension(".NES");
    QCOMPARE(nesUpperSystems.size(), 1);
    QCOMPARE(nesUpperSystems[0], Systems::ID_NES);
    
    // Test unknown extension
    auto unknown = Systems::getSystemsForExtension(".unknown");
    QVERIFY(unknown.isEmpty());
}

void ConstantsTest::testAmbiguousExtensions() {
    // Test ambiguous extensions (used by multiple systems)
    auto isoSystems = Systems::getSystemsForExtension(".iso");
    QVERIFY(isoSystems.size() > 1);  // ISO used by PS1, PS2, GameCube, etc.
    QVERIFY(isoSystems.contains(Systems::ID_PSX));
    QVERIFY(isoSystems.contains(Systems::ID_PS2));
    
    QVERIFY(Systems::isAmbiguousExtension(".iso"));
    QVERIFY(Systems::isAmbiguousExtension(".cue"));  // PS1, Saturn, Sega CD
    
    // Test unambiguous extensions
    QVERIFY(!Systems::isAmbiguousExtension(".nes"));
    QVERIFY(!Systems::isAmbiguousExtension(".gba"));
}

void ConstantsTest::testSystemGrouping() {
    // Test manufacturer groupings
    QVERIFY(Systems::NINTENDO_SYSTEMS.contains(Systems::ID_NES));
    QVERIFY(Systems::NINTENDO_SYSTEMS.contains(Systems::ID_SNES));
    QVERIFY(Systems::NINTENDO_SYSTEMS.contains(Systems::ID_N64));
    QVERIFY(!Systems::NINTENDO_SYSTEMS.contains(Systems::ID_PSX));
    
    QVERIFY(Systems::SONY_SYSTEMS.contains(Systems::ID_PSX));
    QVERIFY(Systems::SONY_SYSTEMS.contains(Systems::ID_PS2));
    QVERIFY(!Systems::SONY_SYSTEMS.contains(Systems::ID_NES));
    
    QVERIFY(Systems::SEGA_SYSTEMS.contains(Systems::ID_GENESIS));
    QVERIFY(Systems::SEGA_SYSTEMS.contains(Systems::ID_DREAMCAST));
    
    // Test media type groupings
    QVERIFY(Systems::DISC_SYSTEMS.contains(Systems::ID_PSX));
    QVERIFY(Systems::DISC_SYSTEMS.contains(Systems::ID_SATURN));
    QVERIFY(!Systems::DISC_SYSTEMS.contains(Systems::ID_NES));
    
    QVERIFY(Systems::CARTRIDGE_SYSTEMS.contains(Systems::ID_NES));
    QVERIFY(Systems::CARTRIDGE_SYSTEMS.contains(Systems::ID_SNES));
    QVERIFY(!Systems::CARTRIDGE_SYSTEMS.contains(Systems::ID_PSX));
    
    // Test handheld grouping
    QVERIFY(Systems::HANDHELD_SYSTEMS.contains(Systems::ID_GB));
    QVERIFY(Systems::HANDHELD_SYSTEMS.contains(Systems::ID_GBA));
    QVERIFY(Systems::HANDHELD_SYSTEMS.contains(Systems::ID_PSP));
    QVERIFY(!Systems::HANDHELD_SYSTEMS.contains(Systems::ID_NES));
}

// ============================================================================
// Template and Settings Tests
// ============================================================================

void ConstantsTest::testTemplateDefaults() {
    QCOMPARE(Templates::DEFAULT_SIMPLE, QStringLiteral("{title} ({region})"));
    QVERIFY(Templates::DEFAULT_NO_INTRO.contains(QStringLiteral("{title}")));
    QVERIFY(Templates::DEFAULT_REDUMP.contains(QStringLiteral("{disc}")));
}

void ConstantsTest::testTemplateVariables() {
    QVERIFY(Templates::ALL_VARIABLES.contains(Templates::Variables::TITLE));
    QVERIFY(Templates::ALL_VARIABLES.contains(Templates::Variables::EXT));
    QVERIFY(Templates::ALL_VARIABLES.contains(Templates::Variables::ID));
    QVERIFY(Templates::isValidVariable(QStringLiteral("title")));
    QVERIFY(!Templates::isValidVariable(QStringLiteral("unknown")));
}

void ConstantsTest::testSettingsDefaults() {
    QCOMPARE(Settings::Defaults::NAMING_TEMPLATE, Templates::DEFAULT_SIMPLE);
    QCOMPARE(QString(Settings::Organize::NAMING_TEMPLATE), QStringLiteral("organize/naming_template"));
    QCOMPARE(QString(Settings::Providers::SCREENSCRAPER_USERNAME), QStringLiteral("screenscraper/username"));
    QCOMPARE(Settings::Defaults::PROVIDER_PRIORITY, QStringLiteral("ScreenScraper (Primary)"));
}

QTEST_MAIN(ConstantsTest)
#include "test_constants.moc"
