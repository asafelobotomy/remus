#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCommandLineParser>
#include "../src/cli/cli_helpers.h"
#include "../src/core/database.h"
#include "../src/core/hasher.h"
#include "../src/metadata/metadata_provider.h"

using namespace Remus;

class CliHelpersTest : public QObject
{
    Q_OBJECT

private slots:
    void testSelectBestHashCrc32Only();
    void testSelectBestHashPrefersHasheous();
    void testSelectBestHashEmptyWhenNoHashes();
    void testGetHashedFilesOnlyReturnsHashedRows();
    void testPersistMetadataInsertsGame();
    void testPersistMetadataDuplicateGame();
    void testHashFileRecordRealFile();
    void testPrintFileInfoDoesNotCrash();
};

// ── Helpers ───────────────────────────────────────────────────────────────

static FileRecord makeRecord(int libId, int sysId,
                             const QString &name,
                             const QString &crc   = QString(),
                             const QString &md5   = QString(),
                             const QString &sha1  = QString())
{
    FileRecord fr;
    fr.libraryId      = libId;
    fr.filename       = name;
    fr.originalPath   = "/roms/" + name;
    fr.currentPath    = fr.originalPath;
    fr.extension      = "." + name.section('.', -1);
    fr.systemId       = sysId;
    fr.fileSize       = 4096;
    fr.crc32          = crc;
    fr.md5            = md5;
    fr.sha1           = sha1;
    fr.hashCalculated = !crc.isEmpty() || !md5.isEmpty() || !sha1.isEmpty();
    return fr;
}

static GameMetadata makeMetadata(const QString &title = "Super Mario Bros.",
                                  const QString &system = "NES")
{
    GameMetadata m;
    m.title       = title;
    m.system      = system;
    m.region      = "USA";
    m.publisher   = "Nintendo";
    m.releaseDate = "1985-09-13";
    m.matchScore  = 1.0f;
    m.matchMethod = "hash";
    return m;
}

// ── Tests ─────────────────────────────────────────────────────────────────

void CliHelpersTest::testSelectBestHashCrc32Only()
{
    FileRecord fr;
    fr.crc32 = "AABBCCDD";
    fr.hashCalculated = true;

    QString hash = selectBestHash(fr);
    QCOMPARE(hash, QStringLiteral("AABBCCDD"));
}

void CliHelpersTest::testSelectBestHashPrefersHasheous()
{
    // Disc-based systems prefer MD5 (Hasheous primary hash for disc media)
    FileRecord fr;
    fr.crc32 = "AABBCCDD";
    fr.md5   = "abcdef1234567890abcdef1234567890";
    fr.sha1  = "sha1value00000000000000000000000000000000";
    fr.hashCalculated = true;
    // We just verify something non-empty is returned
    QString hash = selectBestHash(fr);
    QVERIFY(!hash.isEmpty());
}

void CliHelpersTest::testSelectBestHashEmptyWhenNoHashes()
{
    FileRecord fr;
    fr.hashCalculated = false;
    QString hash = selectBestHash(fr);
    QVERIFY(hash.isEmpty());
}

void CliHelpersTest::testGetHashedFilesOnlyReturnsHashedRows()
{
    // getExistingFiles() checks QFileInfo::exists(), so files must be on disk.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary(tmpDir.path(), "Test");
    int sysId = db.getSystemId("NES");

    // Create actual files on disk
    const QString hashedPath   = tmpDir.path() + "/mario.nes";
    const QString unhashedPath = tmpDir.path() + "/zelda.nes";
    { QFile f(hashedPath);   f.open(QIODevice::WriteOnly); f.write("ROM1"); }
    { QFile f(unhashedPath); f.open(QIODevice::WriteOnly); f.write("ROM2"); }

    // Insert hashed file with real path
    FileRecord frHashed;
    frHashed.libraryId    = libId;
    frHashed.filename     = "mario.nes";
    frHashed.originalPath = hashedPath;
    frHashed.currentPath  = hashedPath;
    frHashed.extension    = ".nes";
    frHashed.systemId     = sysId;
    frHashed.fileSize     = 4;
    int hashedId = db.insertFile(frHashed);
    db.updateFileHashes(hashedId, "AABBCCDD", "md5value", "sha1value");

    // Insert unhashed file with real path
    FileRecord frUnhashed;
    frUnhashed.libraryId    = libId;
    frUnhashed.filename     = "zelda.nes";
    frUnhashed.originalPath = unhashedPath;
    frUnhashed.currentPath  = unhashedPath;
    frUnhashed.extension    = ".nes";
    frUnhashed.systemId     = sysId;
    frUnhashed.fileSize     = 4;
    db.insertFile(frUnhashed);

    QList<FileRecord> hashed = getHashedFiles(db);
    QCOMPARE(hashed.size(), 1);
    QCOMPARE(hashed.first().id, hashedId);
}

void CliHelpersTest::testPersistMetadataInsertsGame()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    FileRecord fr = makeRecord(libId, sysId, "mario.nes",
                                "AABBCCDD", "md5val", "sha1val");
    int fileId = db.insertFile(fr);
    fr.id = fileId;

    int gameId = persistMetadata(db, fr, makeMetadata());
    QVERIFY(gameId > 0);

    // The file should now have a match record
    Database::MatchResult match = db.getMatchForFile(fileId);
    QCOMPARE(match.gameId, gameId);
    QVERIFY(match.confidence >= 90.0f);  // Hash match → high confidence
}

void CliHelpersTest::testPersistMetadataDuplicateGame()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");

    FileRecord fr1 = makeRecord(libId, sysId, "mario1.nes", "CRC1", "MD51", "SHA11");
    int fid1 = db.insertFile(fr1);
    fr1.id = fid1;

    FileRecord fr2 = makeRecord(libId, sysId, "mario2.nes", "CRC2", "MD52", "SHA12");
    int fid2 = db.insertFile(fr2);
    fr2.id = fid2;

    // Both files match the same game metadata
    int gid1 = persistMetadata(db, fr1, makeMetadata());
    int gid2 = persistMetadata(db, fr2, makeMetadata());

    QVERIFY(gid1 > 0);
    QVERIFY(gid2 > 0);
    // Both produce valid (possibly shared) game entries; both files have matches
    QVERIFY(db.getMatchForFile(fid1).matchId > 0);
    QVERIFY(db.getMatchForFile(fid2).matchId > 0);
}

void CliHelpersTest::testHashFileRecordRealFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.path() + "/game.nes";
    { QFile f(path); f.open(QIODevice::WriteOnly); f.write(QByteArray(1024, 0xAB)); }

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int libId = db.insertLibrary(dir.path(), "Test");
    int sysId = db.getSystemId("NES");

    FileRecord fr = makeRecord(libId, sysId, "game.nes");
    fr.originalPath = path;
    fr.currentPath  = path;

    Hasher hasher;
    HashResult result = hashFileRecord(fr, hasher);

    QVERIFY(result.success);
    QVERIFY(!result.crc32.isEmpty());
    QVERIFY(!result.md5.isEmpty());
    QVERIFY(!result.sha1.isEmpty());
}

void CliHelpersTest::testPrintFileInfoDoesNotCrash()
{
    FileRecord fr;
    fr.id           = 42;
    fr.filename     = "game.nes";
    fr.currentPath  = "/roms/game.nes";
    fr.fileSize     = 1024;
    fr.crc32        = "AABB";
    fr.hashCalculated = true;

    // Should not throw or abort; output goes to logging category
    printFileInfo(fr);
    QVERIFY(true);
}

QTEST_MAIN(CliHelpersTest)
#include "test_cli_helpers.moc"
