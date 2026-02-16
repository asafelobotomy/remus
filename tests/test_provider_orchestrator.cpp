#include <QtTest>
#include <QSignalSpy>
#include "metadata/provider_orchestrator.h"
#include "core/constants/match_methods.h"

using namespace Remus;

class StubProvider : public MetadataProvider {
    Q_OBJECT
public:
    explicit StubProvider(const QString &id, QObject *parent = nullptr)
        : MetadataProvider(parent), m_id(id) {}

    QString name() const override { return m_id; }
    bool requiresAuth() const override { return false; }

    QList<SearchResult> searchByName(const QString &, const QString &, const QString &) override {
        return m_searchResults;
    }

    GameMetadata getByHash(const QString &, const QString &) override { return m_hashMetadata; }
    GameMetadata getById(const QString &) override { return m_idMetadata; }
    ArtworkUrls getArtwork(const QString &) override { return m_artwork; }

    GameMetadata m_hashMetadata;
    GameMetadata m_idMetadata;
    QList<SearchResult> m_searchResults;
    ArtworkUrls m_artwork;

private:
    QString m_id;
};

class ProviderOrchestratorTest : public QObject {
    Q_OBJECT

private slots:
    void hashProviderPriority();
    void fallsBackToNameSearch();
    void artworkFallback();
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

QTEST_MAIN(ProviderOrchestratorTest)
#include "test_provider_orchestrator.moc"
