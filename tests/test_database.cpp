#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include "../src/core/database.h"
#include "../src/core/constants/constants.h"

using namespace Remus;

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitializeInMemory();
    void testInsertAndGetFile();
    void testSystemIdsRemainStableAcrossReopen();
    void testInitializeRepairsDanglingSystemIds();
    void testInitializeRollsBackFailedMigrations();
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
    void testInsertAndGetPatchedFileMetadata();
    void testInsertAndFindAppliedPatch();
    void testUpdateFileHashesPromotesPatchedMetadata();

    // Phase 0 characterization tests — safety net for Phase 1 Database split
    void testDeleteFilesForLibrary();
    void testGetAllFilesIncludesStaleEntries();
    void testGetExistingFilesOnlyReturnsValidPaths();
    void testGetFilePath();
    void testUpdateFileOriginalPath();
    void testGetFilesByParent();
    void testGetAllMatches();
    void testInsertAndGetModInstallation();
    void testRemoveModInstallation();
    void testUpsertAndGetCatalogCache();
    void testDeleteLibraryCascadesFiles();
    void testInsertMatchWithNameMatchScore();
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

static bool execSql(QSqlQuery &query, const QString &sql)
{
    if (!query.exec(sql)) {
        qWarning() << "SQL failed:" << sql << query.lastError().text();
        return false;
    }

    return true;
}

