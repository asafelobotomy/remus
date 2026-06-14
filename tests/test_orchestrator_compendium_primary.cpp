#include <QtTest>

#include "metadata/provider_orchestrator.h"
#include "../core/constants/match_methods.h"
#include "../core/constants/providers.h"

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

    GameMetadata getByHash(const QString &, const QString &) override {
        ++m_hashCallCount;
        return m_hashMetadata;
    }
    GameMetadata getBySerial(const QString &, const QString &) override {
        ++m_serialCallCount;
        return m_serialMetadata;
    }
    GameMetadata getById(const QString &) override {
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
    int m_hashCallCount = 0;
    int m_serialCallCount = 0;

private:
    QString m_id;
};

class CompendiumPrimaryOrchestratorTest : public QObject {
    Q_OBJECT

private slots:
    void twoPassRouting_CompendiumTitleOnly();
    void identityLock_CompendiumWins();
    void artworkGapFill_RatingMerged();
    void externalIdGapFill_HasheousMerged();
    void compendiumMiss_FallsThrough();
};

void CompendiumPrimaryOrchestratorTest::twoPassRouting_CompendiumTitleOnly() {
    ProviderOrchestrator orchestrator;

    auto *compendium = new StubProvider(QStringLiteral("compendium"));
    compendium->m_hashMetadata.title = QStringLiteral("Canonical Title");
    compendium->m_hashMetadata.matchScore = 1.0f;
    compendium->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;

    auto *localDb = new StubProvider(QStringLiteral("localdatabase"));
    localDb->m_hashMetadata.title = QStringLiteral("Wrong Title");
    localDb->m_hashMetadata.publisher = QStringLiteral("Sega");
    localDb->m_hashMetadata.description = QStringLiteral("A classic platformer.");

    orchestrator.addProvider(QStringLiteral("compendium"), compendium, Constants::Providers::Priority::COMPENDIUM);
    orchestrator.addProvider(QStringLiteral("localdatabase"), localDb, Constants::Providers::Priority::LOCAL_DATABASE);

    const GameMetadata result = orchestrator.searchWithFallback(
        QStringLiteral("AABBCCDD"), QStringLiteral("rom-filename"), QStringLiteral("Genesis"));

    QCOMPARE(result.title, QStringLiteral("Canonical Title"));
    QCOMPARE(result.matchMethod, QString(Constants::MatchMethods::HASH));
    QCOMPARE(result.publisher, QStringLiteral("Sega"));
    QCOMPARE(result.description, QStringLiteral("A classic platformer."));
    QCOMPARE(localDb->m_hashCallCount, 1);
}

void CompendiumPrimaryOrchestratorTest::identityLock_CompendiumWins() {
    ProviderOrchestrator orchestrator;

    auto *compendium = new StubProvider(QStringLiteral("compendium"));
    compendium->m_hashMetadata.title = QStringLiteral("Sonic");
    compendium->m_hashMetadata.system = QStringLiteral("Mega Drive");
    compendium->m_hashMetadata.matchScore = 1.0f;
    compendium->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;

    auto *localDb = new StubProvider(QStringLiteral("localdatabase"));
    localDb->m_hashMetadata.title = QStringLiteral("Sonic the Hedgehog (USA)");
    localDb->m_hashMetadata.publisher = QStringLiteral("Sega");

    orchestrator.addProvider(QStringLiteral("compendium"), compendium, Constants::Providers::Priority::COMPENDIUM);
    orchestrator.addProvider(QStringLiteral("localdatabase"), localDb, Constants::Providers::Priority::LOCAL_DATABASE);

    const GameMetadata result = orchestrator.searchWithFallback(
        QStringLiteral("11223344"), QStringLiteral("Sonic (World)"), QStringLiteral("Genesis"));

    QCOMPARE(result.title, QStringLiteral("Sonic"));
    QCOMPARE(result.system, QStringLiteral("Mega Drive"));
    QCOMPARE(result.publisher, QStringLiteral("Sega"));
}

