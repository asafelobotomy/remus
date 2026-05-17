#include <QtTest>
#include <QSignalSpy>
#include "metadata/hasheous_provider.h"
#include "metadata/igdb_provider.h"
#include "metadata/screenscraper_provider.h"
#include "metadata/thegamesdb_provider.h"

using namespace Remus;

class InspectableIGDBProvider : public IGDBProvider {
public:
    using IGDBProvider::IGDBProvider;

    bool authenticatedForTest() const { return m_authenticated; }
};

class ProvidersMinimalTest : public QObject {
    Q_OBJECT

private slots:
    void hasheousIdLookupUnsupported();
    void igdbHashUnsupported();
    void igdbCredentialsDoNotPreAuthenticate();
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

void ProvidersMinimalTest::igdbCredentialsDoNotPreAuthenticate()
{
    InspectableIGDBProvider provider;
    provider.setCredentials(QStringLiteral("client"), QStringLiteral("secret"));

    // Setting credentials should make the provider configurable/available,
    // but it must not mark the bearer-token auth state as already satisfied.
    QVERIFY(provider.isAvailable());
    QVERIFY(!provider.authenticatedForTest());
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
    // getByHash() is a silent no-op for TheGamesDB (no error signal emitted);
    // the orchestrator's supportsHashMatch flag prevents this path in normal use.
    QVERIFY(spy.isEmpty());
    QVERIFY(provider.isAvailable());
}

QTEST_MAIN(ProvidersMinimalTest)
#include "test_providers_minimal.moc"
