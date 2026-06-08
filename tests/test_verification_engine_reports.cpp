#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "test_verification_engine_fixture.h"

void VerificationEngineTest::testGetMissingGames() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    populateDb(db, "7b5e9e81", "811b027eaf99c2def7b933c5208636de", "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    VerificationEngine engine(&db);
    const QString datPath = writeDat(dir);
    QVERIFY(!datPath.isEmpty());
    engine.importDat(datPath, "NES");

    QList<DatRomEntry> missing = engine.getMissingGames("NES");
    QCOMPARE(missing.size(), 1);
    QCOMPARE(missing.first().gameName, QStringLiteral("Donkey Kong"));
}

void VerificationEngineTest::testVerifyPatchedHashPromotesMetadata() {
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

void VerificationEngineTest::testExportReportCsv() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    populateDb(db, "7b5e9e81", "811b027eaf99c2def7b933c5208636de", "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

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

    QVERIFY(csv.startsWith("File ID,"));
    QVERIFY(csv.contains("Verified"));
    QVERIFY(csv.count('\n') >= 2);
}

void VerificationEngineTest::testExportReportJson() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    populateDb(db, "7b5e9e81", "811b027eaf99c2def7b933c5208636de", "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

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

void VerificationEngineTest::testGetImportedDatsReturnsHeaders() {
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

    QMap<QString, DatHeader> patchDats = engine.getImportedPatchDats();
    QVERIFY(patchDats.isEmpty() || !patchDats.contains("NES"));
}

void VerificationEngineTest::testVerifyLibraryWithSystemFilter() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int nesId = db.getSystemId("NES");
    int snesId = db.getSystemId("SNES");

    FileRecord nes;
    nes.libraryId = libId;
    nes.filename = "mario.nes";
    nes.originalPath = "/roms/mario.nes";
    nes.currentPath = nes.originalPath;
    nes.extension = ".nes";
    nes.systemId = nesId;
    nes.fileSize = 40960;
    nes.crc32 = "7b5e9e81";
    nes.hashCalculated = true;
    int nesFileId = db.insertFile(nes);
    db.updateFileHashes(
        nesFileId, "7b5e9e81", "811b027eaf99c2def7b933c5208636de", "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

    FileRecord snes;
    snes.libraryId = libId;
    snes.filename = "dkc.sfc";
    snes.originalPath = "/roms/dkc.sfc";
    snes.currentPath = snes.originalPath;
    snes.extension = ".sfc";
    snes.systemId = snesId;
    snes.fileSize = 1024;
    snes.crc32 = "abcdef01";
    snes.hashCalculated = true;
    int snesFileId = db.insertFile(snes);
    db.updateFileHashes(snesFileId, "abcdef01", QString(), QString());

    VerificationEngine engine(&db);
    engine.importDat(writeDat(dir), "NES");

    QList<VerificationResult> nesResults = engine.verifyLibrary("NES");
    for (const auto &r : nesResults) {
        QCOMPARE(r.system, QStringLiteral("NES"));
    }

    QList<VerificationResult> allResults = engine.verifyLibrary();
    QVERIFY(allResults.size() >= nesResults.size());
}