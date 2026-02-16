#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "metadata/metadata_cache.h"

using namespace Remus;

class MetadataCacheTest : public QObject {
    Q_OBJECT

private slots:
    void storeAndRetrieveMetadata();
    void artworkAndCleanup();
};

static QSqlDatabase createDatabase()
{
    const QString connectionName = QStringLiteral("cache-test-%1").arg(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(":memory:");
    db.open();

    QSqlQuery query(db);
    query.exec("CREATE TABLE cache (cache_key TEXT PRIMARY KEY, cache_value BLOB, expiry TEXT, created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    return db;
}

void MetadataCacheTest::storeAndRetrieveMetadata()
{
    QSqlDatabase db = createDatabase();
    MetadataCache cache(db);

    GameMetadata metadata;
    metadata.id = "42";
    metadata.title = "Test Game";
    metadata.system = "NES";
    metadata.region = "USA";
    metadata.publisher = "Pub";
    metadata.developer = "Dev";
    metadata.genres = {"Action", "Puzzle"};
    metadata.releaseDate = "1991-01-01";
    metadata.description = "Desc";
    metadata.players = 2;
    metadata.rating = 8.5f;
    metadata.providerId = "dummy";
    metadata.boxArtUrl = "http://example";
    metadata.matchMethod = "hash";
    metadata.matchScore = 1.0f;
    metadata.externalIds.insert("ext", "123");
    metadata.fetchedAt = QDateTime::currentDateTimeUtc();

    QVERIFY(cache.store(metadata, "abcd", "NES"));

    GameMetadata byHash = cache.getByHash("abcd", "NES");
    QCOMPARE(byHash.title, metadata.title);
    QCOMPARE(byHash.genres.size(), metadata.genres.size());
    QCOMPARE(byHash.externalIds.value("ext"), QString("123"));

    GameMetadata byProvider = cache.getByProviderId("dummy", "42");
    QCOMPARE(byProvider.title, metadata.title);

    const auto stats = cache.getStats();
    QVERIFY(stats.totalEntries >= 1);
    QVERIFY(stats.entriesThisWeek >= 1);
    QVERIFY(stats.totalSizeBytes > 0);
}

void MetadataCacheTest::artworkAndCleanup()
{
    QSqlDatabase db = createDatabase();
    MetadataCache cache(db);

    ArtworkUrls artwork;
    artwork.boxFront = QUrl("front");
    artwork.boxBack = QUrl("back");

    QVERIFY(cache.storeArtwork("game123", artwork));

    ArtworkUrls loaded = cache.getArtwork("game123");
    QCOMPARE(loaded.boxFront, artwork.boxFront);
    QCOMPARE(loaded.boxBack, artwork.boxBack);

    QSqlQuery insertOld(db);
    insertOld.prepare("INSERT INTO cache (cache_key, cache_value, expiry, created_at) VALUES ('old','{}', datetime('now','-40 days'), datetime('now','-40 days'))");
    QVERIFY(insertOld.exec());

    const int removed = cache.clearOldCache(30);
    QCOMPARE(removed, 1);
}

QTEST_MAIN(MetadataCacheTest)
#include "test_metadata_cache.moc"
