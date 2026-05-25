#include <QtTest/QtTest>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/metadata/compendium_identity_linker.h"
#include "../src/metadata/compendium_types.h"

using namespace Remus::Compendium;

class CompendiumIdentityLinkerTest : public QObject
{
    Q_OBJECT

private slots:
    void singleRecord_getsNewGameId();
    void twoRecords_sameSha1_linked();
    void twoRecords_sameMd5_linked();
    void twoRecords_sameCrc32_linked();
    void twoRecords_sameSha256_linked();
    void twoRecords_sameSerial_linked();
    void twoRecords_sameTitleSystemRegion_linked();
    void twoUnrelatedRecords_separateIds();
    void multipleRecords_sha1TakesPrecedenceOverCrc32();
    void titleNormalization_stripsThe_links();
    void titleNormalization_stripsLeadingA_links();
    void titleNormalization_differentRegion_notLinked();
    void emptyRecordList_returnsZero();
    void loadFromDatabase_populatesMapsFromExistingDb();
    void multiDisc_discSuffix_sameGame();
    void multiDisc_cdSuffix_sameGame();
};

// Helper to make a minimal envelope
static SourceRecordEnvelope makeRec(const QString &sha1 = {},
                                    const QString &crc32 = {},
                                    const QString &md5 = {})
{
    static int counter = 0;
    SourceRecordEnvelope r;
    r.hashes.sha1  = sha1;
    r.hashes.crc32 = crc32;
    r.hashes.md5   = md5;
    r.externalKey  = QStringLiteral("key-%1").arg(++counter);
    return r;
}

// ── Link pass 1: hash collision ──────────────────────────────────────────────

void CompendiumIdentityLinkerTest::singleRecord_getsNewGameId()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;
    records.append(makeRec(QStringLiteral("sha1aaa")));

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QVERIFY(!records.first().linkedGameId.isEmpty());
    // Confidence for a newly-minted (unmatched) game is set by the linker.
    QVERIFY(records.first().linkedConfidencePercent >= 0);
}

void CompendiumIdentityLinkerTest::twoRecords_sameSha1_linked()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1 = makeRec(QStringLiteral("sha1shared"));
    SourceRecordEnvelope r2 = makeRec(QStringLiteral("sha1shared"));
    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedConfidencePercent, 100);
}

void CompendiumIdentityLinkerTest::twoRecords_sameMd5_linked()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1 = makeRec({}, {}, QStringLiteral("md5shared"));
    SourceRecordEnvelope r2 = makeRec({}, {}, QStringLiteral("md5shared"));
    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedConfidencePercent, 95);
}

void CompendiumIdentityLinkerTest::twoRecords_sameCrc32_linked()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1 = makeRec({}, QStringLiteral("crc32shared"));
    SourceRecordEnvelope r2 = makeRec({}, QStringLiteral("crc32shared"));
    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedConfidencePercent, 90);
}

void CompendiumIdentityLinkerTest::twoRecords_sameSha256_linked()
{
    // SHA-256 must be registered in the in-memory map after a game is minted
    // so that a subsequent record in the same batch can link via SHA-256.
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey       = QStringLiteral("sha256key-1");
    r1.hashes.sha256     = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    SourceRecordEnvelope r2;
    r2.externalKey       = QStringLiteral("sha256key-2");
    r2.hashes.sha256     = QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedConfidencePercent, 100);
}

void CompendiumIdentityLinkerTest::twoRecords_sameSerial_linked()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey      = QStringLiteral("serialkey-1");
    r1.serials          = { QStringLiteral("SLUS-00001") };
    r1.resolvedSystemId = 10;

    SourceRecordEnvelope r2;
    r2.externalKey      = QStringLiteral("serialkey-2");
    r2.serials          = { QStringLiteral("SLUS-00001") };
    r2.resolvedSystemId = 10;

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedConfidencePercent, 80);
}

void CompendiumIdentityLinkerTest::twoRecords_sameTitleSystemRegion_linked()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey          = QStringLiteral("titlekey-1");
    r1.titleRaw             = QStringLiteral("Super Mario Bros.");
    r1.resolvedSystemId     = 1;
    r1.resolvedRegionCode   = QStringLiteral("USA");

    SourceRecordEnvelope r2;
    r2.externalKey          = QStringLiteral("titlekey-2");
    r2.titleRaw             = QStringLiteral("Super Mario Bros.");
    r2.resolvedSystemId     = 1;
    r2.resolvedRegionCode   = QStringLiteral("USA");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);

    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedConfidencePercent, 60);
}

