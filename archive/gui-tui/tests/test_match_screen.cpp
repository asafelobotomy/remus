/**
 * @file test_match_screen.cpp
 * @brief Unit tests for MatchScreen data logic (loadFromDatabase, confidenceIcon)
 *
 * Creates a TuiApp with an in-memory database (no notcurses initialisation)
 * and verifies the screen's file loading, match status, and confidence display.
 * All assertions use the public query API.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/tui/app.h"
#include "../src/tui/match_screen.h"
#include "../src/core/database.h"

using namespace Remus;

class TestMatchScreen : public QObject
{
    Q_OBJECT

private:
    /// Helper: create stub files and DB records.
    /// Returns file IDs: [NES matched, NES unmatched, SNES hashed-unmatched].
    QVector<int> populateTestData(TuiApp &app, const QString &romDir)
    {
        QDir().mkpath(romDir);
        QFile(romDir + "/Mario.nes").open(QIODevice::WriteOnly);
        QFile(romDir + "/Zelda.nes").open(QIODevice::WriteOnly);
        QFile(romDir + "/FZero.sfc").open(QIODevice::WriteOnly);

        Database &db = app.db();
        int libId = db.insertLibrary(romDir, "Test Library");
        Q_ASSERT(libId > 0);

        int nesId  = db.getSystemId("NES");
        int snesId = db.getSystemId("SNES");
        Q_ASSERT(nesId > 0 && snesId > 0);

        QVector<int> ids;

        // NES file 1 — will have a high-confidence match
        FileRecord f1;
        f1.libraryId = libId; f1.filename = "Mario.nes";
        f1.originalPath = romDir + "/Mario.nes";
        f1.currentPath = f1.originalPath;
        f1.extension = ".nes"; f1.systemId = nesId;
        f1.hashCalculated = true; f1.crc32 = "AABB1122";
        ids.append(db.insertFile(f1));

        // NES file 2 — hashed but no match
        FileRecord f2;
        f2.libraryId = libId; f2.filename = "Zelda.nes";
        f2.originalPath = romDir + "/Zelda.nes";
        f2.currentPath = f2.originalPath;
        f2.extension = ".nes"; f2.systemId = nesId;
        f2.hashCalculated = false;
        ids.append(db.insertFile(f2));
        // Mark as hashed via the proper updateFileHashes path
        db.updateFileHashes(ids.last(), "CCDD3344", "", "");

        // SNES file — not yet hashed
        FileRecord f3;
        f3.libraryId = libId; f3.filename = "FZero.sfc";
        f3.originalPath = romDir + "/FZero.sfc";
        f3.currentPath = f3.originalPath;
        f3.extension = ".sfc"; f3.systemId = snesId;
        f3.hashCalculated = false;
        ids.append(db.insertFile(f3));

        // Insert match for file 1
        int gid = db.insertGame("Super Mario Bros.", nesId, "USA",
                                "Nintendo", "Nintendo", "1985-09-13",
                                "Classic platformer", "Platform", "1", 9.0f);
        db.insertMatch(ids[0], gid, 97.0f, "hash");

        return ids;
    }

private slots:

    // ── loadFromDatabase ──────────────────────────────────

    void testLoadFromDatabaseFileCount()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/match.db"));
        populateTestData(app, tmp.path() + "/roms");

        MatchScreen screen(app);
        screen.loadFromDatabase();

        QCOMPARE(screen.fileCount(), 3);
    }

    void testLoadFromDatabaseMatchedFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/match.db"));
        auto ids = populateTestData(app, tmp.path() + "/roms");

        MatchScreen screen(app);
        screen.loadFromDatabase();

        // Find matched file
        bool found = false;
        for (int i = 0; i < screen.fileCount(); ++i) {
            const auto &e = screen.fileAt(i);
            if (e.fileId == ids[0]) {
                QCOMPARE(e.filename, std::string("Mario.nes"));
                QCOMPARE(e.matchStatus, std::string("match ✓"));
                QVERIFY(e.confidence >= 90);
                QVERIFY(!e.title.empty());
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testLoadFromDatabaseUnmatchedFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/match.db"));
        auto ids = populateTestData(app, tmp.path() + "/roms");

        MatchScreen screen(app);
        screen.loadFromDatabase();

        // File 2 is hashed but unmatched
        for (int i = 0; i < screen.fileCount(); ++i) {
            const auto &e = screen.fileAt(i);
            if (e.fileId == ids[1]) {
                QCOMPARE(e.matchStatus, std::string("unmatched"));
                return;
            }
        }
        QFAIL("Expected to find unmatched file");
    }

    void testLoadFromDatabaseNotHashedFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/match.db"));
        auto ids = populateTestData(app, tmp.path() + "/roms");

        MatchScreen screen(app);
        screen.loadFromDatabase();

        // File 3 is not hashed
        for (int i = 0; i < screen.fileCount(); ++i) {
            const auto &e = screen.fileAt(i);
            if (e.fileId == ids[2]) {
                QCOMPARE(e.matchStatus, std::string("not hashed"));
                return;
            }
        }
        QFAIL("Expected to find not-hashed file");
    }

    void testLoadFromDatabaseEmptyDB()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/empty.db"));

        MatchScreen screen(app);
        screen.loadFromDatabase();

        QCOMPARE(screen.fileCount(), 0);
        QVERIFY(!screen.isPipelineRunning());
    }

    // ── confidenceIcon ────────────────────────────────────

    void testConfidenceIconHigh()
    {
        QCOMPARE(MatchScreen::confidenceIcon(100), std::string("✓"));
        QCOMPARE(MatchScreen::confidenceIcon(90),  std::string("✓"));
    }

    void testConfidenceIconMedium()
    {
        QCOMPARE(MatchScreen::confidenceIcon(75), std::string("~"));
        QCOMPARE(MatchScreen::confidenceIcon(60), std::string("~"));
    }

    void testConfidenceIconLow()
    {
        QCOMPARE(MatchScreen::confidenceIcon(30), std::string("?"));
        QCOMPARE(MatchScreen::confidenceIcon(1),  std::string("?"));
    }

    void testConfidenceIconNone()
    {
        QCOMPARE(MatchScreen::confidenceIcon(0),  std::string("-"));
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestMatchScreen t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_match_screen.moc"
