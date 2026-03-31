#include <QtTest/QtTest>
#include "../src/core/system_resolver.h"
#include "../src/core/constants/systems.h"
#include "../src/core/constants/providers.h"

using namespace Remus;

class SystemResolverTest : public QObject
{
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

void SystemResolverTest::testDisplayName()
{
    QCOMPARE(SystemResolver::displayName(Constants::Systems::ID_GENESIS), QStringLiteral("Sega Genesis / Mega Drive"));
    QCOMPARE(SystemResolver::displayName(-1), QStringLiteral("Unknown"));
}

void SystemResolverTest::testInternalName()
{
    QCOMPARE(SystemResolver::internalName(Constants::Systems::ID_GENESIS), QStringLiteral("Genesis"));
    QCOMPARE(SystemResolver::internalName(-1), QStringLiteral("Unknown"));
}

void SystemResolverTest::testProviderName()
{
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::THEGAMESDB), QStringLiteral("18"));
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::SCREENSCRAPER), QStringLiteral("1"));
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::IGDB), QStringLiteral("genesis"));
    QCOMPARE(SystemResolver::providerName(Constants::Systems::ID_GENESIS, Constants::Providers::HASHEOUS), QStringLiteral("Genesis"));
    QCOMPARE(SystemResolver::providerName(-1, Constants::Providers::IGDB), QString());
}

void SystemResolverTest::testSystemIdByName()
{
    QCOMPARE(SystemResolver::systemIdByName("Genesis"), Constants::Systems::ID_GENESIS);
    QCOMPARE(SystemResolver::systemIdByName("Unknown"), 0);
}

void SystemResolverTest::testIsValidSystem()
{
    QVERIFY(SystemResolver::isValidSystem(Constants::Systems::ID_GENESIS));
    QVERIFY(!SystemResolver::isValidSystem(-1));
}

void SystemResolverTest::testSystemIdByDatName()
{
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

    // Unknown DAT name returns 0
    QCOMPARE(SystemResolver::systemIdByDatName("Unknown System"), 0);
    QCOMPARE(SystemResolver::systemIdByDatName(""), 0);
}

void SystemResolverTest::testSystemIdByDatName_caseInsensitive()
{
    using namespace Constants::Systems;

    QCOMPARE(SystemResolver::systemIdByDatName("sega - mega drive - genesis"), ID_GENESIS);
    QCOMPARE(SystemResolver::systemIdByDatName("SONY - PLAYSTATION"), ID_PSX);
}

void SystemResolverTest::testResolveSystemName()
{
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
