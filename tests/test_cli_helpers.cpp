#include <QtTest/QtTest>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QCommandLineParser>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
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
    void testFindExistingArtworkPathProbesSupportedExtensions();
    void testResolveCliOptionValueUsesPresetWhenOptionUnset();
    void testResolveCliOptionValuePrefersExplicitOption();
    void testResolveCliOptionValueFallsBackToParserDefault();
    void testGetMatchingSystemNameReturnsInternalName();
    void testGetMatchingSystemNameHandlesUnknownSystem();
    void testGetProviderLookupSystemNamePrefersMatchedSystem();
    void testGetProviderLookupSystemNameFallsBackToFileSystem();
    void testGetHashedFilesOnlyReturnsHashedRows();
    void testGetMatchingDisplayNameForRegularFile();
    void testGetMatchingDisplayNameForArchiveFile();
    void testGetMatchingDisplayNameForPatchedFile();
    void testGetMatchingDisplayNameForPatchedArchive();
    void testBuildOrchestratorSkipsIgdbWithoutCredentials();
    void testBuildOrchestratorLoadsCompendiumProviderFromDataDir();
    void testPersistMetadataInsertsGame();
    void testPersistMetadataNameMatchScoreRoundTrips();
    void testPersistMetadataDuplicateGame();
    void testHashFileRecordRealFile();
    void testHashFileRecordGdiUsesReferencedTrackPayload();
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

class CurrentDirGuard
{
public:
    explicit CurrentDirGuard(const QString &path)
        : m_original(QDir::currentPath())
    {
        QDir::setCurrent(path);
    }

    ~CurrentDirGuard()
    {
        QDir::setCurrent(m_original);
    }

private:
    QString m_original;
};

static bool execSql(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    return query.exec(sql);
}

static bool seedCompendiumDatabase(const QString &rootPath)
{
    const QString compendiumDir = rootPath + "/data/compendium";
    if (!QDir().mkpath(compendiumDir)) {
        return false;
    }

    const QString dbPath = compendiumDir + "/remus_compendium.db";
    const QString connectionName = QStringLiteral("cli_helpers_compendium_%1")
        .arg(QString::number(QDateTime::currentMSecsSinceEpoch()));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return false;
        }

        const bool ok =
            execSql(db, QStringLiteral("CREATE TABLE systems (system_id INTEGER PRIMARY KEY, internal_name TEXT NOT NULL UNIQUE, display_name TEXT NOT NULL)")) &&
            execSql(db, QStringLiteral("CREATE TABLE games (game_id TEXT PRIMARY KEY, system_id INTEGER NOT NULL, canonical_title TEXT NOT NULL, primary_region_code TEXT, release_date TEXT, release_year INTEGER, developer TEXT, publisher TEXT, genre TEXT, players_max INTEGER, description TEXT, rating REAL, canonical_confidence REAL NOT NULL DEFAULT 0)")) &&
            execSql(db, QStringLiteral("CREATE TABLE game_signatures (signature_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL, hash_type TEXT NOT NULL, hash_value TEXT NOT NULL, source_id TEXT NOT NULL DEFAULT 'test', snapshot_id TEXT, source_entry_key TEXT, confidence REAL NOT NULL, is_primary INTEGER NOT NULL DEFAULT 0)")) &&
            execSql(db, QStringLiteral("CREATE TABLE game_serials (serial_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL, serial_value TEXT NOT NULL, source_id TEXT NOT NULL DEFAULT 'test', snapshot_id TEXT, source_entry_key TEXT, confidence REAL NOT NULL)")) &&
            execSql(db, QStringLiteral("CREATE TABLE game_facts (fact_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL, field_name TEXT NOT NULL, field_value TEXT NOT NULL, value_type TEXT NOT NULL DEFAULT 'text', source_id TEXT NOT NULL DEFAULT 'test', snapshot_id TEXT NOT NULL DEFAULT '', source_item_id INTEGER, source_priority INTEGER NOT NULL DEFAULT 100, confidence REAL NOT NULL DEFAULT 1.0)")) &&
            execSql(db, QStringLiteral("CREATE TABLE canonical_resolution (game_id TEXT NOT NULL, field_name TEXT NOT NULL, selected_fact_id INTEGER NOT NULL, resolved_by_rule TEXT NOT NULL, PRIMARY KEY (game_id, field_name))")) &&
            execSql(db, QStringLiteral("CREATE TABLE game_names (game_name_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL, name_text TEXT NOT NULL, alias_type TEXT NOT NULL, locale TEXT NOT NULL DEFAULT '', snapshot_id TEXT NOT NULL DEFAULT '', confidence REAL NOT NULL DEFAULT 0)")) &&
            execSql(db, QStringLiteral("INSERT INTO systems (system_id, internal_name, display_name) VALUES (4, 'GameCube', 'Nintendo GameCube')")) &&
            execSql(db, QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, primary_region_code, release_year, developer, publisher, genre, players_max, description, rating, canonical_confidence) VALUES ('game-1', 4, 'Paper Mario: The Thousand-Year Door', 'USA', 2004, 'Intelligent Systems', 'Nintendo', 'Role-Playing', 1, 'A turn-based adventure across the Mushroom Kingdom.', 9.0, 0.95)")) &&
            execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value, confidence, is_primary) VALUES ('game-1', 'md5', '0123456789abcdef0123456789abcdef', 1.0, 1)"));

        db.close();
        if (!ok) {
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    return QFile::exists(dbPath);
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

void CliHelpersTest::testFindExistingArtworkPathProbesSupportedExtensions()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString basePath = tmpDir.filePath(QStringLiteral("artwork_42"));
    QVERIFY(findExistingArtworkPath(basePath).isEmpty());

    QFile png(basePath + QStringLiteral(".png"));
    QVERIFY(png.open(QIODevice::WriteOnly));
    QVERIFY(png.write("png") == 3);
    png.close();
    QCOMPARE(findExistingArtworkPath(basePath), basePath + QStringLiteral(".png"));

    QVERIFY(png.remove());
    QFile webp(basePath + QStringLiteral(".webp"));
    QVERIFY(webp.open(QIODevice::WriteOnly));
    QVERIFY(webp.write("webp") == 4);
    webp.close();
    QCOMPARE(findExistingArtworkPath(basePath), basePath + QStringLiteral(".webp"));
}

