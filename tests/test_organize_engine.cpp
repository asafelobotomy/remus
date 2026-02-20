#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "../src/core/organize_engine.h"
#include "../src/core/database.h"
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
    void testUndoOperation();
    void testWouldCollide();
    void testResolveCollisionSkip();
    void testResolveCollisionRename();

private:
    // Write a small ROM file into dir and register it in db.
    static int makeRomFile(const QTemporaryDir &dir,
                           Database &db,
                           const QString &filename = "mario.nes")
    {
        const QString path = dir.path() + "/" + filename;
        QFile f(path);
        Q_ASSERT(f.open(QIODevice::WriteOnly));
        f.write("FAKE ROM DATA");

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
    { QFile f(dst); f.open(QIODevice::WriteOnly); f.write("existing"); }

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);

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
    { QFile f(path); f.open(QIODevice::WriteOnly); f.write("existing"); }

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
    { QFile f(dst); f.open(QIODevice::WriteOnly); f.write("old content"); }

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);

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

void OrganizeEngineTest::testUndoOperation()
{
    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));
    int fileId = makeRomFile(srcDir, db);
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

    { QFile f(path); f.open(QIODevice::WriteOnly); f.write("x"); }
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
    { QFile f(path); f.open(QIODevice::WriteOnly); }

    QString resolved = OrganizeEngine::resolveCollision(path, CollisionStrategy::Rename);
    QVERIFY(resolved != path);
    QVERIFY(resolved.endsWith(".nes"));
    QVERIFY(resolved.contains("game"));
}

QTEST_MAIN(OrganizeEngineTest)
#include "test_organize_engine.moc"
