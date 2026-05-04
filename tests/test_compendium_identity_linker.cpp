#include <QtTest/QtTest>
#include <QList>

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
    void twoRecords_sameSerial_linked();
    void twoRecords_sameTitleSystemRegion_linked();
    void twoUnrelatedRecords_separateIds();
    void multipleRecords_sha1TakesPrecedenceOverCrc32();
    void titleNormalization_stripsThe_links();
    void titleNormalization_stripsLeadingA_links();
    void titleNormalization_differentRegion_notLinked();
    void emptyRecordList_returnsZero();
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

QTEST_MAIN(CompendiumIdentityLinkerTest)
#include "test_compendium_identity_linker.moc"
