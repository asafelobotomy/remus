#include <QSqlError>
#include <QTemporaryDir>
#include <QUuid>

#include "test_database_fixture.h"

FileRecord makeRecord(int libId, int sysId, const QString &name)
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

bool execSql(QSqlQuery &query, const QString &sql)
{
    if (!query.exec(sql)) {
        qWarning() << "SQL failed:" << sql << query.lastError().text();
        return false;
    }

    return true;
}

bool createLegacyDatabaseWithBrokenAppliedPatches(const QString &dbPath)
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

bool tableHasColumn(const QString &dbPath, const QString &tableName, const QString &columnName)
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

void DatabaseTest::testInitializeInMemory()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));
}

void DatabaseTest::testInitializeEnablesForeignKeysOnExistingDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath("foreign_keys.db");

    {
        Database db;
        QVERIFY(db.initialize(dbPath));
    }

    Database reopened;
    QVERIFY(reopened.initialize(dbPath));

    QSqlQuery pragma(reopened.database());
    QVERIFY(pragma.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(pragma.next());
    QCOMPARE(pragma.value(0).toInt(), 1);
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
        const int legacyGenesisId =
            Constants::DatabaseSchema::Migrations::LEGACY_SYSTEM_SLOT_OFFSET * 2 + genesisId;

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
        insertLegacy.addBindValue(legacyGenesisId);
        insertLegacy.addBindValue(QStringLiteral("Genesis"));
        insertLegacy.addBindValue(QStringLiteral("Sega Genesis / Mega Drive"));
        insertLegacy.addBindValue(QStringLiteral("Sega"));
        insertLegacy.addBindValue(4);
        insertLegacy.addBindValue(QStringLiteral(".md,.gen,.smd,.bin,.32x,.68k"));
        insertLegacy.addBindValue(QStringLiteral("CRC32"));
        QVERIFY(insertLegacy.exec());

        QSqlQuery corruptFile(db.database());
        corruptFile.prepare("UPDATE files SET system_id = ? WHERE id = ?");
        corruptFile.addBindValue(legacyGenesisId);
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

QTEST_MAIN(DatabaseTest)
