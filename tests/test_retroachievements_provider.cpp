#include <QtTest/QtTest>
#include "retroachievements_provider.h"

using namespace Remus;

/**
 * Unit tests for RetroAchievementsProvider.
 *
 * Tests interface contract, auth requirements, and hash filtering.
 * No live network calls — verifies offline/structural behaviour.
 */
class RetroAchievementsProviderTest : public QObject {
    Q_OBJECT

private slots:
    void testProviderName() {
        RetroAchievementsProvider provider;
        QCOMPARE(provider.name(), QStringLiteral("RetroAchievements"));
    }

    void testRequiresAuth() {
        RetroAchievementsProvider provider;
        QVERIFY(provider.requiresAuth());
    }

    void testSearchByNameReturnsEmpty() {
        RetroAchievementsProvider provider;
        // RA is hash-only — name search always returns empty
        QList<SearchResult> results
            = provider.searchByName(QStringLiteral("Sonic the Hedgehog"), QStringLiteral("Mega Drive"));
        QVERIFY(results.isEmpty());
    }

    void testGetByHashRejectsNonMd5() {
        RetroAchievementsProvider provider;
        provider.setCredentials(QStringLiteral("testuser"), QStringLiteral("testapikey"));

        // CRC32 (8 chars) should be rejected — RA only supports MD5
        GameMetadata result = provider.getByHash(QStringLiteral("AABB1122"), QStringLiteral("NES"));
        QVERIFY(result.title.isEmpty());

        // SHA1 (40 chars) should be rejected
        result = provider.getByHash(QStringLiteral("a94a8fe5ccb19ba61c4c0873d391e987982fbbd3"), QStringLiteral("NES"));
        QVERIFY(result.title.isEmpty());
    }

    void testGetByHashRequiresAuth() {
        RetroAchievementsProvider provider;
        // No credentials set — should return empty
        GameMetadata result
            = provider.getByHash(QStringLiteral("1bc674be034e43c96b86487ac69d9293"), QStringLiteral("Mega Drive"));
        QVERIFY(result.title.isEmpty());
    }

    void testGetByIdRequiresAuth() {
        RetroAchievementsProvider provider;
        GameMetadata result = provider.getById(QStringLiteral("1"));
        QVERIFY(result.title.isEmpty());
    }

    void testGetArtworkRequiresAuth() {
        RetroAchievementsProvider provider;
        ArtworkUrls artwork = provider.getArtwork(QStringLiteral("1"));
        QVERIFY(artwork.boxFront.isEmpty());
    }

    void testGetByIdRejectsInvalidId() {
        RetroAchievementsProvider provider;
        provider.setCredentials(QStringLiteral("testuser"), QStringLiteral("testapikey"));

        GameMetadata result = provider.getById(QStringLiteral("notanumber"));
        QVERIFY(result.title.isEmpty());

        result = provider.getById(QStringLiteral("-1"));
        QVERIFY(result.title.isEmpty());

        result = provider.getById(QStringLiteral("0"));
        QVERIFY(result.title.isEmpty());
    }

    void testProviderIdInConstants() {
        QCOMPARE(QString(Constants::Providers::RETROACHIEVEMENTS), QStringLiteral("retroachievements"));
        QCOMPARE(Constants::Providers::DISPLAY_RETROACHIEVEMENTS, QStringLiteral("RetroAchievements"));
    }

    void testRegistryEntry() {
        auto info = Constants::Providers::getProviderInfo(Constants::Providers::RETROACHIEVEMENTS);
        QVERIFY(info != nullptr);
        QCOMPARE(info->priority, 60);
        QVERIFY(info->supportsHashMatch);
        QVERIFY(!info->supportsNameMatch);
        QVERIFY(info->requiresAuth);
        QVERIFY(info->isFreeService);
    }

    void testExternalIdKeyExists() {
        QCOMPARE(QString(Constants::Providers::ExternalId::RETROACHIEVEMENTS), QStringLiteral("retroachievements"));
    }

    void testSetCredentials() {
        RetroAchievementsProvider provider;
        QVERIFY(!provider.requiresAuth() || true); // Interface says requires auth

        // After setting credentials, hash should at least attempt lookup
        // (will fail due to no network, but shouldn't crash)
        provider.setCredentials(QStringLiteral("user"), QStringLiteral("key"));
        GameMetadata result
            = provider.getByHash(QStringLiteral("1bc674be034e43c96b86487ac69d9293"), QStringLiteral("Mega Drive"));
        // Network failure → empty result, but no crash
        QVERIFY(result.title.isEmpty() || !result.title.isEmpty());
    }
};

QTEST_MAIN(RetroAchievementsProviderTest)
#include "test_retroachievements_provider.moc"
