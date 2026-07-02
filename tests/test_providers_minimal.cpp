#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include "metadata/hasheous_provider.h"
#include "metadata/igdb_provider.h"
#include "metadata/screenscraper_provider.h"
#include "metadata/thegamesdb_provider.h"
#include "core/constants/constants.h"

using namespace Remus;

class InspectableIGDBProvider : public IGDBProvider {
public:
    using IGDBProvider::IGDBProvider;

    bool authenticatedForTest() const {
        return m_authenticated;
    }
};

class InspectableHasheousProvider : public HasheousProvider {
public:
    using HasheousProvider::HasheousProvider;

    bool proxyEnabledForTest() const {
        return metadataProxyEnabled();
    }
};

class ProvidersMinimalTest : public QObject {
    Q_OBJECT

private slots:
    void hasheousIdLookupUnsupported();
    void igdbHashUnsupported();
    void igdbCredentialsDoNotPreAuthenticate();
    void screenscraperRequiresAuth();
    void thegamesdbHashUnsupported();
    // Security validation tests
    void igdbGetByIdRejectsNonNumericId();
    void igdbGetByIdRejectsZeroOrNegativeId();
    void igdbGetArtworkRejectsNonNumericId();
    void igdbFetchPlatformSlugRejectsInvalidSlug();
    void hasheousConstructorIgnoresQSettings();
};

void ProvidersMinimalTest::hasheousIdLookupUnsupported() {
    HasheousProvider provider;
    QSignalSpy spy(&provider, &HasheousProvider::errorOccurred);
    GameMetadata md = provider.getById("123");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
}

void ProvidersMinimalTest::igdbHashUnsupported() {
    IGDBProvider provider;
    QSignalSpy spy(&provider, &IGDBProvider::errorOccurred);
    GameMetadata md = provider.getByHash("abcd", "NES");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
    QVERIFY(!provider.isAvailable());
}

void ProvidersMinimalTest::igdbCredentialsDoNotPreAuthenticate() {
    InspectableIGDBProvider provider;
    provider.setCredentials(QStringLiteral("client"), QStringLiteral("secret"));

    // Setting credentials should make the provider configurable/available,
    // but it must not mark the bearer-token auth state as already satisfied.
    QVERIFY(provider.isAvailable());
    QVERIFY(!provider.authenticatedForTest());
}

void ProvidersMinimalTest::screenscraperRequiresAuth() {
    ScreenScraperProvider provider;
    QSignalSpy spy(&provider, &ScreenScraperProvider::errorOccurred);
    GameMetadata md = provider.getByHash("abcd", "NES");
    QVERIFY(md.title.isEmpty());
    QVERIFY(!spy.isEmpty());
}

void ProvidersMinimalTest::thegamesdbHashUnsupported() {
    TheGamesDBProvider provider;
    QSignalSpy spy(&provider, &TheGamesDBProvider::errorOccurred);
    QVERIFY(!provider.isAvailable());
    GameMetadata md = provider.getByHash("abcd", "NES");
    QVERIFY(md.title.isEmpty());
    // getByHash() is a silent no-op for TheGamesDB (no error signal emitted);
    // the orchestrator's supportsHashMatch flag prevents this path in normal use.
    QVERIFY(spy.isEmpty());
    provider.setApiKey(QStringLiteral("test-api-key"));
    QVERIFY(provider.isAvailable());
}

void ProvidersMinimalTest::igdbGetByIdRejectsNonNumericId() {
    IGDBProvider provider;
    QSignalSpy errorSpy(&provider, &IGDBProvider::errorOccurred);

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::getById.*rejected non-numeric id.*not-a-number")));
    const GameMetadata md = provider.getById(QStringLiteral("not-a-number"));

    QVERIFY(md.title.isEmpty());
    // Validation fires before authentication — errorOccurred must not be emitted.
    QVERIFY(errorSpy.isEmpty());
}

void ProvidersMinimalTest::igdbGetByIdRejectsZeroOrNegativeId() {
    IGDBProvider provider;
    QSignalSpy errorSpy(&provider, &IGDBProvider::errorOccurred);

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::getById.*rejected non-numeric id.*\\b0\\b")));
    const GameMetadata md0 = provider.getById(QStringLiteral("0"));
    QVERIFY(md0.title.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::getById.*rejected non-numeric id.*-1")));
    const GameMetadata mdNeg = provider.getById(QStringLiteral("-1"));
    QVERIFY(mdNeg.title.isEmpty());
    QCOMPARE(errorSpy.size(), 0);
}

void ProvidersMinimalTest::igdbGetArtworkRejectsNonNumericId() {
    IGDBProvider provider;

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::getArtwork.*rejected non-numeric id.*abc")));
    const ArtworkUrls urls = provider.getArtwork(QStringLiteral("abc"));

    QVERIFY(urls.boxFront.isEmpty());
    QVERIFY(urls.screenshot.isEmpty());
}

void ProvidersMinimalTest::igdbFetchPlatformSlugRejectsInvalidSlug() {
    IGDBProvider provider;
    QSignalSpy errorSpy(&provider, &IGDBProvider::errorOccurred);

    // Uppercase letters are not permitted in platform slugs.
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::fetchGamesByPlatformSlug.*rejected invalid slug.*NES")));
    const auto r1 = provider.fetchGamesByPlatformSlug(QStringLiteral("NES"));
    QVERIFY(r1.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    // Spaces are not permitted.
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::fetchGamesByPlatformSlug.*rejected invalid slug")));
    const auto r2 = provider.fetchGamesByPlatformSlug(QStringLiteral("invalid slug"));
    QVERIFY(r2.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    // Empty string is not permitted.
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("IGDB::fetchGamesByPlatformSlug.*rejected invalid slug")));
    const auto r3 = provider.fetchGamesByPlatformSlug(QString());
    QVERIFY(r3.isEmpty());
    QVERIFY(errorSpy.isEmpty());
}

void ProvidersMinimalTest::hasheousConstructorIgnoresQSettings() {
    // The security audit removed the QSettings credential read from the constructor.
    // Even if QSettings has the API key set, the provider must not self-populate.
    QSettings s(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    s.setValue(QStringLiteral("hasheous/client_api_key"), QStringLiteral("qs_api_key"));
    s.sync();

    InspectableHasheousProvider provider;

    QVERIFY2(!provider.proxyEnabledForTest(), "HasheousProvider constructor must not read API key from QSettings");

    // Explicit setApiKey() must still work.
    provider.setApiKey(QStringLiteral("qs_api_key"));
    QVERIFY(provider.proxyEnabledForTest());

    s.remove(QStringLiteral("hasheous/client_api_key"));
    s.sync();
}

QTEST_MAIN(ProvidersMinimalTest)
#include "test_providers_minimal.moc"