static bool createLegacyDatabaseWithBrokenAppliedPatches(const QString &dbPath)
{
    const QString connectionName =
        QStringLiteral("legacy-schema-") + QUuid::createUuid().toString(QUuid::Id128);
    bool ok = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return false;
        }

        QSqlQuery query(db);
        ok =
            execSql(query, QStringLiteral(R"(
                CREATE TABLE systems (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    name TEXT NOT NULL UNIQUE,
                    display_name TEXT NOT NULL,
                    manufacturer TEXT,
                    generation INTEGER,
                    extensions TEXT NOT NULL,
                    preferred_hash TEXT NOT NULL,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            )")) &&
            execSql(query, QStringLiteral(R"(
                CREATE TABLE files (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    library_id INTEGER NOT NULL,
                    original_path TEXT NOT NULL,
                    current_path TEXT NOT NULL,
                    filename TEXT NOT NULL,
                    extension TEXT NOT NULL,
                    file_size INTEGER NOT NULL,
                    system_id INTEGER,
                    crc32 TEXT,
                    md5 TEXT,
                    sha1 TEXT,
                    hash_calculated BOOLEAN DEFAULT 0,
                    is_primary BOOLEAN DEFAULT 1,
                    parent_file_id INTEGER,
                    last_modified TIMESTAMP,
                    scanned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            )")) &&
            execSql(query, QStringLiteral(R"(
                CREATE TABLE matches (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    file_id INTEGER NOT NULL,
                    game_id INTEGER NOT NULL,
                    match_method TEXT NOT NULL,
                    confidence REAL NOT NULL,
                    is_confirmed BOOLEAN DEFAULT 0,
                    is_rejected BOOLEAN DEFAULT 0,
                    matched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    UNIQUE(file_id, game_id)
                )
            )")) &&
            execSql(query, QStringLiteral("CREATE VIEW applied_patches AS SELECT 1 AS id"));

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return ok;
}

static bool tableHasColumn(const QString &dbPath, const QString &tableName, const QString &columnName)
{
    const QString connectionName =
        QStringLiteral("inspect-schema-") + QUuid::createUuid().toString(QUuid::Id128);
    bool found = false;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return false;
        }

        QSqlQuery query(db);
        if (query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
            while (query.next()) {
                if (query.value(1).toString() == columnName) {
                    found = true;
                    break;
                }
            }
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return found;
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

void DatabaseTest::testSystemIdsRemainStableAcrossReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath("stable_systems.db");

    int fileId = 0;
    int genesisId = 0;

    {
        Database db;
        QVERIFY(db.initialize(dbPath));

        genesisId = db.getSystemId("Genesis");
        QCOMPARE(genesisId, 10);

        const int libId = db.insertLibrary("/roms", "Test");
        QVERIFY(libId > 0);

        FileRecord fr = makeRecord(libId, genesisId, "sonic.md");
        fileId = db.insertFile(fr);
        QVERIFY(fileId > 0);
    }

    {
        Database db;
        QVERIFY(db.initialize(dbPath));

        SystemDetector detector;
        for (const QString &name : Remus::Constants::Systems::getSystemInternalNames()) {
            const SystemInfo info = detector.getSystemInfo(name);
            if (!info.name.isEmpty()) {
                const int insertedId = db.insertSystem(info);
                QVERIFY(insertedId > 0);
            }
        }

        QCOMPARE(db.getSystemId("Genesis"), genesisId);

        const FileRecord got = db.getFileById(fileId);
        QCOMPARE(got.systemId, genesisId);

        const QMap<QString, int> counts = db.getFileCountBySystem();
        QCOMPARE(counts.value("Genesis"), 1);
        QCOMPARE(db.getSystemDisplayName(genesisId), QStringLiteral("Sega Genesis / Mega Drive"));
    }
}

void DatabaseTest::testInitializeRepairsDanglingSystemIds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath("repair_systems.db");
    int fileId = 0;

    {
        Database db;
        QVERIFY(db.initialize(dbPath));

        const int genesisId = db.getSystemId("Genesis");
        QCOMPARE(genesisId, 10);

        const int libId = db.insertLibrary("/roms", "Test");
        QVERIFY(libId > 0);

        FileRecord fr = makeRecord(libId, genesisId, "sonic.md");
        fileId = db.insertFile(fr);
        QVERIFY(fileId > 0);

        QSqlQuery renameCanonical(db.database());
        renameCanonical.prepare("UPDATE systems SET name = ? WHERE id = ?");
        renameCanonical.addBindValue(QStringLiteral("Genesis__legacy_10"));
        renameCanonical.addBindValue(10);
        QVERIFY(renameCanonical.exec());

        QSqlQuery insertLegacy(db.database());
        insertLegacy.prepare(R"(
            INSERT INTO systems
            (id, name, display_name, manufacturer, generation, extensions, preferred_hash)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )");
        insertLegacy.addBindValue(49);
        insertLegacy.addBindValue(QStringLiteral("Genesis"));
        insertLegacy.addBindValue(QStringLiteral("Sega Genesis / Mega Drive"));
        insertLegacy.addBindValue(QStringLiteral("Sega"));
        insertLegacy.addBindValue(4);
        insertLegacy.addBindValue(QStringLiteral(".md,.gen,.smd,.bin,.32x,.68k"));
        insertLegacy.addBindValue(QStringLiteral("CRC32"));
        QVERIFY(insertLegacy.exec());

        QSqlQuery corruptFile(db.database());
        corruptFile.prepare("UPDATE files SET system_id = ? WHERE id = ?");
        corruptFile.addBindValue(49);
        corruptFile.addBindValue(fileId);
        QVERIFY(corruptFile.exec());
    }

    {
        Database db;
        QVERIFY(db.initialize(dbPath));

        QCOMPARE(db.getSystemId("Genesis"), 10);

        const FileRecord repaired = db.getFileById(fileId);
        QCOMPARE(repaired.systemId, 10);

        const QMap<QString, int> counts = db.getFileCountBySystem();
        QCOMPARE(counts.value("Genesis"), 1);
    }
}

void DatabaseTest::testInitializeRollsBackFailedMigrations()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath("broken_migration.db");
    QVERIFY(createLegacyDatabaseWithBrokenAppliedPatches(dbPath));
    QVERIFY(!tableHasColumn(dbPath,
                            QStringLiteral("files"),
                            QString::fromLatin1(Constants::DatabaseSchema::Columns::Files::IS_PROCESSED)));

    Database db;
    QVERIFY(!db.initialize(dbPath));

    QVERIFY(!tableHasColumn(dbPath,
                            QStringLiteral("files"),
                            QString::fromLatin1(Constants::DatabaseSchema::Columns::Files::IS_PROCESSED)));
    QVERIFY(!tableHasColumn(dbPath,
                            QStringLiteral("files"),
                            QString::fromLatin1(Constants::DatabaseSchema::Columns::Files::PROCESSING_STATUS)));
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

void DatabaseTest::testInsertAndFindAppliedPatch()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    Database::AppliedPatchRecord record;
    record.basePath = "/roms/base.sfc";
    record.outputPath = "/roms/base [English v2.0].sfc";
    record.patchPath = "/patches/english_v2.bps";
    record.patchFormat = "BPS";
    record.baseTitle = "Base Game";
    record.patchName = "English v2.0";
    record.fileType = "translation";
    record.sourceChecksum = "11111111";
    record.targetChecksum = "22222222";
    record.patchChecksum = "33333333";
    record.baseCrc32 = "AAAA1111";
    record.baseMd5 = "base-md5";
    record.baseSha1 = "base-sha1";
    record.outputCrc32 = "BBBB2222";
    record.outputMd5 = "output-md5";
    record.outputSha1 = "output-sha1";

    QVERIFY(db.insertAppliedPatch(record));

    const Database::AppliedPatchRecord found =
        db.findAppliedPatchByOutputHashes("BBBB2222", "output-md5", "output-sha1");
    QVERIFY(found.id > 0);
    QCOMPARE(found.baseTitle, QStringLiteral("Base Game"));
    QCOMPARE(found.patchName, QStringLiteral("English v2.0"));
    QCOMPARE(found.fileType, QStringLiteral("translation"));
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

// ── Phase 0 characterization tests ─────────────────────────────────────────

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

    // Library record itself should still exist
    QVERIFY(!db.getLibraryPath(libId).isEmpty());
}

void DatabaseTest::testGetAllFilesIncludesStaleEntries()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    // Insert a file whose path doesn't exist on disk
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

    // Insert a file whose path does not exist on disk
    db.insertFile(makeRecord(libId, sysId, "nonexistent.nes"));

    QList<FileRecord> existing = db.getExistingFiles();
    // Path /roms/nonexistent.nes doesn't exist, so should be filtered out
    QCOMPARE(existing.size(), 0);
}

void DatabaseTest::testGetFilePath()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    QString path = db.getFilePath(fileId);
    QCOMPARE(path, QStringLiteral("/roms/mario.nes"));

    // Non-existent file ID returns empty
    QVERIFY(db.getFilePath(99999).isEmpty());
}