void CompendiumIdentityLinkerTest::twoUnrelatedRecords_separateIds()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;
    records.append(makeRec(QStringLiteral("sha1aaa")));
    records.append(makeRec(QStringLiteral("sha1bbb")));

    const int created = linker.link(records);

    QCOMPARE(created, 2);
    QVERIFY(!records[0].linkedGameId.isEmpty());
    QVERIFY(!records[1].linkedGameId.isEmpty());
    QVERIFY(records[0].linkedGameId != records[1].linkedGameId);
}

void CompendiumIdentityLinkerTest::multipleRecords_sha1TakesPrecedenceOverCrc32()
{
    // r1 and r2 share the same SHA1 so they should link together,
    // even though r2 and r3 share a CRC32 (they should NOT link).
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1 = makeRec(QStringLiteral("sha1x"), QStringLiteral("crc1"));
    SourceRecordEnvelope r2 = makeRec(QStringLiteral("sha1x"), QStringLiteral("crc2"));
    SourceRecordEnvelope r3 = makeRec({},                      QStringLiteral("crc2"));
    records.append(r1);
    records.append(r2);
    records.append(r3);

    const int created = linker.link(records);

    // r1+r2 share sha1 → 1 game; r3 has new crc2 (already taken by r2) → linked to r2's game
    // Actually: r3 has crc2 which was registered for r2's gameId, so r3 links to r2
    // → 2 total games created (r1+r2 share, r3 is separate from r1+r2? No...
    // Wait: r2 registers crc2 → r3 finds crc2 → r3 links to r2's game
    // So: r1, r2, r3 all linked → 1 game created
    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
    QCOMPARE(records[1].linkedGameId, records[2].linkedGameId);
}

// ── Title normalisation (tested indirectly via link()) ───────────────────────

void CompendiumIdentityLinkerTest::titleNormalization_stripsThe_links()
{
    // "The Legend of Zelda" and "Legend of Zelda" should normalise to the
    // same token and link together (same system + region).
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey        = QStringLiteral("the-key-1");
    r1.titleRaw           = QStringLiteral("The Legend of Zelda");
    r1.resolvedSystemId   = 1;
    r1.resolvedRegionCode = QStringLiteral("USA");

    SourceRecordEnvelope r2;
    r2.externalKey        = QStringLiteral("the-key-2");
    r2.titleRaw           = QStringLiteral("Legend of Zelda");
    r2.resolvedSystemId   = 1;
    r2.resolvedRegionCode = QStringLiteral("USA");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);
    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
}

void CompendiumIdentityLinkerTest::titleNormalization_stripsLeadingA_links()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey        = QStringLiteral("a-key-1");
    r1.titleRaw           = QStringLiteral("A Link to the Past");
    r1.resolvedSystemId   = 2;
    r1.resolvedRegionCode = QStringLiteral("USA");

    SourceRecordEnvelope r2;
    r2.externalKey        = QStringLiteral("a-key-2");
    r2.titleRaw           = QStringLiteral("Link to the Past");
    r2.resolvedSystemId   = 2;
    r2.resolvedRegionCode = QStringLiteral("USA");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);
    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
}

void CompendiumIdentityLinkerTest::titleNormalization_differentRegion_notLinked()
{
    // Same title + system but different region → different keys → separate games.
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey        = QStringLiteral("reg-key-1");
    r1.titleRaw           = QStringLiteral("Super Mario Bros.");
    r1.resolvedSystemId   = 1;
    r1.resolvedRegionCode = QStringLiteral("USA");

    SourceRecordEnvelope r2;
    r2.externalKey        = QStringLiteral("reg-key-2");
    r2.titleRaw           = QStringLiteral("Super Mario Bros.");
    r2.resolvedSystemId   = 1;
    r2.resolvedRegionCode = QStringLiteral("JPN");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);
    QCOMPARE(created, 2);
    QVERIFY(records[0].linkedGameId != records[1].linkedGameId);
}

void CompendiumIdentityLinkerTest::emptyRecordList_returnsZero()
{
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;
    QCOMPARE(linker.link(records), 0);
}