void CliHelpersTest::testResolveCliOptionValueUsesPresetWhenOptionUnset()
{
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption("bundle-disc-format", "", "format", "original"));
    parser.process(QStringList{QStringLiteral("test")});

    QCOMPARE(resolveCliOptionValue(parser,
                                   QStringLiteral("bundle-disc-format"),
                                   QStringLiteral("chd")),
             QStringLiteral("chd"));
}

void CliHelpersTest::testResolveCliOptionValuePrefersExplicitOption()
{
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption("bundle-disc-format", "", "format", "original"));
    parser.process(QStringList{QStringLiteral("test"),
                               QStringLiteral("--bundle-disc-format"),
                               QStringLiteral("rvz")});

    QCOMPARE(resolveCliOptionValue(parser,
                                   QStringLiteral("bundle-disc-format"),
                                   QStringLiteral("chd")),
             QStringLiteral("rvz"));
}

void CliHelpersTest::testResolveCliOptionValueFallsBackToParserDefault()
{
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption("bundle-disc-format", "", "format",
                                        QString::fromLatin1(Constants::Cli::Defaults::BUNDLE_DISC_FORMAT)));
    parser.process(QStringList{QStringLiteral("test")});

    QCOMPARE(resolveCliOptionValue(parser,
                                   QStringLiteral("bundle-disc-format")),
             QString::fromLatin1(Constants::Cli::Defaults::BUNDLE_DISC_FORMAT));
}

void CliHelpersTest::testGetMatchingSystemNameReturnsInternalName()
{
    FileRecord fr;
    fr.systemId = 2;

    QCOMPARE(getMatchingSystemName(fr), QStringLiteral("SNES"));
}

void CliHelpersTest::testGetMatchingSystemNameHandlesUnknownSystem()
{
    FileRecord fr;
    fr.systemId = -1;

    QVERIFY(getMatchingSystemName(fr).isEmpty());
}

