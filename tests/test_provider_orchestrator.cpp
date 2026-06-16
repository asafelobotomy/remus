#include <QtTest>
#include <QSignalSpy>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QtConcurrent>
#include "metadata/provider_orchestrator.h"
#include "metadata/metadata_cache.h"
#include "metadata/hasheous_provider.h"
#include "core/constants/match_methods.h"
#include "core/constants/provider_fields.h"
#include "core/constants/providers.h"

using namespace Remus;

class StubProvider : public MetadataProvider {
    Q_OBJECT
public:
    explicit StubProvider(const QString &id, QObject *parent = nullptr)
        : MetadataProvider(parent)
        , m_id(id) { }

    QString name() const override {
        return m_id;
    }
    bool requiresAuth() const override {
        return false;
    }

    QList<SearchResult> searchByName(const QString &name, const QString &, const QString &) override {
        m_lastSearchName = name;
        return m_searchResults;
    }

    GameMetadata getByHash(const QString &hash, const QString &) override {
        ++m_hashCallCount;
        m_lastHashArg = hash;
        return m_hashMetadata;
    }
    GameMetadata getBySerial(const QString &, const QString &) override {
        ++m_serialCallCount;
        return m_serialMetadata;
    }
    GameMetadata getById(const QString &id) override {
        ++m_getByIdCallCount;
        m_lastGetByIdArg = id;
        return m_idMetadata;
    }
    ArtworkUrls getArtwork(const QString &) override {
        return m_artwork;
    }

    GameMetadata m_hashMetadata;
    GameMetadata m_serialMetadata;
    GameMetadata m_idMetadata;
    QList<SearchResult> m_searchResults;
    ArtworkUrls m_artwork;
    QString m_lastSearchName;
    QString m_lastHashArg;
    QString m_lastGetByIdArg;
    int m_hashCallCount = 0;
    int m_serialCallCount = 0;
    int m_getByIdCallCount = 0;

private:
    QString m_id;
};

// Stub that returns a fake Hasheous response with an IGDB external ID.
class StubHasheousProvider : public HasheousProvider {
    Q_OBJECT
public:
    explicit StubHasheousProvider(QObject *parent = nullptr)
        : HasheousProvider(parent) { }

protected:
    QJsonObject makePostRequest(const QString &, const QJsonObject &, const QUrlQuery & = QUrlQuery()) override {
        QJsonObject response;
        response["id"] = 42;
        response["name"] = QStringLiteral("Test Game");
        QJsonArray metadata;
        QJsonObject igdbEntry;
        igdbEntry["source"] = QStringLiteral("IGDB");
        igdbEntry["immutableId"] = QStringLiteral("12345");
        metadata.append(igdbEntry);
        response["metadata"] = metadata;
        return response;
    }
};

static QSqlDatabase createTestCacheDb() {
    const QString connectionName = QStringLiteral("orch-cache-%1").arg(QDateTime::currentMSecsSinceEpoch());
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
    void hashMatchSkipsRemainingProviders();
    void hashMatchWithRequireArtworkContinuesForArtwork();

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

    // M1 — Hasheous IGDB proxy-disabled skip count
    void testIgdbSkippedCountWhenProxyDisabled();
    void testGetProviderReturnsCorrectType();

    // P2 — Concurrent match correctness
    void testConcurrentMatchResultsNoDuplicates();

    // P3 — Concurrent field-gap computation correctness
    void testComputeFieldGapConcurrentlyConsistent();

    // Cascade: hash miss → serial → name
    void serialCascadeWhenHashMisses();
    void nameCascadeWhenHashAndSerialMiss();

    void retroAchievementsUsesRaMd5NotNoIntroMd5();
    void retroAchievementsUsesExternalIdBeforeRaHash();
};

