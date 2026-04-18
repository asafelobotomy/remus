#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "../src/core/organize_engine.h"
#include "../src/core/database.h"
#include "../src/core/constants/folder_naming.h"
#include "../src/metadata/metadata_provider.h"

using namespace Remus;

class OrganizeEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void testDryRunProducesNoFilesystemChange();
    void testMoveFile();
    void testCopyFile();
    void testCollisionSkip();
    void testCollisionRename();
    void testCollisionOverwrite();
    void testCollisionOverwritePreservesDestinationOnFailure();
    void testUndoOperation();
    void testWouldCollide();
    void testResolveCollisionSkip();
    void testResolveCollisionRename();
    void testFolderNamingNoneIsFlat();
    void testFolderNamingDefaultCreatesSubfolder();
    void testFolderNamingBatoceraGenesis();
    void testFolderNamingBatoceraSegaCD();
    void testFolderNaming3DO();
    void testFolderNamingNeoGeoCD();
    void testFolderNamingSchemeFromString();

private:
    // Write a small ROM file into dir and register it in db.
    static int makeRomFile(const QTemporaryDir &dir,
                           Database &db,
                           const QString &filename = "mario.nes")
    {
        const QString path = dir.path() + "/" + filename;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create ROM test file:" << path;
            return 0;
        }

        if (f.write("FAKE ROM DATA") != 13) {
            qWarning() << "Failed to write ROM test file:" << path;
            return 0;
        }

        f.close();

        int libId = db.insertLibrary(dir.path(), "Test");
        int sysId = db.getSystemId("NES");

        FileRecord fr;
        fr.libraryId     = libId;
        fr.filename      = filename;
        fr.originalPath  = path;
        fr.currentPath   = path;
        fr.extension     = ".nes";
        fr.systemId      = sysId;
        fr.fileSize      = 13;
        return db.insertFile(fr);
    }

    static GameMetadata makeMetadata()
    {
        GameMetadata m;
        m.title       = "Super Mario Bros.";
        m.system      = "NES";
        m.region      = "USA";
        m.publisher   = "Nintendo";
        m.releaseDate = "1985-09-13";
        m.matchMethod = "hash";
        return m;
    }
};

void OrganizeEngineTest::testDryRunProducesNoFilesystemChange()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);
    const QString originalPath = srcDir.path() + "/mario.nes";
    QVERIFY(QFile::exists(originalPath));

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(true);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Move);
    // Dry-run: file must remain at original location
    QVERIFY(QFile::exists(originalPath));
    QVERIFY(!result.error.contains("permission") || result.success);  // No real error
}

void OrganizeEngineTest::testMoveFile()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);
    const QString originalPath = srcDir.path() + "/mario.nes";

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setCollisionStrategy(CollisionStrategy::Skip);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Move);
    QVERIFY(result.success);
    QVERIFY(!QFile::exists(originalPath));
    QVERIFY(QFile::exists(result.newPath));
}

void OrganizeEngineTest::testCopyFile()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);
    const QString originalPath = srcDir.path() + "/mario.nes";

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setCollisionStrategy(CollisionStrategy::Skip);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Copy);
    QVERIFY(result.success);
    // Source retained for Copy
    QVERIFY(QFile::exists(originalPath));
    QVERIFY(QFile::exists(result.newPath));
}

void OrganizeEngineTest::testCollisionSkip()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    // Pre-create blocking file at destination
    const QString dst = dstDir.path() + "/Super Mario Bros..nes";
    {
        QFile f(dst);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("existing") == 8);
    }

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setCollisionStrategy(CollisionStrategy::Skip);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Move);
    // Skip strategy: operation should be skipped (success=false or newPath=oldPath)
    // The existing destination file should be unchanged
    QFileInfo dstInfo(dst);
    QCOMPARE(dstInfo.size(), static_cast<qint64>(8));  // "existing" still there
}

void OrganizeEngineTest::testCollisionRename()
{
    QTemporaryDir dstDir;
    QVERIFY(dstDir.isValid());

    const QString path = dstDir.path() + "/Super Mario Bros..nes";
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("existing") == 8);
    }

    QString resolved = OrganizeEngine::resolveCollision(path, CollisionStrategy::Rename);
    // Must differ from the original
    QVERIFY(resolved != path);
    // Must end with .nes
    QVERIFY(resolved.endsWith(".nes"));
}

