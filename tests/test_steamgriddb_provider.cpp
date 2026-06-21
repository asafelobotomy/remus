#include <QtTest>

#include "metadata/steamgriddb_provider.h"
#include "core/constants/constants.h"

using namespace Remus;

class SteamGridDBProviderTest : public QObject {
    Q_OBJECT

private slots:
    void unavailableWithoutApiKey();
    void artOnlyLookupMethodsReturnEmpty();
    void resolveArtworkLookupIdPrefersSteamExternalId();
    void resolveArtworkLookupIdKeepsExplicitPrefixes();
};

void SteamGridDBProviderTest::unavailableWithoutApiKey() {
    SteamGridDBProvider provider;
    QVERIFY(!provider.isAvailable());
}

void SteamGridDBProviderTest::artOnlyLookupMethodsReturnEmpty() {
    SteamGridDBProvider provider;
    provider.setApiKey(QStringLiteral("test-key"));
    QVERIFY(provider.isAvailable());

    QVERIFY(provider.searchByName(QStringLiteral("Half-Life")).isEmpty());
    QVERIFY(provider.getByHash(QStringLiteral("deadbeef"), QStringLiteral("PC")).title.isEmpty());
    QVERIFY(provider.getById(QStringLiteral("123")).title.isEmpty());
    QVERIFY(provider.getArtwork(QStringLiteral("not-a-valid-id")).boxFront.isEmpty());
}

void SteamGridDBProviderTest::resolveArtworkLookupIdPrefersSteamExternalId() {
    QMap<QString, QString> externalIds;
    externalIds.insert(Constants::Providers::ExternalId::STEAM, QStringLiteral("730"));

    const QString resolved = SteamGridDBProvider::resolveArtworkLookupId(QStringLiteral("12345"), externalIds);
    QCOMPARE(resolved, QStringLiteral("steam:730"));
}

void SteamGridDBProviderTest::resolveArtworkLookupIdKeepsExplicitPrefixes() {
    const QMap<QString, QString> empty;

    QCOMPARE(SteamGridDBProvider::resolveArtworkLookupId(QStringLiteral("steam:220"), empty),
        QStringLiteral("steam:220"));
    QCOMPARE(SteamGridDBProvider::resolveArtworkLookupId(QStringLiteral("sgdb:42"), empty), QStringLiteral("sgdb:42"));
    QCOMPARE(SteamGridDBProvider::resolveArtworkLookupId(QStringLiteral("999"), empty), QStringLiteral("999"));
}

QTEST_MAIN(SteamGridDBProviderTest)
#include "test_steamgriddb_provider.moc"