void CompendiumPrimaryOrchestratorTest::artworkGapFill_RatingMerged() {
    ProviderOrchestrator orchestrator;

    auto *compendium = new StubProvider(QStringLiteral("compendium"));
    compendium->m_hashMetadata.title = QStringLiteral("Streets of Rage");
    compendium->m_hashMetadata.publisher = QStringLiteral("Sega");
    compendium->m_hashMetadata.developer = QStringLiteral("Ancient");
    compendium->m_hashMetadata.releaseDate = QStringLiteral("1991-08-02");
    compendium->m_hashMetadata.genres = { QStringLiteral("Beat 'em up") };
    compendium->m_hashMetadata.players = 2;
    compendium->m_hashMetadata.description = QStringLiteral("Fight through the city.");
    compendium->m_hashMetadata.matchScore = 1.0f;
    compendium->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;

    auto *screenScraper = new StubProvider(QStringLiteral("screenscraper"));
    screenScraper->m_hashMetadata.boxArtUrl = QStringLiteral("http://example.com/sor-front.jpg");
    screenScraper->m_hashMetadata.rating = 4.5f;

    orchestrator.addProvider(QStringLiteral("compendium"), compendium, Constants::Providers::Priority::COMPENDIUM);
    orchestrator.addProvider(
        QStringLiteral("screenscraper"), screenScraper, Constants::Providers::Priority::SCREENSCRAPER);

    const GameMetadata result = orchestrator.searchWithFallback(
        QStringLiteral("DEADBEEF"), QStringLiteral("Streets of Rage"), QStringLiteral("Genesis"));

    QCOMPARE(result.title, QStringLiteral("Streets of Rage"));
    QCOMPARE(result.boxArtUrl, QStringLiteral("http://example.com/sor-front.jpg"));
    QCOMPARE(result.rating, 4.5f);
    QCOMPARE(screenScraper->m_hashCallCount, 1);
}

void CompendiumPrimaryOrchestratorTest::externalIdGapFill_HasheousMerged() {
    ProviderOrchestrator orchestrator;

    auto *compendium = new StubProvider(QStringLiteral("compendium"));
    compendium->m_hashMetadata.title = QStringLiteral("Mega Man X");
    compendium->m_hashMetadata.publisher = QStringLiteral("Capcom");
    compendium->m_hashMetadata.developer = QStringLiteral("Capcom");
    compendium->m_hashMetadata.releaseDate = QStringLiteral("1993-12-17");
    compendium->m_hashMetadata.genres = { QStringLiteral("Platform") };
    compendium->m_hashMetadata.players = 1;
    compendium->m_hashMetadata.description = QStringLiteral("Action platformer.");
    compendium->m_hashMetadata.boxArtUrl = QStringLiteral("http://example.com/mmx-front.jpg");
    compendium->m_hashMetadata.rating = 4.8f;
    compendium->m_hashMetadata.screenshotUrls = { QStringLiteral("http://example.com/mmx-shot.png") };
    compendium->m_hashMetadata.matchScore = 1.0f;
    compendium->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;

    auto *hasheous = new StubProvider(QStringLiteral("hasheous"));
    hasheous->m_hashMetadata.externalIds.insert(QStringLiteral("igdb"), QStringLiteral("98765"));

    orchestrator.addProvider(QStringLiteral("compendium"), compendium, Constants::Providers::Priority::COMPENDIUM);
    orchestrator.addProvider(QStringLiteral("hasheous"), hasheous, Constants::Providers::Priority::HASHEOUS);

    const GameMetadata result = orchestrator.searchWithFallback(
        QStringLiteral("CAFEBABE"), QStringLiteral("Mega Man X"), QStringLiteral("SNES"));

    QCOMPARE(result.title, QStringLiteral("Mega Man X"));
    QCOMPARE(result.externalIds.value(QStringLiteral("igdb")), QStringLiteral("98765"));
    QCOMPARE(hasheous->m_hashCallCount, 1);
}

void CompendiumPrimaryOrchestratorTest::compendiumMiss_FallsThrough() {
    ProviderOrchestrator orchestrator;

    auto *compendium = new StubProvider(QStringLiteral("compendium"));
    auto *localDb = new StubProvider(QStringLiteral("localdatabase"));
    localDb->m_hashMetadata.title = QStringLiteral("Fallback Title");
    localDb->m_hashMetadata.matchScore = 1.0f;
    localDb->m_hashMetadata.matchMethod = Constants::MatchMethods::HASH;

    auto *hasheous = new StubProvider(QStringLiteral("hasheous"));
    hasheous->m_hashMetadata.title = QStringLiteral("Should Not Be Called");

    orchestrator.addProvider(QStringLiteral("compendium"), compendium, Constants::Providers::Priority::COMPENDIUM);
    orchestrator.addProvider(QStringLiteral("localdatabase"), localDb, Constants::Providers::Priority::LOCAL_DATABASE);
    orchestrator.addProvider(QStringLiteral("hasheous"), hasheous, Constants::Providers::Priority::HASHEOUS);

    const GameMetadata result = orchestrator.searchWithFallback(
        QStringLiteral("00112233"), QStringLiteral("Fallback Title"), QStringLiteral("NES"));

    QCOMPARE(result.title, QStringLiteral("Fallback Title"));
    QCOMPARE(localDb->m_hashCallCount, 1);
    QCOMPARE(hasheous->m_hashCallCount, 0);
}

QTEST_MAIN(CompendiumPrimaryOrchestratorTest)
#include "test_orchestrator_compendium_primary.moc"