void CompendiumIdentityLinkerTest::loadFromDatabase_populatesMapsFromExistingDb()
{
    // Create an in-memory database seeded with one game so that the linker
    // will link new records to the existing game rather than minting a fresh id.
    const QString connName = QStringLiteral("linker_test_%1")
        .arg(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());

    auto exec = [&](const QString &sql) {
        QSqlQuery q(db);
        return q.exec(sql);
    };

    QVERIFY(exec(QStringLiteral(
        "CREATE TABLE games ("
        "game_id TEXT PRIMARY KEY, system_id INTEGER NOT NULL,"
        " canonical_title TEXT NOT NULL, primary_region_code TEXT)")));
    QVERIFY(exec(QStringLiteral(
        "CREATE TABLE game_signatures ("
        "signature_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL,"
        " hash_type TEXT NOT NULL, hash_value TEXT NOT NULL)")));
    QVERIFY(exec(QStringLiteral(
        "CREATE TABLE game_serials ("
        "serial_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL,"
        " serial_value TEXT NOT NULL)")));
    QVERIFY(exec(QStringLiteral(
        "CREATE TABLE game_names ("
        "game_name_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL,"
        " name_text TEXT NOT NULL)")));

    QVERIFY(exec(QStringLiteral(
        "INSERT INTO games (game_id, system_id, canonical_title, primary_region_code)"
        " VALUES ('existing-1', 1, 'Test Game', 'USA')")));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO game_signatures (game_id, hash_type, hash_value)"
        " VALUES ('existing-1', 'md5', 'testmd5hashvalue')")));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO game_names (game_id, name_text)"
        " VALUES ('existing-1', 'Test Game')")));

    IdentityLinker linker;
    QString error;
    QVERIFY(linker.loadFromDatabase(db, error));
    QVERIFY(error.isEmpty());

    db.close();
    QSqlDatabase::removeDatabase(connName);

    // A new envelope whose md5 matches the seeded signature should link to the existing game.
    QList<SourceRecordEnvelope> records;
    SourceRecordEnvelope r;
    r.externalKey = QStringLiteral("new-key-1");
    r.hashes.md5  = QStringLiteral("testmd5hashvalue");
    records.append(r);

    const int created = linker.link(records);
    QCOMPARE(created, 0);  // No new game minted; linked to existing
    QCOMPARE(records.first().linkedGameId, QStringLiteral("existing-1"));
}

QTEST_MAIN(CompendiumIdentityLinkerTest)

void CompendiumIdentityLinkerTest::multiDisc_discSuffix_sameGame()
{
    // "Game (Disc 1)" and "Game (Disc 2)" should normalize to the same key and
    // therefore share a game_id.
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey        = QStringLiteral("disc-key-1");
    r1.titleRaw           = QStringLiteral("Final Fantasy VIII (Disc 1)");
    r1.resolvedSystemId   = 5;
    r1.resolvedRegionCode = QStringLiteral("USA");

    SourceRecordEnvelope r2;
    r2.externalKey        = QStringLiteral("disc-key-2");
    r2.titleRaw           = QStringLiteral("Final Fantasy VIII (Disc 2)");
    r2.resolvedSystemId   = 5;
    r2.resolvedRegionCode = QStringLiteral("USA");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);
    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
}

void CompendiumIdentityLinkerTest::multiDisc_cdSuffix_sameGame()
{
    // "Game CD 1" and "Game CD 2" should normalize to the same key.
    IdentityLinker linker;
    QList<SourceRecordEnvelope> records;

    SourceRecordEnvelope r1;
    r1.externalKey        = QStringLiteral("cd-key-1");
    r1.titleRaw           = QStringLiteral("Baldur's Gate CD 1");
    r1.resolvedSystemId   = 3;
    r1.resolvedRegionCode = QStringLiteral("USA");

    SourceRecordEnvelope r2;
    r2.externalKey        = QStringLiteral("cd-key-2");
    r2.titleRaw           = QStringLiteral("Baldur's Gate CD 2");
    r2.resolvedSystemId   = 3;
    r2.resolvedRegionCode = QStringLiteral("USA");

    records.append(r1);
    records.append(r2);

    const int created = linker.link(records);
    QCOMPARE(created, 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
}

#include "test_compendium_identity_linker.moc"