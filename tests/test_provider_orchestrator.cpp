#include <QtTest>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "metadata/provider_orchestrator.h"
#include "metadata/metadata_cache.h"
#include "core/constants/match_methods.h"

using namespace Remus;

class StubProvider : public MetadataProvider {
    Q_OBJECT
public:
    explicit StubProvider(const QString &id, QObject *parent = nullptr)
        : MetadataProvider(parent), m_id(id) {}

    QString name() const override { return m_id; }
    bool requiresAuth() const override { return false; }

    QList<SearchResult> searchByName(const QString &name, const QString &, const QString &) override {
        m_lastSearchName = name;
        return m_searchResults;
    }

    GameMetadata getByHash(const QString &, const QString &) override { return m_hashMetadata; }
    GameMetadata getById(const QString &) override { return m_idMetadata; }
    ArtworkUrls getArtwork(const QString &) override { return m_artwork; }

    GameMetadata m_hashMetadata;
    GameMetadata m_idMetadata;
    QList<SearchResult> m_searchResults;
    ArtworkUrls m_artwork;
    QString m_lastSearchName;

private:
    QString m_id;
};

static QSqlDatabase createTestCacheDb()
{
    const QString connectionName = QStringLiteral("orch-cache-%1")
        .arg(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(":memory:");
    if (!db.open()) {
        qFatal("Failed to open test cache db");
    }
    QSqlQuery query(db);
    query.exec("CREATE TABLE cache (cache_key TEXT PRIMARY KEY, "
                "cache_value BLOB, expiry TEXT, "
                "created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    return db;
}

class ProviderOrchestratorTest : public QObject {
    Q_OBJECT

private slots:
    void hashProviderPriority();
    void fallsBackToNameSearch();
    void detailFetchFailureEmitsSpecificProviderError();
    void normalizesVersionedNamesBeforeNameSearch();
    void artworkFallback();
    void searchWithFallbackContinuesPastCacheWithoutArtwork();

    // Phase 0 characterization tests — safety net for Phase 5
    void testRemoveProvider();
    void testGetEnabledProviders();
    void testSetProviderEnabled();
    void testAllProvidersFailed();
    void testSearchAllProviders();

    // Cache integration tests
    void cacheHitSkipsProviders();
    void cacheMissStoresResult();
    void artworkCacheHitSkipsProviders();
};

void ProviderOrchestratorTest::hashProviderPriority()
{
    ProviderOrchestrator orchestrator;

    auto *hashProvider = new StubProvider("screenscraper");
    hashProvider->m_hashMetadata.title = "Hash Hit";

    auto *nameProvider = new StubProvider("thegamesdb");

    orchestrator.addProvider("screenscraper", hashProvider, 90);
    orchestrator.addProvider("thegamesdb", nameProvider, 50);

    QSignalSpy trySpy(&orchestrator, &ProviderOrchestrator::tryingProvider);
    QSignalSpy successSpy(&orchestrator, &ProviderOrchestrator::providerSucceeded);

    GameMetadata result = orchestrator.getByHashWithFallback("abcd", "Genesis");

    QCOMPARE(result.title, QString("Hash Hit"));
    QVERIFY(orchestrator.providerSupportsHash("screenscraper"));
    QVERIFY(!orchestrator.providerSupportsHash("thegamesdb"));
    QVERIFY(trySpy.count() >= 1);
    QVERIFY(successSpy.count() == 1);
}

void ProviderOrchestratorTest::fallsBackToNameSearch()
{
    ProviderOrchestrator orchestrator;

    auto *hashProvider = new StubProvider("screenscraper");
    // Return empty to force name fallback

    auto *nameProvider = new StubProvider("igdb");
    SearchResult result;
    result.id = "42";
    result.title = "Name Hit";
    result.matchScore = 0.8f;
    nameProvider->m_searchResults = {result};

    GameMetadata metadata;
    metadata.id = "42";
    metadata.title = "Full Metadata";
    nameProvider->m_idMetadata = metadata;

    orchestrator.addProvider("screenscraper", hashProvider, 90);
    orchestrator.addProvider("igdb", nameProvider, 40);

    GameMetadata found = orchestrator.searchWithFallback("", "Some Game", "NES");

    QCOMPARE(found.title, QString("Full Metadata"));
    QVERIFY(found.matchScore > 0.0f);
    QCOMPARE(found.matchMethod, Constants::MatchMethods::FUZZY);
}

void ProviderOrchestratorTest::detailFetchFailureEmitsSpecificProviderError()
{
    ProviderOrchestrator orchestrator;

    auto *first = new StubProvider("igdb");
    SearchResult failedDetailResult;
    failedDetailResult.id = "42";
    failedDetailResult.title = "Name Hit";
    failedDetailResult.matchScore = 0.8f;
    first->m_searchResults = {failedDetailResult};

    auto *second = new StubProvider("thegamesdb");
    SearchResult successResult;
    successResult.id = "84";
    successResult.title = "Backup Hit";
    successResult.matchScore = 0.75f;
    second->m_searchResults = {successResult};

    GameMetadata metadata;
    metadata.id = "84";
    metadata.title = "Backup Metadata";
    second->m_idMetadata = metadata;

    orchestrator.addProvider("igdb", first, 40);
    orchestrator.addProvider("thegamesdb", second, 30);

    QSignalSpy failedSpy(&orchestrator, &ProviderOrchestrator::providerFailed);

    GameMetadata found = orchestrator.searchWithFallback("", "Some Game", "NES");

    QCOMPARE(found.title, QString("Backup Metadata"));
    QCOMPARE(failedSpy.count(), 1);

    const QList<QVariant> firstFailure = failedSpy.at(0);
    QCOMPARE(firstFailure.at(0).toString(), QString("igdb"));
    QVERIFY(firstFailure.at(1).toString().contains(QStringLiteral("Detail fetch failed after search hit")));
    QVERIFY(firstFailure.at(1).toString().contains(QStringLiteral("Name Hit")));
}

void ProviderOrchestratorTest::normalizesVersionedNamesBeforeNameSearch()
{
    ProviderOrchestrator orchestrator;

    auto *nameProvider = new StubProvider("igdb");
    SearchResult result;
    result.id = "dq3";
    result.title = "Dragon Quest III";
    result.matchScore = 0.99f;
    nameProvider->m_searchResults = {result};

    GameMetadata metadata;
    metadata.id = "dq3";
    metadata.title = "Dragon Quest III";
    nameProvider->m_idMetadata = metadata;

    orchestrator.addProvider("igdb", nameProvider, 40);

    GameMetadata found = orchestrator.searchWithFallback(
        "",
        "Dragon Quest III (English v2.0)[Addendum]",
        "SNES"
    );

    QCOMPARE(nameProvider->m_lastSearchName, QString("Dragon Quest III"));
    QCOMPARE(found.title, QString("Dragon Quest III"));
    QCOMPARE(found.matchMethod, Constants::MatchMethods::NAME);
}

void ProviderOrchestratorTest::artworkFallback()
{
    ProviderOrchestrator orchestrator;

    auto *first = new StubProvider("igdb");
    auto *second = new StubProvider("thegamesdb");

    ArtworkUrls artwork;
    artwork.boxFront = QUrl("http://example/front.png");
    second->m_artwork = artwork;

    orchestrator.addProvider("igdb", first, 10);
    orchestrator.addProvider("thegamesdb", second, 5);

    ArtworkUrls loaded = orchestrator.getArtworkWithFallback("id-1", "NES", QString());
    QCOMPARE(loaded.boxFront, artwork.boxFront);
}

void ProviderOrchestratorTest::searchWithFallbackContinuesPastCacheWithoutArtwork()
{
    QSqlDatabase db = createTestCacheDb();
    MetadataCache cache(db);
    ProviderOrchestrator orchestrator;
    orchestrator.setCache(&cache);

    GameMetadata cached;
    cached.id = "live-a-live";
    cached.title = "Live A Live";
    cached.publisher = "Square";
    cached.developer = "Square";
    cached.releaseDate = "1994-09-02";
    cached.genres = {"RPG"};
    cached.players = 1;
    cache.store(cached, "6291ee08", "SNES");

    auto *provider = new StubProvider("igdb");
    SearchResult result;
    result.id = "igdb-live-a-live";
    result.title = "Live A Live";
    result.matchScore = 0.99f;
    provider->m_searchResults = {result};

    GameMetadata enriched = cached;
    enriched.boxArtUrl = "http://example.com/live-a-live-front.jpg";
    provider->m_idMetadata = enriched;

    orchestrator.addProvider("igdb", provider, 40);

    GameMetadata found = orchestrator.searchWithFallback(
        "6291ee08", "Live A Live", "SNES", QString(), QString(), QString(), QString(), true);

    QCOMPARE(provider->m_lastSearchName, QString("Live A Live"));
    QCOMPARE(found.title, QString("Live A Live"));
    QCOMPARE(found.boxArtUrl, QString("http://example.com/live-a-live-front.jpg"));

    GameMetadata fromCache = cache.getByHash("6291ee08", "SNES");
    QCOMPARE(fromCache.boxArtUrl, QString("http://example.com/live-a-live-front.jpg"));
}

// ── Phase 0 characterization tests ─────────────────────────────────────────

void ProviderOrchestratorTest::testRemoveProvider()
{
    ProviderOrchestrator orchestrator;

    auto *provider = new StubProvider("screenscraper");
    orchestrator.addProvider("screenscraper", provider, 90);
    QVERIFY(orchestrator.getEnabledProviders().contains("screenscraper"));

    orchestrator.removeProvider("screenscraper");
    QVERIFY(!orchestrator.getEnabledProviders().contains("screenscraper"));
}

void ProviderOrchestratorTest::testGetEnabledProviders()
{
    ProviderOrchestrator orchestrator;

    QVERIFY(orchestrator.getEnabledProviders().isEmpty());

    orchestrator.addProvider("a", new StubProvider("a"), 10);
    orchestrator.addProvider("b", new StubProvider("b"), 20);
    orchestrator.addProvider("c", new StubProvider("c"), 5);

    QStringList enabled = orchestrator.getEnabledProviders();
    QCOMPARE(enabled.size(), 3);
    QVERIFY(enabled.contains("a"));
    QVERIFY(enabled.contains("b"));
    QVERIFY(enabled.contains("c"));
}

void ProviderOrchestratorTest::testSetProviderEnabled()
{
    ProviderOrchestrator orchestrator;

    auto *provider = new StubProvider("igdb");
    orchestrator.addProvider("igdb", provider, 40);
    QVERIFY(orchestrator.getEnabledProviders().contains("igdb"));

    orchestrator.setProviderEnabled("igdb", false);
    QVERIFY(!orchestrator.getEnabledProviders().contains("igdb"));

    orchestrator.setProviderEnabled("igdb", true);
    QVERIFY(orchestrator.getEnabledProviders().contains("igdb"));
}

void ProviderOrchestratorTest::testAllProvidersFailed()
{
    ProviderOrchestrator orchestrator;

    // Add providers that return empty results (will fail)
    orchestrator.addProvider("empty1", new StubProvider("empty1"), 10);
    orchestrator.addProvider("empty2", new StubProvider("empty2"), 5);

    QSignalSpy failedSpy(&orchestrator, &ProviderOrchestrator::allProvidersFailed);

    GameMetadata result = orchestrator.searchWithFallback("badhash", "Unknown Game", "NES");
    QVERIFY(result.title.isEmpty());
    QVERIFY(failedSpy.count() >= 1);
}

void ProviderOrchestratorTest::testSearchAllProviders()
{
    ProviderOrchestrator orchestrator;

    auto *p1 = new StubProvider("provider1");
    SearchResult r1;
    r1.id = "1";
    r1.title = "Result A";
    r1.matchScore = 0.9f;
    p1->m_searchResults = {r1};

    auto *p2 = new StubProvider("provider2");
    SearchResult r2;
    r2.id = "2";
    r2.title = "Result B";
    r2.matchScore = 0.7f;
    p2->m_searchResults = {r2};

    orchestrator.addProvider("provider1", p1, 10);
    orchestrator.addProvider("provider2", p2, 5);

    QList<SearchResult> all = orchestrator.searchAllProviders("Test Game", "NES");
    QCOMPARE(all.size(), 2);
}

// ── Cache integration tests ────────────────────────────────────────────────

void ProviderOrchestratorTest::cacheHitSkipsProviders()
{
    QSqlDatabase db = createTestCacheDb();
    MetadataCache cache(db);
    ProviderOrchestrator orchestrator;
    orchestrator.setCache(&cache);

    // Pre-populate cache
    GameMetadata cached;
    cached.id = "99";
    cached.title = "Cached Game";
    cached.providerId = "screenscraper";
    cache.store(cached, "abc123", "NES");

    // Provider should NOT be called — hash provider returns non-empty but
    // the cache should short-circuit before it's tried.
    auto *hashProvider = new StubProvider("screenscraper");
    hashProvider->m_hashMetadata.title = "API Result";
    orchestrator.addProvider("screenscraper", hashProvider, 90);

    GameMetadata result = orchestrator.getByHashWithFallback("abc123", "NES");
    QCOMPARE(result.title, QString("Cached Game"));
}

void ProviderOrchestratorTest::cacheMissStoresResult()
{
    QSqlDatabase db = createTestCacheDb();
    MetadataCache cache(db);
    ProviderOrchestrator orchestrator;
    orchestrator.setCache(&cache);

    auto *hashProvider = new StubProvider("screenscraper");
    hashProvider->m_hashMetadata.title = "Fresh Result";
    hashProvider->m_hashMetadata.id = "77";
    hashProvider->m_hashMetadata.providerId = "screenscraper";
    orchestrator.addProvider("screenscraper", hashProvider, 90);

    // First call — cache miss, provider supplies the result
    GameMetadata result = orchestrator.getByHashWithFallback("def456", "SNES");
    QCOMPARE(result.title, QString("Fresh Result"));

    // Verify it was stored in cache
    GameMetadata fromCache = cache.getByHash("def456", "SNES");
    QCOMPARE(fromCache.title, QString("Fresh Result"));
}

void ProviderOrchestratorTest::artworkCacheHitSkipsProviders()
{
    QSqlDatabase db = createTestCacheDb();
    MetadataCache cache(db);
    ProviderOrchestrator orchestrator;
    orchestrator.setCache(&cache);

    // Pre-populate artwork cache
    ArtworkUrls cachedArt;
    cachedArt.boxFront = QUrl("http://example/cached-front.png");
    cache.storeArtwork("art-1", cachedArt);

    auto *provider = new StubProvider("igdb");
    ArtworkUrls apiArt;
    apiArt.boxFront = QUrl("http://example/api-front.png");
    provider->m_artwork = apiArt;
    orchestrator.addProvider("igdb", provider, 40);

    ArtworkUrls result = orchestrator.getArtworkWithFallback("art-1", "NES", QString());
    QCOMPARE(result.boxFront, QUrl("http://example/cached-front.png"));
}

QTEST_MAIN(ProviderOrchestratorTest)
#include "test_provider_orchestrator.moc"
