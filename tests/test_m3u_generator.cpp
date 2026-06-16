#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/m3u_generator.h"
#include "../src/core/disc_set_utils.h"
#include "../src/core/database.h"

using namespace Remus;

class M3UGeneratorTest : public QObject {
    Q_OBJECT

private slots:
    void testDetectMultiDiscGames();
    void testDetectSingleDiscExcluded();
    void testGenerateM3UFile();
    void testGenerateAll();
    void testGenerateAllScopedFileIds();

private:
    static int insertDiscFile(Database &db, int libId, int sysId, const QString &filename) {
        FileRecord fr;
        fr.libraryId = libId;
        fr.filename = filename;
        fr.originalPath = "/roms/psx/" + filename;
        fr.currentPath = fr.originalPath;
        fr.extension = "." + filename.section('.', -1);
        fr.systemId = sysId;
        fr.fileSize = 700 * 1024 * 1024;
        fr.hashCalculated = false;
        return db.insertFile(fr);
    }
};

void M3UGeneratorTest::testDetectMultiDiscGames() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    insertDiscFile(db, libId, sysId, "Final Fantasy VII (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Final Fantasy VII (USA) (Disc 2).chd");
    insertDiscFile(db, libId, sysId, "Final Fantasy VII (USA) (Disc 3).chd");
    insertDiscFile(db, libId, sysId, "Chrono Cross (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Chrono Cross (USA) (Disc 2).chd");
    insertDiscFile(db, libId, sysId, "Castlevania - Symphony of the Night.chd");

    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    M3UGenerator gen(db);
    QMap<QString, QList<int>> multiDisc = gen.detectMultiDiscGames();

    QCOMPARE(multiDisc.size(), 2);

    int ff7Discs = 0;
    int ccDiscs = 0;
    for (auto it = multiDisc.constBegin(); it != multiDisc.constEnd(); ++it) {
        if (it.key().contains("Final Fantasy VII"))
            ff7Discs = it.value().size();
        else if (it.key().contains("Chrono Cross"))
            ccDiscs = it.value().size();
        else
            QFAIL(qPrintable("Unexpected game detected: " + it.key()));
    }
    QCOMPARE(ff7Discs, 3);
    QCOMPARE(ccDiscs, 2);
}

void M3UGeneratorTest::testDetectSingleDiscExcluded() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    insertDiscFile(db, libId, sysId, "Gran Turismo (USA).chd");
    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    M3UGenerator gen(db);
    QMap<QString, QList<int>> multiDisc = gen.detectMultiDiscGames();
    QVERIFY(multiDisc.isEmpty());
}

void M3UGeneratorTest::testGenerateM3UFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    M3UGenerator gen(db);

    const QStringList discPaths = { "/roms/psx/Final Fantasy VII (USA) (Disc 1).chd",
        "/roms/psx/Final Fantasy VII (USA) (Disc 2).chd", "/roms/psx/Final Fantasy VII (USA) (Disc 3).chd" };
    const QString m3uPath = dir.path() + "/Final Fantasy VII (USA).m3u";

    bool ok = gen.generateM3U("Final Fantasy VII (USA)", discPaths, m3uPath);
    QVERIFY(ok);
    QVERIFY(QFile::exists(m3uPath));

    QFile f(m3uPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(f.readAll());

    for (const QString &path : discPaths) {
        QVERIFY2(content.contains(path) || content.contains(QFileInfo(path).fileName()),
            qPrintable("Missing disc in M3U: " + path));
    }
}

void M3UGeneratorTest::testGenerateAll() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    insertDiscFile(db, libId, sysId, "Metal Gear Solid (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Metal Gear Solid (USA) (Disc 2).chd");
    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    M3UGenerator gen(db);
    int count = gen.generateAll(QString(), dir.path());
    QCOMPARE(count, 1);

    QDir outDir(dir.path());
    QStringList m3uFiles = outDir.entryList({ "*.m3u" }, QDir::Files);
    QCOMPARE(m3uFiles.size(), 1);
}

void M3UGeneratorTest::testGenerateAllScopedFileIds() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    const int mgs1 = insertDiscFile(db, libId, sysId, "Metal Gear Solid (USA) (Disc 1).chd");
    const int mgs2 = insertDiscFile(db, libId, sysId, "Metal Gear Solid (USA) (Disc 2).chd");
    insertDiscFile(db, libId, sysId, "Final Fantasy VIII (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Final Fantasy VIII (USA) (Disc 2).chd");
    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    M3UGenerator gen(db);
    const int count = gen.generateAll(QSet<int> { mgs1, mgs2 }, dir.path());
    QCOMPARE(count, 1);

    QDir outDir(dir.path());
    const QStringList m3uFiles = outDir.entryList({ "*.m3u" }, QDir::Files);
    QCOMPARE(m3uFiles.size(), 1);
    QVERIFY(m3uFiles.first().contains("Metal Gear Solid"));
}

QTEST_MAIN(M3UGeneratorTest)
#include "test_m3u_generator.moc"