void OrganizeEngineTest::testCollisionOverwrite()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    const QString dst = dstDir.path() + "/Super Mario Bros..nes";
    {
        QFile f(dst);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("old content") == 11);
    }

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setCollisionStrategy(CollisionStrategy::Overwrite);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Copy);
    QVERIFY(result.success);
    // Destination should now contain the new content ("FAKE ROM DATA")
    QFile f(result.newPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("FAKE ROM DATA"));
}

void OrganizeEngineTest::testCollisionOverwritePreservesDestinationOnFailure()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    const QString missingSource = srcDir.path() + "/missing.nes";
    const QString dst = dstDir.path() + "/Super Mario Bros..nes";
    {
        QFile f(dst);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("old content") == 11);
    }

    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary(srcDir.path(), "Test");
    int sysId = db.getSystemId("NES");

    FileRecord fr;
    fr.libraryId = libId;
    fr.filename = "missing.nes";
    fr.originalPath = missingSource;
    fr.currentPath = missingSource;
    fr.extension = ".nes";
    fr.systemId = sysId;
    fr.fileSize = 0;
    const int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setCollisionStrategy(CollisionStrategy::Overwrite);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Copy);
    QVERIFY(!result.success);

    QFile f(dst);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), QByteArray("old content"));
}

void OrganizeEngineTest::testUndoOperation()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);
    const QString originalPath = srcDir.path() + "/mario.nes";

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setCollisionStrategy(CollisionStrategy::Skip);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Move);
    QVERIFY(result.success);
    QVERIFY(!QFile::exists(originalPath));

    QVERIFY(engine.undoOperation(result.undoId));
    QVERIFY(QFile::exists(originalPath));
}

void OrganizeEngineTest::testWouldCollide()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.path() + "/existing.nes";
    QVERIFY(!OrganizeEngine::wouldCollide(path));

    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("x") == 1);
    }
    QVERIFY(OrganizeEngine::wouldCollide(path));
}

void OrganizeEngineTest::testResolveCollisionSkip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + "/game.nes";
    QString resolved = OrganizeEngine::resolveCollision(path, CollisionStrategy::Skip);
    // Skip strategy returns the same path (caller decides not to proceed)
    QCOMPARE(resolved, path);
}

void OrganizeEngineTest::testResolveCollisionRename()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Create the base file so collision is detected
    const QString path = dir.path() + "/game.nes";
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
    }

    QString resolved = OrganizeEngine::resolveCollision(path, CollisionStrategy::Rename);
    QVERIFY(resolved != path);
    QVERIFY(resolved.endsWith(".nes"));
    QVERIFY(resolved.contains("game"));
}

void OrganizeEngineTest::testFolderNamingNoneIsFlat()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setFolderNaming(Constants::FolderNaming::Scheme::None);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Copy);
    QVERIFY(result.success);
    // Flat: file lands directly in dstDir, no subfolder
    QFileInfo info(result.newPath);
    QCOMPARE(info.absolutePath(), QDir(dstDir.path()).absolutePath());
}

void OrganizeEngineTest::testFolderNamingDefaultCreatesSubfolder()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
    QVERIFY(fileId > 0);

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setFolderNaming(Constants::FolderNaming::Scheme::Default);

    OrganizeResult result = engine.organizeFile(fileId, makeMetadata(),
                                                dstDir.path(), FileOperation::Copy);
    QVERIFY(result.success);
    // Default (ES-DE) for NES = "nes" subfolder
    QFileInfo info(result.newPath);
    QCOMPARE(info.absolutePath(), QDir(dstDir.path()).filePath("nes"));
    QVERIFY(QFile::exists(result.newPath));
}

void OrganizeEngineTest::testFolderNamingBatoceraGenesis()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    // Create a Genesis ROM file
    const QString path = srcDir.path() + "/sonic.md";
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QVERIFY(f.write("FAKE ROM DATA") == 13);
    f.close();

    int libId = db.insertLibrary(srcDir.path(), "Test");
    int sysId = db.getSystemId("Genesis");
    QVERIFY(sysId > 0);

    FileRecord fr;
    fr.libraryId    = libId;
    fr.filename     = "sonic.md";
    fr.originalPath = path;
    fr.currentPath  = path;
    fr.extension    = ".md";
    fr.systemId     = sysId;
    fr.fileSize     = 13;
    int fileId = db.insertFile(fr);
    QVERIFY(fileId > 0);

    GameMetadata meta;
    meta.title  = "Sonic the Hedgehog";
    meta.system = "Genesis";
    meta.region = "USA";

    OrganizeEngine engine(db);
    engine.setTemplate("{title}{ext}");
    engine.setDryRun(false);
    engine.setFolderNaming(Constants::FolderNaming::Scheme::Batocera);

    OrganizeResult result = engine.organizeFile(fileId, meta,
                                                dstDir.path(), FileOperation::Copy);
    QVERIFY(result.success);
    // Batocera uses "megadrive" for Genesis
    QFileInfo info(result.newPath);
    QCOMPARE(info.absolutePath(), QDir(dstDir.path()).filePath("megadrive"));
}

