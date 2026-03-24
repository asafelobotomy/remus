#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
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

static const char *k_patchDatXml =
    "<?xml version=\"1.0\"?>\n"
    "<datafile>\n"
    "    <header>\n"
    "        <name>NES Patch Catalog (Test)</name>\n"
    "        <description>Known patched ROM outputs</description>\n"
    "        <version>20260102</version>\n"
    "        <author>test</author>\n"
    "    </header>\n"
    "    <game name=\"Dragon Quest III (English v2.0)[Addendum]\" base_title=\"Dragon Quest III\" patch_name=\"English v2.0 Addendum\" file_type=\"translation\">\n"
    "        <description>Verified translated build</description>\n"
    "        <rom name=\"Dragon Quest III (English v2.0)[Addendum].nes\"\n"
    "             size=\"40960\"\n"
    "             crc=\"1a2b3c4d\"\n"
    "             md5=\"11111111111111111111111111111111\"\n"
    "             sha1=\"2222222222222222222222222222222222222222\"/>\n"
    "    </game>\n"
    "</datafile>\n";

// ── File-scope helpers (kept outside the class body for MOC compatibility) ──

static QString writeDat(const QTemporaryDir &dir)
{
    const QString path = dir.path() + "/test.dat";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open DAT file for writing:" << path;
        return QString();
    }

    if (f.write(k_datXml) != qstrlen(k_datXml)) {
        qWarning() << "Failed to write DAT contents to:" << path;
        return QString();
    }

    f.close();
    return path;
}

static QString writePatchDat(const QTemporaryDir &dir)
{
    const QString path = dir.path() + "/patch-test.dat";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open patch DAT file for writing:" << path;
        return QString();
    }

    if (f.write(k_patchDatXml) != qstrlen(k_patchDatXml)) {
        qWarning() << "Failed to write patch DAT contents to:" << path;
        return QString();
    }

    f.close();
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
    void testImportPatchDat();
    void testVerifyMatchingHash();
    void testVerifyMismatch();
    void testVerifyNotInDat();
    void testVerifyHashMissing();
    void testVerifySummary();
    void testHasDat();
    void testHasPatchDat();
    void testRemoveDat();
    void testRemovePatchDat();
    void testGetMissingGames();
    void testVerifyPatchedHashPromotesMetadata();

    // Phase 0 characterization tests — safety net for Phase 2 split
    void testExportReportCsv();
    void testExportReportJson();
    void testGetImportedDatsReturnsHeaders();
    void testVerifyLibraryWithSystemFilter();
};

// ── Test implementations ───────────────────────────────────────────────────

void VerificationEngineTest::testImportDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    int count = engine.importDat(datPath, "NES");
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
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.fileId, fileId);
    QCOMPARE(result.status, VerificationStatus::Verified);
}

void VerificationEngineTest::testImportPatchDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    const QString datPath = writePatchDat(dir);
    QVERIFY(!datPath.isEmpty());
    const int count = engine.importPatchDat(datPath, "NES");
    QCOMPARE(count, 1);
}

void VerificationEngineTest::testVerifyMismatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = populateDb(db, "ffffffff");  // Wrong CRC

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

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
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

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
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

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
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");
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

    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");
    QVERIFY(engine.hasDat("NES"));
    QVERIFY(!engine.hasDat("SNES"));
}

void VerificationEngineTest::testHasPatchDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    QVERIFY(!engine.hasPatchDat("NES"));

    const QString datPath = writePatchDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importPatchDat(datPath, "NES");
    QVERIFY(engine.hasPatchDat("NES"));
    QVERIFY(!engine.hasPatchDat("SNES"));
}

void VerificationEngineTest::testRemoveDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");
    QVERIFY(engine.hasDat("NES"));

    QVERIFY(engine.removeDat("NES"));
    QVERIFY(!engine.hasDat("NES"));
}

void VerificationEngineTest::testRemovePatchDat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    const QString datPath = writePatchDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importPatchDat(datPath, "NES");
    QVERIFY(engine.hasPatchDat("NES"));

    QVERIFY(engine.removePatchDat("NES"));
    QVERIFY(!engine.hasPatchDat("NES"));
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
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

    QList<DatRomEntry> missing = engine.getMissingGames("NES");
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().gameName, QStringLiteral("Donkey Kong"));
}

