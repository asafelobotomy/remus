#include <QtTest/QtTest>

#include "../src/core/constants/match_methods.h"
#include "../src/core/database.h"
#include "../src/metadata/metadata_cache.h"

using namespace Remus;
using namespace Remus::Constants;

class CacheDeserializationTest : public QObject {
    Q_OBJECT

private slots:
    void roundTripPreservesMetadataFields();
};

void CacheDeserializationTest::roundTripPreservesMetadataFields() {
    Database db;
    QVERIFY(db.initialize(QStringLiteral(":memory:")));

    MetadataCache cache(db.database());

    GameMetadata original;
    original.id = QStringLiteral("12345");
    original.title = QStringLiteral("Super Mario Bros.");
    original.system = QStringLiteral("NES");
    original.region = QStringLiteral("USA");
    original.publisher = QStringLiteral("Nintendo");
    original.developer = QStringLiteral("Nintendo R&D4");
    original.genres << QStringLiteral("Platform") << QStringLiteral("Action");
    original.releaseDate = QStringLiteral("1985-09-13");
    original.description = QStringLiteral("Classic platformer");
    original.players = 2;
    original.rating = 9.5f;
    original.providerId = QStringLiteral("screenscraper");
    original.boxArtUrl = QStringLiteral("https://example.com/art.jpg");
    original.matchMethod = MatchMethods::HASH;
    original.matchScore = 1.0f;
    original.externalIds[QStringLiteral("igdb")] = QStringLiteral("999");
    original.fetchedAt = QDateTime::currentDateTime();

    const QString hash = QStringLiteral("811b027eaf99c2def7b933c5208636de");
    QVERIFY(cache.store(original, hash, QStringLiteral("NES")));

    const GameMetadata retrieved = cache.getByHash(hash, QStringLiteral("NES"));
    QCOMPARE(retrieved.title, original.title);
    QCOMPARE(retrieved.genres, original.genres);
    QCOMPARE(retrieved.rating, original.rating);
    QCOMPARE(retrieved.externalIds, original.externalIds);
    QCOMPARE(retrieved.matchScore, original.matchScore);
    QVERIFY(retrieved.fetchedAt.isValid());
}

QTEST_MAIN(CacheDeserializationTest)

#include "test_cache_deserialization.moc"