void DatabaseTest::testUpdateFileOriginalPath()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));

    const QString newOrigPath = "/extracted/mario.nes";
    QVERIFY(db.updateFileOriginalPath(fileId, newOrigPath));

    FileRecord got = db.getFileById(fileId);
    QCOMPARE(got.originalPath, newOrigPath);
}

void DatabaseTest::testGetFilesByParent()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("PlayStation");

    // Insert a parent CUE file
    FileRecord cue = makeRecord(libId, sysId, "game.cue");
    cue.isPrimary = true;
    int parentId = db.insertFile(cue);
    QVERIFY(parentId > 0);

    // Insert child BIN files
    FileRecord bin1 = makeRecord(libId, sysId, "game (Track 1).bin");
    bin1.isPrimary = false;
    bin1.parentFileId = parentId;
    int child1 = db.insertFile(bin1);
    QVERIFY(child1 > 0);

    FileRecord bin2 = makeRecord(libId, sysId, "game (Track 2).bin");
    bin2.isPrimary = false;
    bin2.parentFileId = parentId;
    int child2 = db.insertFile(bin2);
    QVERIFY(child2 > 0);

    QList<FileRecord> children = db.getFilesByParent(parentId);
    QCOMPARE(children.size(), 2);
}

void DatabaseTest::testGetAllMatches()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));

    int gid1 = db.insertGame("Super Mario Bros.", sysId);
    int gid2 = db.insertGame("Legend of Zelda", sysId);

    db.insertMatch(fid1, gid1, 100.0f, "hash");
    db.insertMatch(fid2, gid2, 85.0f, "fuzzy");

    QMap<int, Database::MatchResult> matches = db.getAllMatches();
    QCOMPARE(matches.size(), 2);
    QVERIFY(matches.contains(fid1));
    QVERIFY(matches.contains(fid2));
    QCOMPARE(matches[fid1].gameTitle, QStringLiteral("Super Mario Bros."));
    QCOMPARE(matches[fid2].matchMethod, QStringLiteral("fuzzy"));
}