void OrganizeEngineTest::testFolderNamingBatoceraSegaCD()
{
    using Constants::FolderNaming::Scheme;
    using Constants::FolderNaming::folderNameForSystemId;
    using namespace Constants::Systems;

    // Batocera uses "megacd" (not "segacd") for Sega CD
    QCOMPARE(folderNameForSystemId(ID_SEGA_CD, Scheme::Batocera), QStringLiteral("megacd"));
    // All other schemes use "segacd"
    QCOMPARE(folderNameForSystemId(ID_SEGA_CD, Scheme::Default),  QStringLiteral("segacd"));
    QCOMPARE(folderNameForSystemId(ID_SEGA_CD, Scheme::RetroPie), QStringLiteral("segacd"));
    QCOMPARE(folderNameForSystemId(ID_SEGA_CD, Scheme::EmuDeck),  QStringLiteral("segacd"));
    QCOMPARE(folderNameForSystemId(ID_SEGA_CD, Scheme::RomM),     QStringLiteral("segacd"));
}

void OrganizeEngineTest::testFolderNaming3DO()
{
    using Constants::FolderNaming::Scheme;
    using Constants::FolderNaming::folderNameForSystemId;
    using namespace Constants::Systems;

    QCOMPARE(folderNameForSystemId(ID_3DO, Scheme::Default),  QStringLiteral("3do"));
    QCOMPARE(folderNameForSystemId(ID_3DO, Scheme::Batocera), QStringLiteral("3do"));
    QCOMPARE(folderNameForSystemId(ID_3DO, Scheme::RetroPie), QStringLiteral("3do"));
    QCOMPARE(folderNameForSystemId(ID_3DO, Scheme::EmuDeck),  QStringLiteral("3do"));
    QCOMPARE(folderNameForSystemId(ID_3DO, Scheme::RomM),     QStringLiteral("3do"));
}

void OrganizeEngineTest::testFolderNamingNeoGeoCD()
{
    using Constants::FolderNaming::Scheme;
    using Constants::FolderNaming::folderNameForSystemId;
    using namespace Constants::Systems;

    QCOMPARE(folderNameForSystemId(ID_NEO_GEO_CD, Scheme::Default),  QStringLiteral("neogeocd"));
    QCOMPARE(folderNameForSystemId(ID_NEO_GEO_CD, Scheme::Batocera), QStringLiteral("neogeocd"));
    QCOMPARE(folderNameForSystemId(ID_NEO_GEO_CD, Scheme::RetroPie), QStringLiteral("neogeocd"));
    QCOMPARE(folderNameForSystemId(ID_NEO_GEO_CD, Scheme::EmuDeck),  QStringLiteral("neogeocd"));
    QCOMPARE(folderNameForSystemId(ID_NEO_GEO_CD, Scheme::RomM),     QStringLiteral("neogeocd"));
}

void OrganizeEngineTest::testFolderNamingSchemeFromString()
{
    using Constants::FolderNaming::Scheme;
    using Constants::FolderNaming::schemeFromString;

    QCOMPARE(schemeFromString("none"), Scheme::None);
    QCOMPARE(schemeFromString("default"), Scheme::Default);
    QCOMPARE(schemeFromString("Default"), Scheme::Default);
    QCOMPARE(schemeFromString("es-de"), Scheme::Default);
    QCOMPARE(schemeFromString("batocera"), Scheme::Batocera);
    QCOMPARE(schemeFromString("retropie"), Scheme::RetroPie);
    QCOMPARE(schemeFromString("emudeck"), Scheme::EmuDeck);
    QCOMPARE(schemeFromString("romm"), Scheme::RomM);
    QCOMPARE(schemeFromString("unknown"), Scheme::None);
}

QTEST_MAIN(OrganizeEngineTest)
#include "test_organize_engine.moc"