void VerificationEngineTest::testVerifyPatchedHashPromotesMetadata()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId("NES");

    FileRecord fr;
    fr.libraryId = libId;
    fr.filename = "Dragon Quest III (English v2.0)[Addendum].nes";
    fr.originalPath = "/roms/Dragon Quest III (English v2.0)[Addendum].nes";
    fr.currentPath = fr.originalPath;
    fr.extension = ".nes";
    fr.systemId = sysId;
    fr.fileSize = 40960;
    fr.crc32 = "1a2b3c4d";
    fr.md5 = "11111111111111111111111111111111";
    fr.sha1 = "2222222222222222222222222222222222222222";
    fr.hashCalculated = true;
    const int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);
    QVERIFY(db.updateFileHashes(fileId, fr.crc32, fr.md5, fr.sha1));

    VerificationEngine engine(&db);
    const QString datPath = writePatchDat(dir);
    QVERIFY(!datPath.isEmpty());
    QCOMPARE(engine.importPatchDat(datPath, "NES"), 1);

    const VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.status, VerificationStatus::Verified);
    QVERIFY(result.notes.contains("patch catalog"));

    const FileRecord updated = db.getFileById(fileId);
    QCOMPARE(updated.baseTitle, QStringLiteral("Dragon Quest III"));
    QCOMPARE(updated.fileType, QStringLiteral("translation"));
    QVERIFY(updated.isPatched);
    QCOMPARE(updated.patchName, QStringLiteral("English v2.0 Addendum"));
}

// ── Phase 0 characterization tests ─────────────────────────────────────────

void VerificationEngineTest::testExportReportCsv()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    populateDb(db, "7b5e9e81",
               "811b027eaf99c2def7b933c5208636de",
               "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    QList<VerificationResult> results = engine.verifyLibrary("NES");
    QVERIFY(!results.isEmpty());

    const QString csvPath = dir.filePath("report.csv");
    QVERIFY(engine.exportReport(results, csvPath, "csv"));

    QFile f(csvPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csv = QString::fromUtf8(f.readAll());
    f.close();

    // CSV must have a header row
    QVERIFY(csv.startsWith("File ID,"));
    // Must contain the verified file
    QVERIFY(csv.contains("Verified"));
    // Must have at least header + 1 data row
    QVERIFY(csv.count('\n') >= 2);
}

void VerificationEngineTest::testExportReportJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    populateDb(db, "7b5e9e81",
               "811b027eaf99c2def7b933c5208636de",
               "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    QList<VerificationResult> results = engine.verifyLibrary("NES");
    QVERIFY(!results.isEmpty());

    const QString jsonPath = dir.filePath("report.json");
    QVERIFY(engine.exportReport(results, jsonPath, "json"));

    QFile f(jsonPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    QCOMPARE(parseErr.error, QJsonParseError::NoError);
    QVERIFY(doc.isArray());

    QJsonArray arr = doc.array();
    QCOMPARE(arr.size(), results.size());

    QJsonObject first = arr.first().toObject();
    QVERIFY(first.contains("status"));
    QCOMPARE(first["status"].toString(), QStringLiteral("verified"));
    QVERIFY(first.contains("filename"));
    QVERIFY(first.contains("system"));
}

void VerificationEngineTest::testGetImportedDatsReturnsHeaders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    QMap<QString, DatHeader> dats = engine.getImportedDats();
    QVERIFY(dats.contains("NES"));
    QCOMPARE(dats["NES"].name, QStringLiteral("Nintendo - NES (Test)"));
    QCOMPARE(dats["NES"].version, QStringLiteral("20260101"));

    // No patch DATs imported yet
    QMap<QString, DatHeader> patchDats = engine.getImportedPatchDats();
    QVERIFY(patchDats.isEmpty() || !patchDats.contains("NES"));
}

void VerificationEngineTest::testVerifyLibraryWithSystemFilter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    // Add files for two systems
    int libId = db.insertLibrary("/roms", "Test");
    int nesId = db.getSystemId("NES");
    int snesId = db.getSystemId("SNES");

    FileRecord nes;
    nes.libraryId      = libId;
    nes.filename       = "mario.nes";
    nes.originalPath   = "/roms/mario.nes";
    nes.currentPath    = nes.originalPath;
    nes.extension      = ".nes";
    nes.systemId       = nesId;
    nes.fileSize       = 40960;
    nes.crc32          = "7b5e9e81";
    nes.hashCalculated = true;
    int nesFileId = db.insertFile(nes);
    db.updateFileHashes(nesFileId, "7b5e9e81", "811b027eaf99c2def7b933c5208636de",
                        "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    FileRecord snes;
    snes.libraryId      = libId;
    snes.filename       = "dkc.sfc";
    snes.originalPath   = "/roms/dkc.sfc";
    snes.currentPath    = snes.originalPath;
    snes.extension      = ".sfc";
    snes.systemId       = snesId;
    snes.fileSize       = 1024;
    snes.crc32          = "abcdef01";
    snes.hashCalculated = true;
    int snesFileId = db.insertFile(snes);
    db.updateFileHashes(snesFileId, "abcdef01", QString(), QString());

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    // Filter to NES only — should not include SNES file
    QList<VerificationResult> nesResults = engine.verifyLibrary("NES");
    for (const auto &r : nesResults) {
        QCOMPARE(r.system, QStringLiteral("NES"));
    }

    // Full library — should include both
    QList<VerificationResult> allResults = engine.verifyLibrary();
    QVERIFY(allResults.size() >= nesResults.size());
}

QTEST_MAIN(VerificationEngineTest)
#include "test_verification_engine.moc"
