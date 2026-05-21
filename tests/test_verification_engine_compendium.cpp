// test_verification_engine_compendium.cpp
//
// Exercises the VerificationEngine compendium path:
//   hasDat / hasPatchDat / loadDatCache / loadPatchDatCache / getMissingGames
// all driven by a minimal in-process compendium SQLite database.

#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryFile>

#include "../src/core/database.h"
#include "../src/core/verification_engine.h"

using namespace Remus;

namespace {

// ── Minimal compendium builder ────────────────────────────────────────────────
// Creates a temporary SQLite file that mirrors the compendium schema
// (only the tables VerificationEngine queries).

bool buildMinimalCompendium(const QString &path)
{
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), "compendium_build_conn");
    db.setDatabaseName(path);
    if (!db.open()) return false;

    QSqlQuery q(db);

    // systems table
    q.exec(R"(
        CREATE TABLE systems (
            system_id   INTEGER PRIMARY KEY AUTOINCREMENT,
            internal_name TEXT NOT NULL UNIQUE,
            display_name  TEXT NOT NULL,
            preferred_hash TEXT NOT NULL DEFAULT 'crc32',
            is_disc_based INTEGER NOT NULL DEFAULT 0,
            is_handheld   INTEGER NOT NULL DEFAULT 0,
            created_at    TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )");
    q.exec("INSERT INTO systems (internal_name, display_name, preferred_hash) VALUES ('NES','Nintendo Entertainment System','crc32')");

    // games table (minimal columns)
    q.exec(R"(
        CREATE TABLE games (
            game_id       TEXT PRIMARY KEY,
            system_id     INTEGER NOT NULL,
            canonical_title TEXT NOT NULL,
            FOREIGN KEY (system_id) REFERENCES systems(system_id)
        )
    )");
    q.exec("INSERT INTO games VALUES ('nes-smb','1','Super Mario Bros.')");
    q.exec("INSERT INTO games VALUES ('nes-dk', '1','Donkey Kong')");

    // sources table (required for FK)
    q.exec(R"(
        CREATE TABLE sources (
            source_id TEXT PRIMARY KEY,
            display_name TEXT NOT NULL,
            source_type TEXT NOT NULL,
            priority INTEGER NOT NULL DEFAULT 100,
            enabled  INTEGER NOT NULL DEFAULT 1
        )
    )");
    q.exec("INSERT INTO sources VALUES ('no-intro','No-Intro','dat',100,1)");

    // game_signatures
    q.exec(R"(
        CREATE TABLE game_signatures (
            signature_id INTEGER PRIMARY KEY AUTOINCREMENT,
            game_id    TEXT NOT NULL,
            hash_type  TEXT NOT NULL,
            hash_value TEXT NOT NULL,
            source_id  TEXT NOT NULL,
            confidence REAL NOT NULL DEFAULT 1.0,
            is_primary INTEGER NOT NULL DEFAULT 0,
            UNIQUE (hash_type, hash_value)
        )
    )");
    // Super Mario Bros. — matches populateDb defaults in the runtime fixture
    q.exec("INSERT INTO game_signatures(game_id,hash_type,hash_value,source_id) VALUES('nes-smb','crc32','7b5e9e81','no-intro')");
    q.exec("INSERT INTO game_signatures(game_id,hash_type,hash_value,source_id) VALUES('nes-smb','md5','811b027eaf99c2def7b933c5208636de','no-intro')");
    q.exec("INSERT INTO game_signatures(game_id,hash_type,hash_value,source_id) VALUES('nes-smb','sha1','ea343f4e445a9050d4b4fbac2c77d0693b1d0922','no-intro')");
    // Donkey Kong — deliberately absent from the library
    q.exec("INSERT INTO game_signatures(game_id,hash_type,hash_value,source_id) VALUES('nes-dk','crc32','deadbeef','no-intro')");
    q.exec("INSERT INTO game_signatures(game_id,hash_type,hash_value,source_id) VALUES('nes-dk','md5','00000000000000000000000000000001','no-intro')");

    // patch_catalog_sources + patch_entries
    q.exec(R"(
        CREATE TABLE patch_catalog_sources (
            source_id   INTEGER PRIMARY KEY AUTOINCREMENT,
            system_name TEXT NOT NULL,
            catalog_name    TEXT NOT NULL,
            catalog_version TEXT,
            catalog_source  TEXT,
            catalog_description TEXT,
            entry_count INTEGER NOT NULL DEFAULT 0,
            created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE (system_name, catalog_name)
        )
    )");
    q.exec("INSERT INTO patch_catalog_sources(system_name,catalog_name,catalog_version,catalog_source,entry_count) VALUES('NES','NES Patches','20260101','community',1)");

    q.exec(R"(
        CREATE TABLE patch_entries (
            entry_id  INTEGER PRIMARY KEY AUTOINCREMENT,
            source_id INTEGER NOT NULL,
            game_name TEXT NOT NULL,
            rom_name  TEXT NOT NULL,
            rom_size  INTEGER,
            crc32     TEXT,
            md5       TEXT,
            sha1      TEXT,
            sha256    TEXT,
            description TEXT,
            status    TEXT,
            base_title TEXT,
            patch_name TEXT,
            file_type  TEXT,
            FOREIGN KEY (source_id) REFERENCES patch_catalog_sources(source_id)
        )
    )");
    q.exec(R"(INSERT INTO patch_entries(source_id,game_name,rom_name,crc32,md5,sha1,base_title,patch_name,file_type)
              VALUES(1,'Dragon Quest III (English)','dq3en.nes','1a2b3c4d','11111111111111111111111111111111','2222222222222222222222222222222222222222','Dragon Quest III','English v2.0','translation'))");

    db.close();
    QSqlDatabase::removeDatabase("compendium_build_conn");
    return true;
}