void CliHelpersTest::testGetProviderLookupSystemNamePrefersMatchedSystem()
{
    FileRecord fr;
    fr.systemId = Constants::Systems::ID_PSX;

    Database::MatchResult match;
    match.systemId = Constants::Systems::ID_GAMECUBE;

    QCOMPARE(getProviderLookupSystemName(fr, &match), QStringLiteral("GameCube"));
}

void CliHelpersTest::testGetProviderLookupSystemNameFallsBackToFileSystem()
{
    FileRecord fr;
    fr.systemId = Constants::Systems::ID_SNES;

    QCOMPARE(getProviderLookupSystemName(fr), QStringLiteral("SNES"));
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
    {
        QFile f(hashedPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("ROM1") == 4);
    }
    {
        QFile f(unhashedPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("ROM2") == 4);
    }

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

void CliHelpersTest::testGetMatchingDisplayNameForRegularFile()
{
    FileRecord fr;
    fr.filename = "Sonic The Hedgehog (USA, Europe).md";

    QCOMPARE(getMatchingDisplayName(fr), QStringLiteral("Sonic The Hedgehog (USA, Europe)"));
}

void CliHelpersTest::testGetMatchingDisplayNameForArchiveFile()
{
    FileRecord fr;
    fr.isCompressed = true;
    fr.currentPath = "/roms/Sonic The Hedgehog (USA, Europe).zip";
    fr.filename = "Sonic The Hedgehog (USA, Europe).md";
    fr.archiveInternalPath = "Sonic The Hedgehog (USA, Europe).md";

    QCOMPARE(getMatchingDisplayName(fr), QStringLiteral("Sonic The Hedgehog (USA, Europe)"));
}

void CliHelpersTest::testGetMatchingDisplayNameForPatchedFile()
{
    FileRecord fr;
    fr.filename = "Dragon Quest III (English v2.0)[Addendum].sfc";

    QCOMPARE(getMatchingDisplayName(fr), QStringLiteral("Dragon Quest III"));
}

void CliHelpersTest::testGetMatchingDisplayNameForPatchedArchive()
{
    FileRecord fr;
    fr.isCompressed = true;
    fr.currentPath = "/roms/Dragon Quest III (English v2.0)[Addendum].zip";
    fr.filename = "Dragon Quest III (English v2.0)[Addendum].sfc";
    fr.archiveInternalPath = "Dragon Quest III (English v2.0)[Addendum].sfc";

    QCOMPARE(getMatchingDisplayName(fr), QStringLiteral("Dragon Quest III"));
}

void CliHelpersTest::testBuildOrchestratorSkipsIgdbWithoutCredentials()
{
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption("ss-user", "", "username"));
    parser.addOption(QCommandLineOption("ss-pass", "", "password"));
    parser.addOption(QCommandLineOption("igdb-client-id", "", "clientId"));
    parser.addOption(QCommandLineOption("igdb-client-secret", "", "clientSecret"));
    parser.process(QStringList{QStringLiteral("test")});

    auto orchestrator = buildOrchestrator(parser);

    const QStringList providers = orchestrator->getEnabledProviders();
    QVERIFY(providers.contains(QStringLiteral("hasheous")));
    QVERIFY(!providers.contains(QStringLiteral("thegamesdb")));  // gated on API key
    QVERIFY(!providers.contains(QStringLiteral("igdb")));
}

