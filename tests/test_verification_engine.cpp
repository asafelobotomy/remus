#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/verification_engine.h"
#include "../src/core/database.h"

using namespace Remus;

// ── Minimal Logiqx-format DAT content for NES ─────────────────────────────

static const char *k_datXml =
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE datafile PUBLIC \"-//Logiqx//DTD ROM Management Datafile//EN\"\n"
    "    \"http://www.logiqx.com/Docs/CMakeLists.dtd\">\n"
    "<datafile>\n"
    "    <header>\n"
    "        <name>Nintendo - NES (Test)</name>\n"
    "        <description>Test DAT</description>\n"
    "        <version>20260101</version>\n"
    "        <author>test</author>\n"
    "    </header>\n"
    "    <game name=\"Super Mario Bros.\">\n"
    "        <description>Super Mario Bros.</description>\n"
    "        <rom name=\"Super Mario Bros. (World).nes\"\n"
    "             size=\"40960\"\n"
    "             crc=\"7b5e9e81\"\n"
    "             md5=\"811b027eaf99c2def7b933c5208636de\"\n"
    "             sha1=\"ea343f4e445a9050d4b4fbac2c77d0693b1d0922\"/>\n"
    "    </game>\n"
    "    <game name=\"Donkey Kong\">\n"
    "        <description>Donkey Kong</description>\n"
    "        <rom name=\"Donkey Kong (World).nes\"\n"
    "             size=\"16384\"\n"
    "             crc=\"deadbeef\"\n"
    "             md5=\"00000000000000000000000000000001\"\n"
    "             sha1=\"0000000000000000000000000000000000000001\"/>\n"
    "    </game>\n"
    "</datafile>\n";

// ── File-scope helpers (kept outside the class body for MOC compatibility) ──

static QString writeDat(const QTemporaryDir &dir)
{
    const QString path = dir.path() + "/test.dat";
    QFile f(path);
    Q_ASSERT(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(k_datXml);
    return path;
}

static int populateDb(Database &db, const QString &crc,
                      const QString &md5 = QString(),
                      const QString &sha1 = QString(),
                      bool hashCalculated = true)
{
    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    FileRecord fr;
    fr.libraryId      = libId;
    fr.filename       = "Super Mario Bros. (World).nes";
    fr.originalPath   = "/roms/Super Mario Bros. (World).nes";
    fr.currentPath    = fr.originalPath;
    fr.extension      = ".nes";
    fr.systemId       = sysId;
    fr.fileSize       = 40960;
    fr.crc32          = crc;
    fr.md5            = md5;
    fr.sha1           = sha1;
    fr.hashCalculated = hashCalculated;
    int fileId = db.insertFile(fr);
    // insertFile omits hash_calculated; persist it explicitly when needed.
    if (hashCalculated && !(crc.isEmpty() && md5.isEmpty() && sha1.isEmpty())) {
        db.updateFileHashes(fileId, crc, md5, sha1);
    }
    return fileId;
}



// ── Fixture ────────────────────────────────────────────────────────────────

class VerificationEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void testImportDat();
    void testVerifyMatchingHash();
    void testVerifyMismatch();
    void testVerifyNotInDat();
    void testVerifyHashMissing();
    void testVerifySummary();
    void testHasDat();
    void testRemoveDat();
    void testGetMissingGames();
};

// ── Test implementations ───────────────────────────────────────────────────

void VerificationEngineTest::testImportDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    int count = engine.importDat(writeDat(dir), "NES");
    QCOMPARE(count, 2);  // Two game entries in the DAT
}

void VerificationEngineTest::testVerifyMatchingHash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = populateDb(db, "7b5e9e81",
                             "811b027eaf99c2def7b933c5208636de",
                             "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.fileId, fileId);
    QCOMPARE(result.status, VerificationStatus::Verified);
}

void VerificationEngineTest::testVerifyMismatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = populateDb(db, "ffffffff");  // Wrong CRC

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.status, VerificationStatus::NotInDat);
}

void VerificationEngineTest::testVerifyNotInDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    // Insert a file for a different game not in the DAT
    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    FileRecord fr;
    fr.libraryId      = libId;
    fr.filename       = "Unknown Game.nes";
    fr.originalPath   = "/roms/Unknown Game.nes";
    fr.currentPath    = fr.originalPath;
    fr.extension      = ".nes";
    fr.systemId       = sysId;
    fr.fileSize       = 8192;
    fr.crc32          = "cafebabe";
    fr.hashCalculated = true;
    int fileId = db.insertFile(fr);
    // insertFile omits hash_calculated; persist it explicitly.
    db.updateFileHashes(fileId, "cafebabe", QString(), QString());

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.status, VerificationStatus::NotInDat);
}

void VerificationEngineTest::testVerifyHashMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    // hashCalculated = false means no hashes present
    int fileId = populateDb(db, QString(), QString(), QString(), false);

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.status, VerificationStatus::HashMissing);
}

void VerificationEngineTest::testVerifySummary()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    // One matching file
    populateDb(db, "7b5e9e81",
               "811b027eaf99c2def7b933c5208636de",
               "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");
    engine.verifyLibrary("NES");

    VerificationSummary summary = engine.getLastSummary();
    QCOMPARE(summary.totalFiles, 1);
    QCOMPARE(summary.verified, 1);
    QCOMPARE(summary.mismatched, 0);
}

void VerificationEngineTest::testHasDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    QVERIFY(!engine.hasDat("NES"));

    engine.importDat(writeDat(dir), "NES");
    QVERIFY(engine.hasDat("NES"));
    QVERIFY(!engine.hasDat("SNES"));
}

void VerificationEngineTest::testRemoveDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");
    QVERIFY(engine.hasDat("NES"));

    QVERIFY(engine.removeDat("NES"));
    QVERIFY(!engine.hasDat("NES"));
}

void VerificationEngineTest::testGetMissingGames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    // Only Mario is in the library; Donkey Kong is in the DAT but not in the library.
    populateDb(db, "7b5e9e81",
               "811b027eaf99c2def7b933c5208636de",
               "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    QList<DatRomEntry> missing = engine.getMissingGames("NES");
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().gameName, QStringLiteral("Donkey Kong"));
}

QTEST_MAIN(VerificationEngineTest)
#include "test_verification_engine.moc"
