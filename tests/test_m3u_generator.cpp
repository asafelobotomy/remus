#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/m3u_generator.h"
#include "../src/core/database.h"

using namespace Remus;

class M3UGeneratorTest : public QObject
{
    Q_OBJECT

private slots:
    // Static helper tests — no DB required
    void testIsMultiDiscTrue();
    void testIsMultiDiscFalse();
    void testExtractBaseTitle();
    void testExtractDiscNumber();
    void testExtractDiscNumberMissing();

    // Instance tests — in-memory DB
    void testDetectMultiDiscGames();
    void testDetectSingleDiscExcluded();
    void testGenerateM3UFile();
    void testGenerateAll();

private:
    static int insertDiscFile(Database &db, int libId, int sysId,
                               const QString &filename)
    {
        FileRecord fr;
        fr.libraryId     = libId;
        fr.filename      = filename;
        fr.originalPath  = "/roms/psx/" + filename;
        fr.currentPath   = fr.originalPath;
        fr.extension     = "." + filename.section('.', -1);
        fr.systemId      = sysId;
        fr.fileSize      = 700 * 1024 * 1024;
        fr.hashCalculated = false;
        return db.insertFile(fr);
    }
};

// ── Static helper tests ────────────────────────────────────────────────────

void M3UGeneratorTest::testIsMultiDiscTrue()
{
    QVERIFY(M3UGenerator::isMultiDisc("Final Fantasy VII (USA) (Disc 1).chd"));
    QVERIFY(M3UGenerator::isMultiDisc("Metal Gear Solid (USA) (Disc 2).bin"));
    QVERIFY(M3UGenerator::isMultiDisc("Xenogears (USA) (Disc 1 of 2).iso"));
}

void M3UGeneratorTest::testIsMultiDiscFalse()
{
    QVERIFY(!M3UGenerator::isMultiDisc("Super Mario 64 (USA).n64"));
    QVERIFY(!M3UGenerator::isMultiDisc("Chrono Trigger (USA).sfc"));
    QVERIFY(!M3UGenerator::isMultiDisc(""));
}

void M3UGeneratorTest::testExtractBaseTitle()
{
    QString base = M3UGenerator::extractBaseTitle(
        "Final Fantasy VII (USA) (Disc 1).chd");
    // Base title should not contain a disc specifier
    QVERIFY(!base.isEmpty());
    QVERIFY(!base.contains("Disc 1", Qt::CaseInsensitive));
    QVERIFY(base.contains("Final Fantasy VII"));
}

void M3UGeneratorTest::testExtractDiscNumber()
{
    QCOMPARE(M3UGenerator::extractDiscNumber(
        "Final Fantasy VII (USA) (Disc 1).chd"), 1);
    QCOMPARE(M3UGenerator::extractDiscNumber(
        "Metal Gear Solid (USA) (Disc 2).bin"), 2);
    QCOMPARE(M3UGenerator::extractDiscNumber(
        "Xenogears (USA) (Disc 1 of 2).iso"), 1);
}

void M3UGeneratorTest::testExtractDiscNumberMissing()
{
    QCOMPARE(M3UGenerator::extractDiscNumber(
        "Chrono Trigger (USA).sfc"), 0);
}

// ── Instance tests ─────────────────────────────────────────────────────────

void M3UGeneratorTest::testDetectMultiDiscGames()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0) QSKIP("PlayStation system not in default DB");

    insertDiscFile(db, libId, sysId, "Final Fantasy VII (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Final Fantasy VII (USA) (Disc 2).chd");
    insertDiscFile(db, libId, sysId, "Final Fantasy VII (USA) (Disc 3).chd");
    insertDiscFile(db, libId, sysId, "Chrono Cross (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Chrono Cross (USA) (Disc 2).chd");
    // Single-disc game — should NOT appear
    insertDiscFile(db, libId, sysId, "Castlevania - Symphony of the Night.chd");

    M3UGenerator gen(db);
    QMap<QString, QList<int>> multiDisc = gen.detectMultiDiscGames();

    // Should detect 2 multi-disc games
    QCOMPARE(multiDisc.size(), 2);

    // Each entry should have the correct disc count
    for (auto it = multiDisc.constBegin(); it != multiDisc.constEnd(); ++it) {
        if (it.key().contains("Final Fantasy VII")) {
            QCOMPARE(it.value().size(), 3);
        } else if (it.key().contains("Chrono Cross")) {
            QCOMPARE(it.value().size(), 2);
        } else {
            QFAIL(qPrintable("Unexpected game detected: " + it.key()));
        }
    }
}

void M3UGeneratorTest::testDetectSingleDiscExcluded()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0) QSKIP("PlayStation system not in default DB");

    insertDiscFile(db, libId, sysId, "Gran Turismo (USA).chd");

    M3UGenerator gen(db);
    QMap<QString, QList<int>> multiDisc = gen.detectMultiDiscGames();
    QVERIFY(multiDisc.isEmpty());
}

void M3UGeneratorTest::testGenerateM3UFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    M3UGenerator gen(db);

    const QStringList discPaths = {
        "/roms/psx/Final Fantasy VII (USA) (Disc 1).chd",
        "/roms/psx/Final Fantasy VII (USA) (Disc 2).chd",
        "/roms/psx/Final Fantasy VII (USA) (Disc 3).chd"
    };
    const QString m3uPath = dir.path() + "/Final Fantasy VII (USA).m3u";

    bool ok = gen.generateM3U("Final Fantasy VII (USA)", discPaths, m3uPath);
    QVERIFY(ok);
    QVERIFY(QFile::exists(m3uPath));

    QFile f(m3uPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(f.readAll());

    // All three disc paths must appear in the playlist
    for (const QString &path : discPaths) {
        QVERIFY2(content.contains(path) || content.contains(QFileInfo(path).fileName()),
                 qPrintable("Missing disc in M3U: " + path));
    }
}

void M3UGeneratorTest::testGenerateAll()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/psx", "PSX");
    int sysId = db.getSystemId("PlayStation");
    if (sysId == 0) QSKIP("PlayStation system not in default DB");

    insertDiscFile(db, libId, sysId, "Metal Gear Solid (USA) (Disc 1).chd");
    insertDiscFile(db, libId, sysId, "Metal Gear Solid (USA) (Disc 2).chd");

    M3UGenerator gen(db);
    int count = gen.generateAll(QString(), dir.path());
    QCOMPARE(count, 1);

    // Verify the playlist file was created in the output directory
    QDir outDir(dir.path());
    QStringList m3uFiles = outDir.entryList({"*.m3u"}, QDir::Files);
    QCOMPARE(m3uFiles.size(), 1);
}

QTEST_MAIN(M3UGeneratorTest)
#include "test_m3u_generator.moc"