void DatabaseTest::testInsertAndGetModInstallation()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int baseId = db.insertFile(makeRecord(libId, sysId, "base.nes"));
    int patchedId = db.insertFile(makeRecord(libId, sysId, "patched.nes"));

    Database::ModInstallationRecord mod;
    mod.baseFileId     = baseId;
    mod.patchedFileId  = patchedId;
    mod.catalogModId   = "mod-123";
    mod.modTitle       = "Translation Patch";
    mod.modAuthor      = "translator";
    mod.modVersion     = "1.0";
    mod.modType        = "translation";
    mod.patchFormat    = "BPS";
    int modId = db.insertModInstallation(mod);
    QVERIFY(modId > 0);

    QList<Database::ModInstallationRecord> mods = db.getModInstallations(baseId);
    QCOMPARE(mods.size(), 1);
    QCOMPARE(mods.first().modTitle, QStringLiteral("Translation Patch"));
    QCOMPARE(mods.first().catalogModId, QStringLiteral("mod-123"));
}

void DatabaseTest::testRemoveModInstallation()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int baseId = db.insertFile(makeRecord(libId, sysId, "base.nes"));
    int patchedId = db.insertFile(makeRecord(libId, sysId, "patched.nes"));

    Database::ModInstallationRecord mod;
    mod.baseFileId    = baseId;
    mod.patchedFileId = patchedId;
    mod.catalogModId  = "mod-456";
    mod.modTitle      = "Hack";
    int modId = db.insertModInstallation(mod);
    QVERIFY(modId > 0);

    QVERIFY(db.removeModInstallation(modId));
    QCOMPARE(db.getModInstallations(baseId).size(), 0);
}

void DatabaseTest::testUpsertAndGetCatalogCache()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    Database::ModCatalogCacheRecord cache;
    cache.sourceUrl = "https://example.com/catalog.json";
    cache.etag      = "abc123";
    cache.modCount  = 42;
    int cacheId = db.upsertCatalogCache(cache);
    QVERIFY(cacheId > 0);

    Database::ModCatalogCacheRecord got = db.getCatalogCache("https://example.com/catalog.json");
    QVERIFY(got.id > 0);
    QCOMPARE(got.etag, QStringLiteral("abc123"));
    QCOMPARE(got.modCount, 42);

    // Upsert with same URL updates the record
    cache.etag     = "def456";
    cache.modCount = 50;
    int updatedId = db.upsertCatalogCache(cache);
    QVERIFY(updatedId > 0);

    Database::ModCatalogCacheRecord updated = db.getCatalogCache("https://example.com/catalog.json");
    QCOMPARE(updated.etag, QStringLiteral("def456"));
    QCOMPARE(updated.modCount, 50);

    // Non-existent URL returns empty record
    Database::ModCatalogCacheRecord missing = db.getCatalogCache("https://nonexistent.example.com");
    QCOMPARE(missing.id, 0);
}

void DatabaseTest::testDeleteLibraryCascadesFiles()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/cascade", "Cascade Test");
    int sysId = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));
    QVERIFY(fid1 > 0);
    QVERIFY(fid2 > 0);

    // Verify files exist
    QCOMPARE(db.getFilesBySystem("NES").size(), 2);

    // Delete library — files should also be removed
    QVERIFY(db.deleteLibrary(libId));

    // Library path gone
    QVERIFY(db.getLibraryPath(libId).isEmpty());

    // Files should be gone too
    QCOMPARE(db.getFileById(fid1).id, 0);
    QCOMPARE(db.getFileById(fid2).id, 0);
}

void DatabaseTest::testInsertMatchWithNameMatchScore()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId  = db.insertLibrary("/roms", "Test");
    int sysId  = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int gameId = db.insertGame("Super Mario Bros.", sysId);

    QVERIFY(db.insertMatch(fileId, gameId, 85.0f, "fuzzy", 0.92f));

    Database::MatchResult m = db.getMatchForFile(fileId);
    QCOMPARE(m.matchMethod, QStringLiteral("fuzzy"));
    QVERIFY(m.nameMatchScore >= 0.91f && m.nameMatchScore <= 0.93f);
}

QTEST_MAIN(DatabaseTest)
#include "test_database.moc"