void ProviderOrchestratorTest::hashProviderPriority() {
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

void ProviderOrchestratorTest::fallsBackToNameSearch() {
    ProviderOrchestrator orchestrator;

    auto *hashProvider = new StubProvider("screenscraper");
    // Return empty to force name fallback

    auto *nameProvider = new StubProvider("igdb");
    SearchResult result;
    result.id = "42";
    result.title = "Name Hit";
    result.matchScore = 0.8f;
    nameProvider->m_searchResults = { result };

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

void ProviderOrchestratorTest::detailFetchFailureEmitsSpecificProviderError() {
    ProviderOrchestrator orchestrator;

    auto *first = new StubProvider("igdb");
    SearchResult failedDetailResult;
    failedDetailResult.id = "42";
    failedDetailResult.title = "Name Hit";
    failedDetailResult.matchScore = 0.8f;
    first->m_searchResults = { failedDetailResult };

    auto *second = new StubProvider("thegamesdb");
    SearchResult successResult;
    successResult.id = "84";
    successResult.title = "Backup Hit";
    successResult.matchScore = 0.75f;
    second->m_searchResults = { successResult };

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

void ProviderOrchestratorTest::normalizesVersionedNamesBeforeNameSearch() {
    ProviderOrchestrator orchestrator;

    auto *nameProvider = new StubProvider("igdb");
    SearchResult result;
    result.id = "dq3";
    result.title = "Dragon Quest III";
    result.matchScore = 0.99f;
    nameProvider->m_searchResults = { result };

    GameMetadata metadata;
    metadata.id = "dq3";
    metadata.title = "Dragon Quest III";
    nameProvider->m_idMetadata = metadata;

    orchestrator.addProvider("igdb", nameProvider, 40);

    GameMetadata found = orchestrator.searchWithFallback("", "Dragon Quest III (English v2.0)[Addendum]", "SNES");

    QCOMPARE(nameProvider->m_lastSearchName, QString("Dragon Quest III"));
    QCOMPARE(found.title, QString("Dragon Quest III"));
    QCOMPARE(found.matchMethod, Constants::MatchMethods::NAME);
}

void ProviderOrchestratorTest::artworkFallback() {
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

void ProviderOrchestratorTest::searchWithFallbackContinuesPastCacheWithoutArtwork() {
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
    cached.genres = { "RPG" };
    cached.players = 1;
    cache.store(cached, "6291ee08", "SNES");

    auto *provider = new StubProvider("igdb");
    SearchResult result;
    result.id = "igdb-live-a-live";
    result.title = "Live A Live";
    result.matchScore = 0.99f;
    provider->m_searchResults = { result };

    GameMetadata enriched = cached;
    enriched.boxArtUrl = "http://example.com/live-a-live-front.jpg";
    provider->m_idMetadata = enriched;

    orchestrator.addProvider("igdb", provider, 40);

    GameMetadata found = orchestrator.searchWithFallback(
        "6291ee08", "Live A Live", "SNES", QString(), QString(), QString(), QString(), 0, QString(), true);

    QCOMPARE(provider->m_lastSearchName, QString("Live A Live"));
    QCOMPARE(found.title, QString("Live A Live"));
    QCOMPARE(found.boxArtUrl, QString("http://example.com/live-a-live-front.jpg"));

    GameMetadata fromCache = cache.getByHash("6291ee08", "SNES");
    QCOMPARE(fromCache.boxArtUrl, QString("http://example.com/live-a-live-front.jpg"));
}

// ── Phase 0 characterization tests ─────────────────────────────────────────

void ProviderOrchestratorTest::testRemoveProvider() {
    ProviderOrchestrator orchestrator;

    auto *provider = new StubProvider("screenscraper");
    orchestrator.addProvider("screenscraper", provider, 90);
    QVERIFY(orchestrator.getEnabledProviders().contains("screenscraper"));

    orchestrator.removeProvider("screenscraper");
    QVERIFY(!orchestrator.getEnabledProviders().contains("screenscraper"));
}

void ProviderOrchestratorTest::testGetEnabledProviders() {
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

void ProviderOrchestratorTest::testSetProviderEnabled() {
    ProviderOrchestrator orchestrator;

    auto *provider = new StubProvider("igdb");
    orchestrator.addProvider("igdb", provider, 40);
    QVERIFY(orchestrator.getEnabledProviders().contains("igdb"));

    orchestrator.setProviderEnabled("igdb", false);
    QVERIFY(!orchestrator.getEnabledProviders().contains("igdb"));

    orchestrator.setProviderEnabled("igdb", true);
    QVERIFY(orchestrator.getEnabledProviders().contains("igdb"));
}

void ProviderOrchestratorTest::testAllProvidersFailed() {
    ProviderOrchestrator orchestrator;

    // Add providers that return empty results (will fail)
    orchestrator.addProvider("empty1", new StubProvider("empty1"), 10);
    orchestrator.addProvider("empty2", new StubProvider("empty2"), 5);

    QSignalSpy failedSpy(&orchestrator, &ProviderOrchestrator::allProvidersFailed);

    GameMetadata result = orchestrator.searchWithFallback("badhash", "Unknown Game", "NES");
    QVERIFY(result.title.isEmpty());
    QVERIFY(failedSpy.count() >= 1);
}

void ProviderOrchestratorTest::testSearchAllProviders() {
    ProviderOrchestrator orchestrator;

    auto *p1 = new StubProvider("provider1");
    SearchResult r1;
    r1.id = "1";
    r1.title = "Result A";
    r1.matchScore = 0.9f;
    p1->m_searchResults = { r1 };

    auto *p2 = new StubProvider("provider2");
    SearchResult r2;
    r2.id = "2";
    r2.title = "Result B";
    r2.matchScore = 0.7f;
    p2->m_searchResults = { r2 };

    orchestrator.addProvider("provider1", p1, 10);
    orchestrator.addProvider("provider2", p2, 5);

    QList<SearchResult> all = orchestrator.searchAllProviders("Test Game", "NES");
    QCOMPARE(all.size(), 2);
}

// ── Cache integration tests ────────────────────────────────────────────────

void ProviderOrchestratorTest::cacheHitSkipsProviders() {
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

void ProviderOrchestratorTest::cacheMissStoresResult() {
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

void ProviderOrchestratorTest::artworkCacheHitSkipsProviders() {
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

void ProviderOrchestratorTest::hashMatchSkipsRemainingProviders() {
    // A hash match from the first (local) provider must prevent any further
    // provider from being queried — the ROM identity is resolved at 100%.
    ProviderOrchestrator orchestrator;

    auto *first = new StubProvider("compendium");
    first->m_hashMetadata.title = "Super Mario World";
    first->m_hashMetadata.publisher = "Nintendo";
    first->m_hashMetadata.developer = "Nintendo";
    first->m_hashMetadata.releaseDate = "1990-11-21";
    first->m_hashMetadata.genres = { "Platform" };
    first->m_hashMetadata.players = 2;
    first->m_hashMetadata.description = "Classic platformer.";
    first->m_hashMetadata.boxArtUrl = "http://example.com/smw-front.jpg";
    first->m_hashMetadata.rating = 4.9f;
    first->m_hashMetadata.screenshotUrls = { "http://example.com/smw-shot.png" };
    first->m_hashMetadata.externalIds.insert("igdb", "1234");
    first->m_hashMetadata.matchScore = 1.0f;
    first->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;

    auto *second = new StubProvider("hasheous");
    second->m_hashMetadata.title = "Should Not Be Called";

    orchestrator.addProvider("compendium", first, 210);
    orchestrator.addProvider("hasheous", second, 80);

    QSignalSpy trySpy(&orchestrator, &ProviderOrchestrator::tryingProvider);

    const GameMetadata result = orchestrator.searchWithFallback("AABBCCDD", "Super Mario World", "SNES");

    QCOMPARE(result.title, QStringLiteral("Super Mario World"));

    // The second provider should never have been queried.
    QCOMPARE(second->m_hashCallCount, 0);

    // Only the first provider should have been tried.
    bool sawSecond = false;
    for (int i = 0; i < trySpy.count(); ++i) {
        if (trySpy.at(i).at(0).toString() == QLatin1String("hasheous"))
            sawSecond = true;
    }
    QVERIFY(!sawSecond);
}

void ProviderOrchestratorTest::hashMatchWithRequireArtworkContinuesForArtwork() {
    // With requireArtwork=true, if the hash-matched provider has no artwork,
    // the orchestrator should continue to find artwork — but the title from
    // the hash match must be preserved (not overwritten).
    ProviderOrchestrator orchestrator;

    auto *first = new StubProvider("compendium");
    first->m_hashMetadata.title = "Chrono Trigger";
    first->m_hashMetadata.matchScore = 1.0f;
    first->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;
    // No boxArtUrl set — artwork is missing.

    auto *second = new StubProvider("thegamesdb");
    SearchResult sr;
    sr.id = "ct-42";
    sr.title = "Chrono Trigger";
    sr.matchScore = 0.99f;
    second->m_searchResults = { sr };
    GameMetadata enriched;
    enriched.title = "Chrono Trigger";
    enriched.boxArtUrl = "http://example.com/ct-front.jpg";
    second->m_idMetadata = enriched;

    orchestrator.addProvider("compendium", first, 210);
    orchestrator.addProvider("thegamesdb", second, 50);

    const GameMetadata result = orchestrator.searchWithFallback(
        "CCDDEE11", "Chrono Trigger", "SNES", QString(), QString(), QString(), QString(), 0, QString(), true);

    // Title must come from the hash match.
    QCOMPARE(result.title, QStringLiteral("Chrono Trigger"));
    // Artwork should have been fetched from the second provider.
    QVERIFY(!result.boxArtUrl.isEmpty());
}

void ProviderOrchestratorTest::testIgdbSkippedCountWhenProxyDisabled() {
    // No API key → metadataProxyEnabled() returns false → IGDB enrichment skipped
    StubHasheousProvider provider;
    QCOMPARE(provider.igdbSkippedCount(), 0);

    // Each call with an IGDB-identified result should increment the counter
    provider.getByHashes(QStringLiteral("aabbcc00"), QString(), QString(), QString());
    QCOMPARE(provider.igdbSkippedCount(), 1);

    provider.getByHashes(QStringLiteral("ddeeff11"), QString(), QString(), QString());
    QCOMPARE(provider.igdbSkippedCount(), 2);
}

void ProviderOrchestratorTest::testGetProviderReturnsCorrectType() {
    ProviderOrchestrator orchestrator;
    auto *hasheous = new StubHasheousProvider();
    orchestrator.addProvider("hasheous", hasheous, 80);

    MetadataProvider *raw = orchestrator.getProvider("hasheous");
    QVERIFY(raw != nullptr);
    QVERIFY(dynamic_cast<HasheousProvider *>(raw) != nullptr);
    QVERIFY(orchestrator.getProvider("nonexistent") == nullptr);
}

void ProviderOrchestratorTest::testConcurrentMatchResultsNoDuplicates() {
    // P2 acceptance: collecting results from N concurrent match tasks into a
    // shared list must yield exactly N distinct entries — no duplicates.
    //
    // Each worker gets its own ProviderOrchestrator + StubProvider so that no
    // QObject state (including QNetworkAccessManager) is shared across threads.
    // This is the safe concurrency model for HTTP providers documented in
    // HttpMetadataProvider's thread-safety audit comment.

    constexpr int N = 8;
    QList<GameMetadata> collectedResults;
    QMutex resultsMutex;

    QList<int> taskIds;
    taskIds.reserve(N);
    for (int i = 0; i < N; ++i)
        taskIds.append(i);

    QtConcurrent::blockingMap(taskIds, [&](int id) {
        // Worker-local orchestrator + stub — no shared QObject state.
        // Use name-search path (hash lookup is gated on known provider names).
        ProviderOrchestrator localOrch;
        const QString providerName = QStringLiteral("stub-%1").arg(id);
        auto *stub = new StubProvider(providerName);
        const QString title = QStringLiteral("Title-%1").arg(id);
        stub->m_searchResults = { SearchResult { QStringLiteral("id-%1").arg(id), title, { }, { }, 0, 1.0f } };
        stub->m_idMetadata.title = title;
        stub->m_idMetadata.matchScore = 1.0f;
        localOrch.addProvider(providerName, stub, 100);

        GameMetadata result = localOrch.searchWithFallback(
            QStringLiteral("hash-%1").arg(id), QStringLiteral("Game %1").arg(id), QStringLiteral("SNES"));

        QMutexLocker lock(&resultsMutex);
        collectedResults.append(result);
    });

    QCOMPARE(collectedResults.size(), N);

    // All titles must be distinct — no duplicate results written.
    QSet<QString> titles;
    for (const GameMetadata &m : collectedResults)
        titles.insert(m.title);
    QCOMPARE(titles.size(), N);
}

void ProviderOrchestratorTest::testComputeFieldGapConcurrentlyConsistent() {
    // P3 acceptance: computing the field gap for the same metadata stub from
    // N concurrent threads must always yield an identical result — no data
    // races in computeFieldGap (which is a pure const static-style function).

    GameMetadata partial;
    partial.title = QStringLiteral("Sonic");
    partial.publisher = QStringLiteral("Sega");
    // developer, releaseDate, genres, players, description are all empty

    const ProviderOrchestrator::FieldSet expected = ProviderOrchestrator::computeFieldGap(partial);
    QVERIFY(expected.contains(QStringLiteral("developer")));
    QVERIFY(!expected.contains(QStringLiteral("publisher")));

    constexpr int N = 32;
    QList<ProviderOrchestrator::FieldSet> results;
    results.resize(N);
    QMutex mu;

    QList<int> ids;
    ids.reserve(N);
    for (int i = 0; i < N; ++i)
        ids.append(i);

    QtConcurrent::blockingMap(ids, [&](int i) {
        // Each worker computes the gap independently — no shared mutable state.
        const ProviderOrchestrator::FieldSet gap = ProviderOrchestrator::computeFieldGap(partial);
        QMutexLocker lock(&mu);
        results[i] = gap;
    });

    for (int i = 0; i < N; ++i) {
        QCOMPARE(results[i], expected);
    }
}

// Cascade: hash miss → serial → name

void ProviderOrchestratorTest::serialCascadeWhenHashMisses() {
    // Arrange: provider returns empty on hash, a result on serial.
    ProviderOrchestrator orchestrator;

    auto *provider = new StubProvider("compendium");
    // hash returns nothing
    GameMetadata serialHit;
    serialHit.id = "wup-a-baae";
    serialHit.title = "Mario Kart 8";
    serialHit.matchScore = 0.9f;
    serialHit.matchMethod = QStringLiteral("serial");
    provider->m_serialMetadata = serialHit;

    orchestrator.addProvider("compendium", provider, 100);

    // Act: supply a serial; hash is empty so only serial/name paths run.
    GameMetadata result = orchestrator.searchWithFallback(
        QString(), QString(), QStringLiteral("Wii U"), QString(), QString(), QString(), QStringLiteral("WUP-A-BAAE"));

    // Assert: matched via serial, hash never called, serial called once.
    QCOMPARE(result.title, QStringLiteral("Mario Kart 8"));
    QCOMPARE(result.matchMethod, QStringLiteral("serial"));
    QCOMPARE(provider->m_hashCallCount, 0);
    QCOMPARE(provider->m_serialCallCount, 1);
}

void ProviderOrchestratorTest::nameCascadeWhenHashAndSerialMiss() {
    // Arrange: provider returns empty on both hash and serial, a result on name.
    ProviderOrchestrator orchestrator;

    auto *provider = new StubProvider("compendium");
    // hash and serial return nothing; name search succeeds.
    SearchResult sr;
    sr.id = "wup-a-baae";
    sr.title = "Mario Kart 8";
    sr.matchScore = 0.99f;
    provider->m_searchResults = { sr };
    GameMetadata idResult;
    idResult.id = "wup-a-baae";
    idResult.title = "Mario Kart 8";
    provider->m_idMetadata = idResult;

    orchestrator.addProvider("compendium", provider, 100);

    // Act: supply name but no hash or serial.
    GameMetadata result
        = orchestrator.searchWithFallback(QString(), QStringLiteral("Mario Kart 8"), QStringLiteral("Wii U"));

    // Assert: matched via name, hash and serial never called.
    QCOMPARE(result.title, QStringLiteral("Mario Kart 8"));
    QCOMPARE(provider->m_hashCallCount, 0);
    QCOMPARE(provider->m_serialCallCount, 0);
    QVERIFY(!result.matchMethod.isEmpty());
}

void ProviderOrchestratorTest::retroAchievementsUsesRaMd5NotNoIntroMd5() {
    ProviderOrchestrator orchestrator;
    auto *ra = new StubProvider(QStringLiteral("retroachievements"));
    ra->m_hashMetadata.title = QStringLiteral("RA Match");
    orchestrator.addProvider(
        QStringLiteral("retroachievements"), ra, Constants::Providers::Priority::RETROACHIEVEMENTS);

    const GameMetadata result = orchestrator.getHashFromProvider(QStringLiteral("retroachievements"),
        QStringLiteral("abc"), QStringLiteral("NES"), QStringLiteral("crc"), QStringLiteral("nointro-md5"),
        QStringLiteral("sha1"), QStringLiteral("ra-md5-hash"));

    QCOMPARE(result.title, QStringLiteral("RA Match"));
    QCOMPARE(ra->m_lastHashArg, QStringLiteral("ra-md5-hash"));
}

void ProviderOrchestratorTest::retroAchievementsUsesExternalIdBeforeRaHash() {
    ProviderOrchestrator orchestrator;
    auto *ra = new StubProvider(QStringLiteral("retroachievements"));
    ra->m_idMetadata.title = QStringLiteral("RA By ID");
    ra->m_hashMetadata.title = QStringLiteral("RA By Hash");
    orchestrator.addProvider(
        QStringLiteral("retroachievements"), ra, Constants::Providers::Priority::RETROACHIEVEMENTS);

    GameMetadata existing;
    existing.title = QStringLiteral("Canonical");
    existing.externalIds[Constants::Providers::ExternalId::RETROACHIEVEMENTS] = QStringLiteral("12345");

    ProviderOrchestrator::FieldSet gap;
    gap.insert(Constants::ProviderFields::BOX_ART_URL);

    const GameMetadata out = orchestrator.enrichMissingFields(gap, existing, QStringLiteral("hash"),
        QStringLiteral("Canonical"), QStringLiteral("NES"), QString(), QStringLiteral("nointro-md5"), QString(),
        QString(), { }, QStringLiteral("ra-md5-hash"));

    QCOMPARE(ra->m_getByIdCallCount, 1);
    QCOMPARE(ra->m_lastGetByIdArg, QStringLiteral("12345"));
    QCOMPARE(ra->m_hashCallCount, 0);
    QCOMPARE(out.title, QStringLiteral("Canonical"));
}

QTEST_MAIN(ProviderOrchestratorTest)
#include "test_provider_orchestrator.moc"