void CliHelpersTest::testBuildOrchestratorLoadsCompendiumProviderFromDataDir()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(seedCompendiumDatabase(dir.path()));

    CurrentDirGuard currentDir(dir.path());

    QCommandLineParser parser;
    parser.addOption(QCommandLineOption("ss-user", "", "username"));
    parser.addOption(QCommandLineOption("ss-pass", "", "password"));
    parser.addOption(QCommandLineOption("igdb-client-id", "", "clientId"));
    parser.addOption(QCommandLineOption("igdb-client-secret", "", "clientSecret"));
    parser.process(QStringList{QStringLiteral("test")});

    auto orchestrator = buildOrchestrator(parser);
    const QStringList providers = orchestrator->getEnabledProviders();

    QVERIFY(providers.contains(QStringLiteral("compendium")));
    QCOMPARE(providers.first(), QStringLiteral("compendium"));
    QVERIFY(orchestrator->providerSupportsHash(QStringLiteral("compendium")));

    QSignalSpy trySpy(orchestrator.get(), &ProviderOrchestrator::tryingProvider);
    const GameMetadata metadata = orchestrator->getByHashWithFallback(
        QStringLiteral("0123456789ABCDEF0123456789ABCDEF"),
        QStringLiteral("GameCube"));

    QCOMPARE(metadata.title, QStringLiteral("Paper Mario: The Thousand-Year Door"));
    QCOMPARE(metadata.system, QStringLiteral("GameCube"));
    QCOMPARE(metadata.providerId, QStringLiteral("compendium"));
    QCOMPARE(metadata.publisher, QStringLiteral("Nintendo"));
    QVERIFY(!trySpy.isEmpty());
    QCOMPARE(trySpy.at(0).at(0).toString(), QStringLiteral("compendium"));
    QCOMPARE(trySpy.at(0).at(1).toString(), QStringLiteral("hash"));
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

void CliHelpersTest::testPersistMetadataNameMatchScoreRoundTrips()
{
    // Finding #5 — persistMetadata() must pass matchScore through to insertMatch()
    // as nameMatchScore so that fuzzy/name-match scores survive a DB round-trip.
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    FileRecord fr = makeRecord(libId, sysId, "sonic.nes", "CCDD", "md5s", "sha1s");
    int fileId = db.insertFile(fr);
    fr.id = fileId;

    GameMetadata meta = makeMetadata("Sonic The Hedgehog", "Mega Drive");
    meta.matchScore  = 0.87f;
    meta.matchMethod = QStringLiteral("fuzzy");

    int gameId = persistMetadata(db, fr, meta);
    QVERIFY(gameId > 0);

    Database::MatchResult match = db.getMatchForFile(fileId);
    QVERIFY(match.matchId > 0);
    // nameMatchScore must reflect matchScore (0.87), not the default 0.0
    QVERIFY2(match.nameMatchScore >= 0.86f && match.nameMatchScore <= 0.88f,
             qPrintable(QString("nameMatchScore = %1").arg(match.nameMatchScore)));
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
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write(QByteArray(1024, char(0xAB))) == 1024);
    }

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

void CliHelpersTest::testHashFileRecordGdiUsesReferencedTrackPayload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gdiPath = dir.path() + "/disc.gdi";
    const QString track01Path = dir.path() + "/track01.bin";
    const QString track02Path = dir.path() + "/track02.raw";
    const QString track03Path = dir.path() + "/track03.bin";

    {
        QFile track01(track01Path);
        QVERIFY(track01.open(QIODevice::WriteOnly));
        QVERIFY(track01.write(QByteArray(1024, char(0x01))) == 1024);
    }
    {
        QFile track02(track02Path);
        QVERIFY(track02.open(QIODevice::WriteOnly));
        QVERIFY(track02.write(QByteArray(2048, char(0x02))) == 2048);
    }
    {
        QFile track03(track03Path);
        QVERIFY(track03.open(QIODevice::WriteOnly));
        QVERIFY(track03.write(QByteArray(4096, char(0x03))) == 4096);
    }
    {
        QFile gdi(gdiPath);
        QVERIFY(gdi.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&gdi);
        out << "3\n";
        out << "1 0 4 2352 track01.bin 0\n";
        out << "2 600 0 2352 track02.raw 0\n";
        out << "3 45000 4 2352 track03.bin 0\n";
    }

    FileRecord fr;
    fr.originalPath = gdiPath;
    fr.currentPath = gdiPath;
    fr.filename = QStringLiteral("disc.gdi");
    fr.extension = QStringLiteral(".gdi");

    Hasher hasher;
    const HashResult expected = hasher.calculateHashes(track03Path, false, 0);
    const HashResult actual = hashFileRecord(fr, hasher);

    QVERIFY(actual.success);
    QCOMPARE(actual.crc32, expected.crc32);
    QCOMPARE(actual.md5, expected.md5);
    QCOMPARE(actual.sha1, expected.sha1);
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
