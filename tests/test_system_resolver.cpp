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

QTEST_MAIN(SystemResolverTest)
#include "test_system_resolver.moc"
