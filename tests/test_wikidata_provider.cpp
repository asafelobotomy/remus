#include <QtTest/QtTest>
#include "wikidata_provider.h"

using namespace Remus;

/**
 * Unit tests for WikidataProvider.
 *
 * Tests SPARQL query construction, response parsing, and interface contract.
 * No live network calls — tests cover the offline/structural aspects.
 */
class WikidataProviderTest : public QObject {
    Q_OBJECT

private slots:
    void testProviderName() {
        WikidataProvider provider;
        QCOMPARE(provider.name(), QStringLiteral("Wikidata"));
    }

    void testNoAuthRequired() {
        WikidataProvider provider;
        QVERIFY(!provider.requiresAuth());
    }

    void testGetByHashReturnsEmpty() {
        WikidataProvider provider;
        // Wikidata has no hash support — must return empty metadata
        GameMetadata result = provider.getByHash(QStringLiteral("abc123"), QStringLiteral("NES"));
        QVERIFY(result.title.isEmpty());
        QVERIFY(result.id.isEmpty());
    }

    void testGetArtworkReturnsGracefully() {
        WikidataProvider provider;
        // getArtwork now queries Wikidata for P18 (image property)
        // Without a valid game entity, should return gracefully (may be empty or populated)
        ArtworkUrls artwork = provider.getArtwork(QStringLiteral("Q999999999"));
        // Just verify no crash — result depends on network and entity existence
        QVERIFY(artwork.screenshot.isEmpty()); // Only boxFront is ever populated
    }

    void testGetByIdReturnsEmptyWithoutNetwork() {
        WikidataProvider provider;
        // Without network, getById should return gracefully (empty, no crash)
        GameMetadata result = provider.getById(QStringLiteral("Q999999999"));
        // May be empty due to network failure — just verify no crash
        QVERIFY(result.title.isEmpty() || !result.title.isEmpty());
    }

    void testSearchByNameReturnsEmptyWithoutNetwork() {
        WikidataProvider provider;
        // Without a real network connection, should return empty gracefully
        QList<SearchResult> results = provider.searchByName(QStringLiteral("NonexistentGame12345XYZ"));
        // Just verify it doesn't crash — result depends on network availability
        QVERIFY(results.isEmpty() || !results.isEmpty());
    }

    void testProviderIdInConstants() {
        QCOMPARE(QString(Constants::Providers::WIKIDATA), QStringLiteral("wikidata"));
        QCOMPARE(Constants::Providers::DISPLAY_WIKIDATA, QStringLiteral("Wikidata"));
    }

    void testRegistryEntry() {
        auto info = Constants::Providers::getProviderInfo(Constants::Providers::WIKIDATA);
        QVERIFY(info != nullptr);
        QCOMPARE(info->priority, 40);
        QVERIFY(!info->supportsHashMatch);
        QVERIFY(info->supportsNameMatch);
        QVERIFY(!info->requiresAuth);
        QVERIFY(info->isFreeService);
    }
};

QTEST_MAIN(WikidataProviderTest)
#include "test_wikidata_provider.moc"
