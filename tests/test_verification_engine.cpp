#include <QTemporaryDir>
#include <QFile>
#include "test_verification_engine_fixture.h"

// ── Minimal Logiqx-format DAT content for NES ─────────────────────────────

static const char *k_datXml = "<?xml version=\"1.0\"?>\n"
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

static const char *k_patchDatXml = "<?xml version=\"1.0\"?>\n"
                                   "<datafile>\n"
                                   "    <header>\n"
                                   "        <name>NES Patch Catalog (Test)</name>\n"
                                   "        <description>Known patched ROM outputs</description>\n"
                                   "        <version>20260102</version>\n"
                                   "        <author>test</author>\n"
                                   "    </header>\n"
                                   "    <game name=\"Dragon Quest III (English v2.0)[Addendum]\" base_title=\"Dragon "
                                   "Quest III\" patch_name=\"English v2.0 Addendum\" file_type=\"translation\">\n"
                                   "        <description>Verified translated build</description>\n"
                                   "        <rom name=\"Dragon Quest III (English v2.0)[Addendum].nes\"\n"
                                   "             size=\"40960\"\n"
                                   "             crc=\"1a2b3c4d\"\n"
                                   "             md5=\"11111111111111111111111111111111\"\n"
                                   "             sha1=\"2222222222222222222222222222222222222222\"/>\n"
                                   "    </game>\n"
                                   "</datafile>\n";

static const char *k_md5OnlyDatXml = "<?xml version=\"1.0\"?>\n"
                                     "<!DOCTYPE datafile PUBLIC \"-//Logiqx//DTD ROM Management Datafile//EN\"\n"
                                     "    \"http://www.logiqx.com/Docs/CMakeLists.dtd\">\n"
                                     "<datafile>\n"
                                     "    <header>\n"
                                     "        <name>Nintendo - NES (MD5 Test)</name>\n"
                                     "        <description>MD5-only DAT</description>\n"
                                     "        <version>20260103</version>\n"
                                     "        <author>test</author>\n"
                                     "    </header>\n"
                                     "    <game name=\"Super Mario Bros.\">\n"
                                     "        <description>Super Mario Bros.</description>\n"
                                     "        <rom name=\"Super Mario Bros. (World).nes\"\n"
                                     "             size=\"40960\"\n"
                                     "             md5=\"811b027eaf99c2def7b933c5208636de\"/>\n"
                                     "    </game>\n"
                                     "</datafile>\n";

// ── File-scope helpers (kept outside the class body for MOC compatibility) ──

QString writeDat(const QTemporaryDir &dir) {
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

QString writePatchDat(const QTemporaryDir &dir) {
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

QString writeMd5OnlyDat(const QTemporaryDir &dir) {
    const QString path = dir.path() + "/md5-only.dat";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open MD5-only DAT file for writing:" << path;
        return QString();
    }

    if (f.write(k_md5OnlyDatXml) != qstrlen(k_md5OnlyDatXml)) {
        qWarning() << "Failed to write MD5-only DAT contents to:" << path;
        return QString();
    }

    f.close();
    return path;
}

int populateDb(Database &db, const QString &crc, const QString &md5, const QString &sha1, bool hashCalculated) {
    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    FileRecord fr;
    fr.libraryId = libId;
    fr.filename = "Super Mario Bros. (World).nes";
    fr.originalPath = "/roms/Super Mario Bros. (World).nes";
    fr.currentPath = fr.originalPath;
    fr.extension = ".nes";
    fr.systemId = sysId;
    fr.fileSize = 40960;
    fr.crc32 = crc;
    fr.md5 = md5;
    fr.sha1 = sha1;
    fr.hashCalculated = hashCalculated;
    int fileId = db.insertFile(fr);
    // insertFile omits hash_calculated; persist it explicitly when needed.
    if (hashCalculated && !(crc.isEmpty() && md5.isEmpty() && sha1.isEmpty())) {
        db.updateFileHashes(fileId, crc, md5, sha1);
    }
    return fileId;
}
void VerificationEngineTest::testImportDat() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    int count = engine.importDat(datPath, "NES");
    QCOMPARE(count, 2); // Two game entries in the DAT
}

void VerificationEngineTest::testVerifyMatchingHash() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId
        = populateDb(db, "7b5e9e81", "811b027eaf99c2def7b933c5208636de", "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.fileId, fileId);
    QCOMPARE(result.status, VerificationStatus::Verified);
}

void VerificationEngineTest::testVerifyOfficialDatFallsBackToMd5WhenPreferredHashMissing() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    const int fileId = populateDb(db, QString(), QStringLiteral("811b027eaf99c2def7b933c5208636de"), QString(), true);

    VerificationEngine engine(&db);
    const QString datPath = writeMd5OnlyDat(dir);
    QVERIFY(!datPath.isEmpty());
    QCOMPARE(engine.importDat(datPath, QStringLiteral("NES")), 1);

    const VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.status, VerificationStatus::Verified);
    QCOMPARE(result.hashType, QStringLiteral("md5"));
    QCOMPARE(result.fileHash, QStringLiteral("811b027eaf99c2def7b933c5208636de"));
}

void VerificationEngineTest::testImportPatchDat() {
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

void VerificationEngineTest::testVerifyMismatch() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = populateDb(db, "ffffffff"); // Wrong CRC

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

    VerificationResult result = engine.verifyFile(fileId);
    QCOMPARE(result.status, VerificationStatus::NotInDat);
}

void VerificationEngineTest::testVerifyNotInDat() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    // Insert a file for a different game not in the DAT
    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    FileRecord fr;
    fr.libraryId = libId;
    fr.filename = "Unknown Game.nes";
    fr.originalPath = "/roms/Unknown Game.nes";
    fr.currentPath = fr.originalPath;
    fr.extension = ".nes";
    fr.systemId = sysId;
    fr.fileSize = 8192;
    fr.crc32 = "cafebabe";
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

void VerificationEngineTest::testVerifyHashMissing() {
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

void VerificationEngineTest::testVerifySummary() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    // One matching file
    populateDb(db, "7b5e9e81", "811b027eaf99c2def7b933c5208636de", "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

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

void VerificationEngineTest::testHasDat() {
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

void VerificationEngineTest::testHasPatchDat() {
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

void VerificationEngineTest::testRemoveDat() {
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

void VerificationEngineTest::testRemovePatchDat() {
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

QTEST_MAIN(VerificationEngineTest)