// Populate the runtime DB with one NES file that has known hashes
int populateRuntimeDb(Database &db,
                      const QString &crc,
                      const QString &md5  = QString(),
                      const QString &sha1 = QString(),
                      bool hashCalculated = true)
{
    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId("NES");

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
    const int fileId = db.insertFile(fr);
    if (hashCalculated && !(crc.isEmpty() && md5.isEmpty() && sha1.isEmpty())) {
        db.updateFileHashes(fileId, crc, md5, sha1);
    }
    return fileId;
}

} // namespace

class VerificationEngineCompendiumTest : public QObject
{
    Q_OBJECT

private:
    // Per-test compendium DB file
    QTemporaryFile m_compendiumFile;
    QString m_compendiumPath;

    void setupCompendium()
    {
        m_compendiumFile.setFileTemplate(QDir::tempPath() + "/remus_test_compendium_XXXXXX.db");
        m_compendiumFile.setAutoRemove(true);
        QVERIFY(m_compendiumFile.open());
        m_compendiumPath = m_compendiumFile.fileName();
        m_compendiumFile.close();
        QVERIFY(buildMinimalCompendium(m_compendiumPath));
    }

private slots:

    // hasDat returns true when compendium has signatures for that system
    void testHasDatFromCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        QVERIFY(engine.hasDat("NES"));
        QVERIFY(!engine.hasDat("SNES"));    // not in compendium
    }

    // hasPatchDat returns true when compendium has patch_catalog_sources for that system
    void testHasPatchDatFromCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        QVERIFY(engine.hasPatchDat("NES"));
        QVERIFY(!engine.hasPatchDat("SNES"));
    }

    // verifyFile succeeds when hash matches a compendium game_signature entry
    void testVerifyMatchingHashFromCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));
        const int fileId = populateRuntimeDb(db,
                                             "7b5e9e81",
                                             "811b027eaf99c2def7b933c5208636de",
                                             "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        const VerificationResult result = engine.verifyFile(fileId);
        QCOMPARE(result.status, VerificationStatus::Verified);
        QVERIFY(result.notes.contains("official DAT"));
    }

    // verifyFile returns NotInDat when hash is not in compendium
    void testVerifyHashNotInCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));
        const int fileId = populateRuntimeDb(db, "cafebabe");

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        const VerificationResult result = engine.verifyFile(fileId);
        QCOMPARE(result.status, VerificationStatus::NotInDat);
    }

    // verifyFile matches a patch catalog entry when official DAT has no match
    void testVerifyPatchHashFromCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));
        // Patch hash — in patch_entries, not game_signatures
        const int fileId = populateRuntimeDb(db,
                                             "1a2b3c4d",
                                             "11111111111111111111111111111111",
                                             "2222222222222222222222222222222222222222");

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        const VerificationResult result = engine.verifyFile(fileId);
        QCOMPARE(result.status, VerificationStatus::Verified);
        QVERIFY(result.notes.contains("patch catalog"));
    }

    // getImportedPatchDats lists compendium catalog sources
    void testGetImportedPatchDatsFromCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        const auto dats = engine.getImportedPatchDats();
        QVERIFY(dats.contains("NES"));
        QCOMPARE(dats.value("NES").name, QString("NES Patches"));
    }

    // getMissingGames correctly identifies a title absent from the library
    void testGetMissingGamesFromCompendium()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));
        // Only Super Mario Bros. is in the library — Donkey Kong should be missing
        populateRuntimeDb(db,
                          "7b5e9e81",
                          "811b027eaf99c2def7b933c5208636de",
                          "ea343f4e445a9050d4b4fbac2c77d0693b1d0922");

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        const QList<DatRomEntry> missing = engine.getMissingGames("NES");
        QCOMPARE(missing.size(), 1);
        QCOMPARE(missing.first().gameName, QString("Donkey Kong"));
    }

    // getMissingGames returns empty list when all catalog titles are present
    void testGetMissingGamesNoneWhenAllPresent()
    {
        setupCompendium();

        Database db;
        QVERIFY(db.initialize(":memory:"));
        // Both games present
        populateRuntimeDb(db, "7b5e9e81");
        // Insert DK entry directly
        {
            const int libId = db.insertLibrary("/roms", "Test");
            const int sysId = db.getSystemId("NES");
            FileRecord fr;
            fr.libraryId      = libId;
            fr.filename       = "Donkey Kong (World).nes";
            fr.originalPath   = "/roms/Donkey Kong (World).nes";
            fr.currentPath    = fr.originalPath;
            fr.extension      = ".nes";
            fr.systemId       = sysId;
            fr.fileSize       = 16384;
            fr.crc32          = "deadbeef";
            fr.hashCalculated = true;
            const int fileId  = db.insertFile(fr);
            db.updateFileHashes(fileId, "deadbeef", "00000000000000000000000000000001", QString());
        }

        VerificationEngine engine(&db);
        engine.setCompendiumDb(m_compendiumPath);

        QVERIFY(engine.getMissingGames("NES").isEmpty());
    }
};

QTEST_MAIN(VerificationEngineCompendiumTest)
#include "test_verification_engine_compendium.moc"
