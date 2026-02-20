#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "../src/core/database.h"

using namespace Remus;

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitializeInMemory();
    void testInsertAndGetFile();
    void testUpdateFileHashes();
    void testRemoveFile();
    void testInsertAndGetMatch();
    void testConfirmRejectMatch();
    void testInsertLibraryAndDelete();
    void testGetFilesBySystem();
    void testMarkFileProcessed();
    void testInsertGame();
    void testUpdateGame();
    void testGetFileCountBySystem();
    void testGetFilesWithoutHashes();
    void testGetUnprocessedFiles();
    void testUpdateFilePath();
};

// ── Helpers ──────────────────────────────────────────────────────────────────

static FileRecord makeRecord(int libId, int sysId, const QString &name)
{
    FileRecord fr;
    fr.libraryId     = libId;
    fr.filename      = name;
    fr.originalPath  = "/roms/" + name;
    fr.currentPath   = fr.originalPath;
    fr.extension     = "." + name.section('.', -1);
    fr.systemId      = sysId;
    fr.fileSize      = 1024;
    return fr;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

void DatabaseTest::testInitializeInMemory()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));
}

void DatabaseTest::testInsertAndGetFile()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    QVERIFY(libId > 0);

    int sysId = db.getSystemId("NES");
    QVERIFY(sysId > 0);

    FileRecord fr = makeRecord(libId, sysId, "mario.nes");
    int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);

    FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.id, fileId);
    QCOMPARE(got.filename, QStringLiteral("mario.nes"));
    QCOMPARE(got.systemId, sysId);
}

void DatabaseTest::testUpdateFileHashes()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    FileRecord fr = makeRecord(libId, sysId, "mario.nes");
    int fileId = db.insertFile(fr);

    QVERIFY(db.updateFileHashes(fileId, "AABBCCDD", "abcd1234md5", "sha1sha1sha1"));

    FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.crc32,  QStringLiteral("AABBCCDD"));
    QCOMPARE(got.md5,    QStringLiteral("abcd1234md5"));
    QCOMPARE(got.sha1,   QStringLiteral("sha1sha1sha1"));
    QVERIFY(got.hashCalculated);
}

void DatabaseTest::testRemoveFile()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    FileRecord fr = makeRecord(libId, sysId, "mario.nes");
    int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);

    QVERIFY(db.removeFile(fileId));

    // After removal the record should not be found
    FileRecord gone = db.getFileById(fileId);
    QCOMPARE(gone.id, 0);
}

void DatabaseTest::testInsertAndGetMatch()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int gameId = db.insertGame("Super Mario Bros.", sysId, "USA",
                               "Nintendo", "Nintendo", "1985-09-13",
                               "Classic platformer", "Platform", "1", 9.0f);
    QVERIFY(gameId > 0);
    QVERIFY(db.insertMatch(fileId, gameId, 100.0f, "hash"));

    Database::MatchResult m = db.getMatchForFile(fileId);
    QCOMPARE(m.fileId,      fileId);
    QCOMPARE(m.gameId,      gameId);
    QCOMPARE(m.matchMethod, QStringLiteral("hash"));
    QVERIFY(m.confidence >= 99.0f);
}

void DatabaseTest::testConfirmRejectMatch()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int gameId = db.insertGame("Super Mario Bros.", sysId);
    db.insertMatch(fileId, gameId, 80.0f, "fuzzy");

    QVERIFY(db.confirmMatch(fileId));
    {
        Database::MatchResult m = db.getMatchForFile(fileId);
        QVERIFY(m.isConfirmed);
        QVERIFY(!m.isRejected);
    }

    QVERIFY(db.rejectMatch(fileId));
    {
        Database::MatchResult m = db.getMatchForFile(fileId);
        QVERIFY(m.isRejected);
    }
}

void DatabaseTest::testInsertLibraryAndDelete()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/nes", "NES Library");
    QVERIFY(libId > 0);
    QCOMPARE(db.getLibraryPath(libId), QStringLiteral("/roms/nes"));

    QVERIFY(db.deleteLibrary(libId));
    QVERIFY(db.getLibraryPath(libId).isEmpty());
}

void DatabaseTest::testGetFilesBySystem()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId   = db.insertLibrary("/roms", "Test");
    int nesId   = db.getSystemId("NES");
    int snesId  = db.getSystemId("SNES");

    db.insertFile(makeRecord(libId, nesId,  "mario.nes"));
    db.insertFile(makeRecord(libId, nesId,  "zelda.nes"));
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

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    QList<FileRecord> unprocBefore = db.getUnprocessedFiles();
    QCOMPARE(unprocBefore.size(), 1);

    QVERIFY(db.markFileProcessed(fileId));

    QList<FileRecord> procAfter = db.getProcessedFiles();
    QCOMPARE(procAfter.size(), 1);
    QCOMPARE(procAfter.first().id, fileId);

    // Unmark
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
    int sysId  = db.getSystemId("SNES");
    int gameId = db.insertGame("Chrono Trigger", sysId);
    QVERIFY(gameId > 0);

    // Enrich with publisher data
    QVERIFY(db.updateGame(gameId, "Square", "Square", "1995-08-22",
                          "Classic RPG", "RPG", "1", 9.8f));
}

void DatabaseTest::testGetFileCountBySystem()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));
    int libId  = db.insertLibrary("/roms", "Test");
    int nesId  = db.getSystemId("NES");
    int snesId = db.getSystemId("SNES");

    db.insertFile(makeRecord(libId, nesId,  "mario.nes"));
    db.insertFile(makeRecord(libId, snesId, "dkc.sfc"));
    db.insertFile(makeRecord(libId, snesId, "ffvi.sfc"));

    QMap<QString, int> counts = db.getFileCountBySystem();
    QCOMPARE(counts.value("NES"),  1);
    QCOMPARE(counts.value("SNES"), 2);
}

void DatabaseTest::testGetFilesWithoutHashes()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));
    db.updateFileHashes(fid1, "AABB", "md5", "sha1");  // fid1 has hashes

    QList<FileRecord> noHash = db.getFilesWithoutHashes();
    QCOMPARE(noHash.size(), 1);
    QCOMPARE(noHash.first().id, fid2);
}

void DatabaseTest::testGetUnprocessedFiles()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");

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

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    const QString newPath = "/roms/organized/mario.nes";
    QVERIFY(db.updateFilePath(fileId, newPath));

    FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.currentPath, newPath);
}

QTEST_MAIN(DatabaseTest)
#include "test_database.moc"
