#include <QtTest>
#include <QSignalSpy>
#include "metadata/hasheous_provider.h"
#include "metadata/igdb_provider.h"
#include "metadata/screenscraper_provider.h"
#include "metadata/thegamesdb_provider.h"

using namespace Remus;

class ProvidersMinimalTest : public QObject {
    Q_OBJECT

private slots:
    void hasheousIdLookupUnsupported();
    void igdbHashUnsupported();
    void screenscraperRequiresAuth();
    void thegamesdbHashUnsupported();
};

void ProvidersMinimalTest::hasheousIdLookupUnsupported()
{
    HasheousProvider provider;
    QSignalSpy spy(&provider, &HasheousProvider::errorOccurred);
    GameMetadata md = provider.getById("123");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
}

void ProvidersMinimalTest::igdbHashUnsupported()
{
    IGDBProvider provider;
    QSignalSpy spy(&provider, &IGDBProvider::errorOccurred);
    GameMetadata md = provider.getByHash("abcd", "NES");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
    QVERIFY(!provider.isAvailable());
}

void ProvidersMinimalTest::screenscraperRequiresAuth()
{
    ScreenScraperProvider provider;
    QSignalSpy spy(&provider, &ScreenScraperProvider::errorOccurred);
    GameMetadata md = provider.getByHash("abcd", "NES");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
}

void ProvidersMinimalTest::thegamesdbHashUnsupported()
{
    TheGamesDBProvider provider;
    QSignalSpy spy(&provider, &TheGamesDBProvider::errorOccurred);
    GameMetadata md = provider.getByHash("abcd", "NES");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
    QVERIFY(provider.isAvailable());
}

QTEST_MAIN(ProvidersMinimalTest)
#include "test_providers_minimal.moc"
