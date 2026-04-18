#include <QTemporaryDir>

#include "test_database_fixture.h"

void DatabaseTest::testGetFilesBySystem()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int nesId = db.getSystemId("NES");
    int snesId = db.getSystemId("SNES");

    db.insertFile(makeRecord(libId, nesId, "mario.nes"));
    db.insertFile(makeRecord(libId, nesId, "zelda.nes"));
    db.insertFile(makeRecord(libId, snesId, "dkc.sfc"));

    QList<FileRecord> nesFiles = db.getFilesBySystem("NES");
    QCOMPARE(nesFiles.size(), 2);

    QList<FileRecord> snesFiles = db.getFilesBySystem("SNES");
    QCOMPARE(snesFiles.size(), 1);
}

void DatabaseTest::testMarkFileProcessed()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    QList<FileRecord> unprocBefore = db.getUnprocessedFiles();
    QCOMPARE(unprocBefore.size(), 1);

    QVERIFY(db.markFileProcessed(fileId));

    QList<FileRecord> procAfter = db.getProcessedFiles();
    QCOMPARE(procAfter.size(), 1);
    QCOMPARE(procAfter.first().id, fileId);

    QVERIFY(db.markFileUnprocessed(fileId));
    QList<FileRecord> unprocAfter = db.getUnprocessedFiles();
    QCOMPARE(unprocAfter.size(), 1);
}

void DatabaseTest::testInsertGame()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));
    int sysId = db.getSystemId("SNES");

    int gameId = db.insertGame("Chrono Trigger", sysId, "USA",
                               "Square", "Square", "1995-08-22",
                               "Classic RPG", "RPG", "1", 9.8f);
    QVERIFY(gameId > 0);
}

void DatabaseTest::testUpdateGame()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));
    int sysId = db.getSystemId("SNES");
    int gameId = db.insertGame("Chrono Trigger", sysId);
    QVERIFY(gameId > 0);

    QVERIFY(db.updateGame(gameId, "Square", "Square", "1995-08-22",
                          "Classic RPG", "RPG", "1", 9.8f));
}

void DatabaseTest::testGetFileCountBySystem()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));
    int libId = db.insertLibrary("/roms", "Test");
    int nesId = db.getSystemId("NES");
    int snesId = db.getSystemId("SNES");

    db.insertFile(makeRecord(libId, nesId, "mario.nes"));
    db.insertFile(makeRecord(libId, snesId, "dkc.sfc"));
    db.insertFile(makeRecord(libId, snesId, "ffvi.sfc"));

    QMap<QString, int> counts = db.getFileCountBySystem();
    QCOMPARE(counts.value("NES"), 1);
    QCOMPARE(counts.value("SNES"), 2);
}

void DatabaseTest::testGetFilesWithoutHashes()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));
    db.updateFileHashes(fid1, "AABB", "md5", "sha1");

    QList<FileRecord> noHash = db.getFilesWithoutHashes();
    QCOMPARE(noHash.size(), 1);
    QCOMPARE(noHash.first().id, fid2);
}

void DatabaseTest::testInsertFileDuplicateReturnsZero()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId("NES");

    const FileRecord first = makeRecord(libId, sysId, "mario.nes");
    QVERIFY(db.insertFile(first) > 0);
    QCOMPARE(db.insertFile(first), 0);
}

void DatabaseTest::testInsertArchiveMembersWithSameBasenameRemainDistinct()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId("Nintendo DS");

    FileRecord first = makeRecord(libId, sysId, "game.nds");
    first.originalPath = QStringLiteral("/roms/archive.zip");
    first.currentPath = first.originalPath;
    first.isCompressed = true;
    first.archivePath = first.originalPath;
    first.archiveInternalPath = QStringLiteral("folder-a/game.nds");

    FileRecord second = first;
    second.archiveInternalPath = QStringLiteral("folder-b/game.nds");

    QVERIFY(db.insertFile(first) > 0);
    QVERIFY(db.insertFile(second) > 0);
    QCOMPARE(db.getAllFiles().size(), 2);
}

void DatabaseTest::testGetUnprocessedFiles()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));
    db.markFileProcessed(fid1);

    QList<FileRecord> unproc = db.getUnprocessedFiles();
    QCOMPARE(unproc.size(), 1);
    QCOMPARE(unproc.first().id, fid2);
}

void DatabaseTest::testUpdateFilePath()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    const QString newPath = "/roms/organized/mario.nes";
    QVERIFY(db.updateFilePath(fileId, newPath));

    FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.currentPath, newPath);
}

void DatabaseTest::testInsertAndGetPatchedFileMetadata()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId("SNES");

    FileRecord fr = makeRecord(libId, sysId, "Dragon Quest III (English v2.0)[Addendum].sfc");
    const int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);

    const FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.baseTitle, QStringLiteral("Dragon Quest III"));
    QCOMPARE(got.fileType, QStringLiteral("translation"));
    QVERIFY(got.isPatched);
    QCOMPARE(got.patchName, QStringLiteral("English v2.0 Addendum"));
}

void DatabaseTest::testUpdateFileHashesPromotesPatchedMetadata()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId("SNES");

    FileRecord fr = makeRecord(libId, sysId, "Dragon Quest III.sfc");
    const int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);

    Database::AppliedPatchRecord record;
    record.basePath = "/roms/Dragon Quest III.sfc";
    record.outputPath = "/roms/Dragon Quest III [English v2.0].sfc";
    record.patchPath = "/patches/dq3-english.bps";
    record.patchFormat = "BPS";
    record.baseTitle = "Dragon Quest III";
    record.patchName = "English v2.0";
    record.fileType = "translation";
    record.outputCrc32 = "CCCC3333";
    record.outputMd5 = "patched-md5";
    record.outputSha1 = "patched-sha1";
    QVERIFY(db.insertAppliedPatch(record));

    QVERIFY(db.updateFileHashes(fileId, "CCCC3333", "patched-md5", "patched-sha1"));

    const FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.baseTitle, QStringLiteral("Dragon Quest III"));
    QCOMPARE(got.fileType, QStringLiteral("translation"));
    QVERIFY(got.isPatched);
    QCOMPARE(got.patchName, QStringLiteral("English v2.0"));
}

void DatabaseTest::testDeleteFilesForLibrary()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    db.insertFile(makeRecord(libId, sysId, "zelda.nes"));
    QCOMPARE(db.getFilesBySystem("NES").size(), 2);

    QVERIFY(db.deleteFilesForLibrary(libId));
    QCOMPARE(db.getFilesBySystem("NES").size(), 0);
    QVERIFY(!db.getLibraryPath(libId).isEmpty());
}

void DatabaseTest::testGetAllFilesIncludesStaleEntries()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    db.insertFile(makeRecord(libId, sysId, "nonexistent.nes"));

    QList<FileRecord> all = db.getAllFiles();
    QVERIFY(!all.isEmpty());
}

void DatabaseTest::testGetExistingFilesOnlyReturnsValidPaths()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    db.insertFile(makeRecord(libId, sysId, "nonexistent.nes"));

    QList<FileRecord> existing = db.getExistingFiles();
    QCOMPARE(existing.size(), 0);
}

void DatabaseTest::testGetFilePath()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    QString path = db.getFilePath(fileId);
    QCOMPARE(path, QStringLiteral("/roms/mario.nes"));
    QVERIFY(db.getFilePath(99999).isEmpty());
}

void DatabaseTest::testUpdateFileOriginalPath()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    const QString newOrigPath = "/extracted/mario.nes";
    QVERIFY(db.updateFileOriginalPath(fileId, newOrigPath));

    FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.originalPath, newOrigPath);
}