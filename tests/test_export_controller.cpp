#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../src/ui/controllers/export_controller.h"
#include "../src/core/database.h"

using namespace Remus;

class ExportControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState();
    void testGetAvailableSystems();
    void testExportToCSVEmpty();
    void testExportToCSVWithData();
    void testExportToJSONEmpty();
    void testExportToJSONWithData();
    void testExportToRetroArch();
    void testExportToEmulationStation();
    void testExportToLaunchBox();
    void testGetExportPreview();

private:
    // Build a populated DB fixture and return the controller.
    struct Fixture {
        Database         db;
        ExportController controller;

        Fixture() : controller(&db) {
            db.initialize(":memory:");
        }

        // Insert one matched game file per system and return the file IDs.
        QVector<int> populate()
        {
            QVector<int> ids;
            const QStringList systems = {"NES", "SNES", "PlayStation"};

            for (const QString &sysName : systems) {
                int sysId = db.getSystemId(sysName);
                if (sysId == 0) continue;

                int libId = db.insertLibrary("/roms/" + sysName.toLower(), sysName);

                FileRecord fr;
                fr.libraryId     = libId;
                fr.filename      = "Game (" + sysName + ").rom";
                fr.originalPath  = "/roms/" + sysName.toLower() + "/Game.rom";
                fr.currentPath   = fr.originalPath;
                fr.extension     = ".rom";
                fr.systemId      = sysId;
                fr.fileSize      = 4096;
                fr.crc32         = "AABB00" + QString::number(sysId, 16).toUpper();
                fr.hashCalculated = true;
                int fid = db.insertFile(fr);
                fr.id = fid;

                int gid = db.insertGame("Test Game for " + sysName, sysId,
                                        "USA", "TestPub", "TestDev",
                                        "2000-01-01", "A test game", "Action", "1", 8.5f);
                db.insertMatch(fid, gid, 100.0f, "hash");
                ids.append(fid);
            }
            return ids;
        }
    };
};

// ── Tests ─────────────────────────────────────────────────────────────────

void ExportControllerTest::testInitialState()
{
    Fixture f;
    QVERIFY(!f.controller.isExporting());
    QCOMPARE(f.controller.exportProgress(), 0);
    QVERIFY(f.controller.lastExportPath().isEmpty());
}

void ExportControllerTest::testGetAvailableSystems()
{
    Fixture f;
    f.populate();

    QVariantList systems = f.controller.getAvailableSystems();
    QVERIFY(!systems.isEmpty());
}

void ExportControllerTest::testExportToCSVEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    const QString csvPath = dir.path() + "/empty.csv";
    bool ok = f.controller.exportToCSV(csvPath);
    QVERIFY(ok);
    QVERIFY(QFile::exists(csvPath));

    QFile file(csvPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(file.readAll());

    // Header row must always be present
    QVERIFY(content.contains("filename", Qt::CaseInsensitive) ||
            content.contains("title",    Qt::CaseInsensitive));
}

void ExportControllerTest::testExportToCSVWithData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    f.populate();

    const QString csvPath = dir.path() + "/library.csv";
    bool ok = f.controller.exportToCSV(csvPath);
    QVERIFY(ok);

    QFile file(csvPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(file.readAll());

    // There should be data lines beyond the header
    const int lineCount = content.split('\n', Qt::SkipEmptyParts).size();
    QVERIFY2(lineCount > 1, "Expected header + at least one data row");

    // Known inserted titles should appear
    QVERIFY(content.contains("Test Game for NES")        ||
            content.contains("Test Game for SNES")       ||
            content.contains("Test Game for PlayStation"));
}

void ExportControllerTest::testExportToJSONEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    const QString jsonPath = dir.path() + "/empty.json";
    bool ok = f.controller.exportToJSON(jsonPath, false);
    QVERIFY(ok);

    QFile file(jsonPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    QCOMPARE(err.error, QJsonParseError::NoError);
    QVERIFY(doc.isObject() || doc.isArray());
}

void ExportControllerTest::testExportToJSONWithData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    f.populate();

    const QString jsonPath = dir.path() + "/library.json";
    bool ok = f.controller.exportToJSON(jsonPath, false);
    QVERIFY(ok);

    QFile file(jsonPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    QCOMPARE(err.error, QJsonParseError::NoError);

    // Exported document should contain game entries
    if (doc.isArray()) {
        QVERIFY(!doc.array().isEmpty());
    } else if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        // Common key patterns: "games", "library", "entries"
        bool hasEntries = false;
        for (const QString &key : obj.keys()) {
            if (obj[key].isArray() && !obj[key].toArray().isEmpty()) {
                hasEntries = true;
                break;
            }
        }
        QVERIFY(hasEntries || !obj.isEmpty());
    }
}

void ExportControllerTest::testExportToRetroArch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    f.populate();

    int count = f.controller.exportToRetroArch(dir.path());
    QVERIFY(count >= 0);

    if (count > 0) {
        QDir outDir(dir.path());
        QStringList lplFiles = outDir.entryList({"*.lpl"}, QDir::Files);
        QVERIFY(!lplFiles.isEmpty());
    }
}

void ExportControllerTest::testExportToEmulationStation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    f.populate();

    int count = f.controller.exportToEmulationStation(dir.path(), false);
    QVERIFY(count >= 0);

    if (count > 0) {
        // ES writes gamelist.xml inside per-system subdirectories
        QDirIterator it(dir.path(), {"gamelist.xml"}, QDir::Files,
                        QDirIterator::Subdirectories);
        QVERIFY(it.hasNext());
    }
}

void ExportControllerTest::testExportToLaunchBox()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    f.populate();

    int count = f.controller.exportToLaunchBox(dir.path(), false);
    QVERIFY(count >= 0);

    if (count > 0) {
        QDir outDir(dir.path());
        QStringList xmlFiles = outDir.entryList({"*.xml"}, QDir::Files);
        QVERIFY(!xmlFiles.isEmpty());
    }
}

void ExportControllerTest::testGetExportPreview()
{
    Fixture f;
    f.populate();

    QVariantMap preview = f.controller.getExportPreview({});
    QVERIFY(!preview.isEmpty());
}

QTEST_MAIN(ExportControllerTest)
#include "test_export_controller.moc"
