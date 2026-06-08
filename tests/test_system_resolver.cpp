#include <QtTest/QtTest>
#include "../src/core/system_resolver.h"
#include "../src/core/constants/systems.h"
#include "../src/core/constants/providers.h"

using namespace Remus;

class SystemResolverTest : public QObject {
    Q_OBJECT

private slots:
    void testDisplayName();
    void testInternalName();
    void testProviderName();
    void testSystemIdByName();
    void testIsValidSystem();
    void testSystemIdByDatName();
    void testSystemIdByDatName_caseInsensitive();
    void testResolveSystemName();
};

void SystemResolverTest::testDisplayName() {
    QCOMPARE(SystemResolver::displayName(Constants::Systems::ID_GENESIS), QStringLiteral("Sega Genesis / Mega Drive"));
    QCOMPARE(SystemResolver::displayName(-1), QStringLiteral("Unknown"));
}

void SystemResolverTest::testInternalName() {
    QCOMPARE(SystemResolver::internalName(Constants::Systems::ID_GENESIS), QStringLiteral("Genesis"));
    QCOMPARE(SystemResolver::internalName(-1), QStringLiteral("Unknown"));
}

void SystemResolverTest::testProviderName() {
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::THEGAMESDB),
        QStringLiteral("18"));
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::SCREENSCRAPER),
        QStringLiteral("1"));
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::IGDB),
        QStringLiteral("genesis"));
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::HASHEOUS),
        QStringLiteral("Genesis"));
    QCOMPARE(SystemResolver::providerName(-1, Constants::Providers::IGDB), QString());
}

void SystemResolverTest::testSystemIdByName() {
    QCOMPARE(SystemResolver::systemIdByName("Genesis"), Constants::Systems::ID_GENESIS);
    QCOMPARE(SystemResolver::systemIdByName("Unknown"), 0);
}

void SystemResolverTest::testIsValidSystem() {
    QVERIFY(SystemResolver::isValidSystem(Constants::Systems::ID_GENESIS));
    QVERIFY(!SystemResolver::isValidSystem(-1));
}

void SystemResolverTest::testSystemIdByDatName() {
    using namespace Constants::Systems;

    // No-Intro canonical DAT names
    QCOMPARE(SystemResolver::systemIdByDatName("Sega - Mega Drive - Genesis"), ID_GENESIS);
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Nintendo Entertainment System"), ID_NES);
    QCOMPARE(SystemResolver::systemIdByDatName("Sony - PlayStation"), ID_PSX);
    QCOMPARE(SystemResolver::systemIdByDatName("Sony - PlayStation 2"), ID_PS2);
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - GameCube"), ID_GAMECUBE);
    QCOMPARE(SystemResolver::systemIdByDatName("Sega - Dreamcast"), ID_DREAMCAST);
    QCOMPARE(SystemResolver::systemIdByDatName("Sega - Master System - Mark III"), ID_MASTER_SYSTEM);
    QCOMPARE(SystemResolver::systemIdByDatName("NEC - PC Engine - TurboGrafx-16"), ID_TURBOGRAFX16);

    // GameTDB-style DAT names (prepend "Nintendo - Nintendo" pattern)
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Nintendo GameCube"), ID_GAMECUBE);
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Nintendo Wii"), ID_WII);

    // Systems previously missing from the resolver
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Family Computer Disk System"), ID_FDS);
    QCOMPARE(SystemResolver::systemIdByDatName("The 3DO Company - 3DO"), ID_3DO);
    QCOMPARE(SystemResolver::systemIdByDatName("SNK - Neo Geo"), ID_NEO_GEO);
    QCOMPARE(SystemResolver::systemIdByDatName("SNK - Neo Geo CD"), ID_NEO_GEO_CD);

    // Newly added systems
    QCOMPARE(SystemResolver::systemIdByDatName("Atari - 5200"), ID_ATARI_5200);
    QCOMPARE(SystemResolver::systemIdByDatName("Atari - 8-bit Family"), ID_ATARI_8BIT);
    QCOMPARE(SystemResolver::systemIdByDatName("Atari - ST"), ID_ATARI_ST);
    QCOMPARE(SystemResolver::systemIdByDatName("Atari - Jaguar CD"), ID_ATARI_JAGUAR_CD);
    QCOMPARE(SystemResolver::systemIdByDatName("Coleco - ColecoVision"), ID_COLECOVISION);
    QCOMPARE(SystemResolver::systemIdByDatName("Mattel - Intellivision"), ID_INTELLIVISION);
    QCOMPARE(SystemResolver::systemIdByDatName("Microsoft - MSX"), ID_MSX);
    QCOMPARE(SystemResolver::systemIdByDatName("Microsoft - MSX2"), ID_MSX2);
    QCOMPARE(SystemResolver::systemIdByDatName("NEC - PC-FX"), ID_PC_FX);
    QCOMPARE(SystemResolver::systemIdByDatName("Philips - CD-i"), ID_CDI);
    QCOMPARE(SystemResolver::systemIdByDatName("Commodore - CD32"), ID_CD32);
    QCOMPARE(SystemResolver::systemIdByDatName("Sega - SG-1000"), ID_SG1000);
    QCOMPARE(SystemResolver::systemIdByDatName("Sega - Naomi"), ID_NAOMI);
    QCOMPARE(SystemResolver::systemIdByDatName("Sega - Naomi 2"), ID_NAOMI);
    QCOMPARE(SystemResolver::systemIdByDatName("Sony - PlayStation 3"), ID_PS3);
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Wii U"), ID_WIIU);

    // Digital / download variants
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Wii (Digital)"), ID_WII);
    QCOMPARE(SystemResolver::systemIdByDatName("Nintendo - Wii U (Digital)"), ID_WIIU);
    QCOMPARE(SystemResolver::systemIdByDatName("Microsoft - XBOX 360 (Games on Demand)"), ID_XBOX360);

    // Unknown DAT name returns 0
    QCOMPARE(SystemResolver::systemIdByDatName("Unknown System"), 0);
    QCOMPARE(SystemResolver::systemIdByDatName(""), 0);
}

void SystemResolverTest::testSystemIdByDatName_caseInsensitive() {
    using namespace Constants::Systems;

    QCOMPARE(SystemResolver::systemIdByDatName("sega - mega drive - genesis"), ID_GENESIS);
    QCOMPARE(SystemResolver::systemIdByDatName("SONY - PLAYSTATION"), ID_PSX);
}

void SystemResolverTest::testResolveSystemName() {
    // Internal name passes through
    QCOMPARE(SystemResolver::resolveSystemName("Genesis"), QStringLiteral("Genesis"));
    QCOMPARE(SystemResolver::resolveSystemName("PlayStation"), QStringLiteral("PlayStation"));

    // DAT name resolves to internal name
    QCOMPARE(SystemResolver::resolveSystemName("Sega - Mega Drive - Genesis"), QStringLiteral("Genesis"));
    QCOMPARE(SystemResolver::resolveSystemName("Sony - PlayStation 2"), QStringLiteral("PlayStation 2"));
    QCOMPARE(SystemResolver::resolveSystemName("Nintendo - GameCube"), QStringLiteral("GameCube"));

    // Unknown name passes through unchanged
    QCOMPARE(SystemResolver::resolveSystemName("Unknown DAT Name"), QStringLiteral("Unknown DAT Name"));
}

QTEST_MAIN(SystemResolverTest)
#include "test_system_resolver.moc"
