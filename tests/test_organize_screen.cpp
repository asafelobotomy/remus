/**
 * @file test_organize_screen.cpp
 * @brief Unit tests for OrganizeScreen data logic.
 *
 * Creates a TuiApp with an in-memory database (no notcurses initialisation)
 * and verifies loadFromDatabase / runDryRun behaviour through the public API.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/tui/app.h"
#include "../src/tui/organize_screen.h"
#include "../src/core/database.h"

using namespace Remus;

class TestOrganizeScreen : public QObject
{
    Q_OBJECT

private:
    /**
     * Populate test DB with:
     *  - 2 files that have high-confidence matches (NES, SNES)
     *  - 1 file with a low-confidence match (below threshold, excluded)
     *  - 1 file with a rejected match (excluded)
     *
     * Returns vector of inserted file IDs in order.
     */
    QVector<int> populateTestData(TuiApp &app, const QString &romDir)
    {
        QDir().mkpath(romDir);
        { QFile f(romDir + "/Mario.nes");   f.open(QIODevice::WriteOnly); }
        { QFile f(romDir + "/Zelda.sfc");   f.open(QIODevice::WriteOnly); }
        { QFile f(romDir + "/Unknown.nes"); f.open(QIODevice::WriteOnly); }
        { QFile f(romDir + "/Rejected.sfc"); f.open(QIODevice::WriteOnly); }

        Database &db = app.db();
        int libId  = db.insertLibrary(romDir, "Test Library");
        int nesId  = db.getSystemId("NES");
        int snesId = db.getSystemId("SNES");
        Q_ASSERT(libId > 0 && nesId > 0 && snesId > 0);

        QVector<int> ids;

        auto makeFile = [&](const QString &name, int sysId) -> int {
            FileRecord fr;
            fr.libraryId   = libId;
            fr.filename    = name;
            fr.originalPath = romDir + "/" + name;
            fr.currentPath  = fr.originalPath;
            fr.extension    = "." + name.section('.', -1);
            fr.systemId     = sysId;
            fr.hashCalculated = true;
            fr.crc32 = "AABBCCDD";
            return db.insertFile(fr);
        };

        // file 0: NES — high confidence match (95%)
        ids.append(makeFile("Mario.nes", nesId));
        // file 1: SNES — high confidence match (90%)
        ids.append(makeFile("Zelda.sfc", snesId));
        // file 2: NES — low confidence match (50%) → should be EXCLUDED
        ids.append(makeFile("Unknown.nes", nesId));
        // file 3: SNES — rejected match → should be EXCLUDED
        ids.append(makeFile("Rejected.sfc", snesId));

        // Insert game records
        int gMario = db.insertGame("Super Mario Bros.", nesId,  "USA",
                                   "Nintendo", "Nintendo", "1985-09-13",
                                   "Classic platformer", "Platform", "1", 9.0f);
        int gZelda = db.insertGame("The Legend of Zelda", snesId, "USA",
                                   "Nintendo", "Nintendo", "1991-11-21",
                                   "Action adventure", "Action", "1", 9.5f);
        int gUnknown = db.insertGame("Unknown Game", nesId, "", "", "", "", "", "", "1", 0.0f);
        int gRejected = db.insertGame("Rejected Game", snesId, "", "", "", "", "", "", "1", 0.0f);

        // Matches
        db.insertMatch(ids[0], gMario,    95.0f, "hash");   // high confidence — included
        db.insertMatch(ids[1], gZelda,    90.0f, "hash");   // high confidence — included
        db.insertMatch(ids[2], gUnknown,  50.0f, "name");   // low confidence — excluded
        db.insertMatch(ids[3], gRejected, 95.0f, "hash");   // rejected below

        // Reject ids[3]
        db.rejectMatch(ids[3]);

        return ids;
    }

private slots:

    // ── loadFromDatabase ──────────────────────────────────────

    void testLoadFromDatabaseFiltersLowConfidence()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        populateTestData(app, tmp.path() + "/roms");

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        // Only the two high-confidence, non-rejected matches should be loaded
        QCOMPARE(screen.entryCount(), 2);
    }

    void testLoadFromDatabaseExcludesRejected()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        populateTestData(app, tmp.path() + "/roms");

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        for (int i = 0; i < screen.entryCount(); ++i) {
            QVERIFY(screen.entryAt(i).confidence >= 75);
            QVERIFY(!screen.entryAt(i).filename.empty());
        }
    }

    void testLoadFromDatabasePopulatesFields()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        populateTestData(app, tmp.path() + "/roms");

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        QCOMPARE(screen.entryCount(), 2);

        bool foundMario = false;
        for (int i = 0; i < screen.entryCount(); ++i) {
            const auto &e = screen.entryAt(i);
            if (e.title == std::string("Super Mario Bros.")) {
                foundMario = true;
                QCOMPARE(e.confidence, 95);
                QVERIFY(e.oldPath.find("Mario.nes") != std::string::npos);
                QVERIFY(!e.system.empty());
            }
        }
        QVERIFY(foundMario);
    }

    void testEntryStatusDefaultIsPending()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        populateTestData(app, tmp.path() + "/roms");

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        for (int i = 0; i < screen.entryCount(); ++i) {
            QCOMPARE(static_cast<int>(screen.entryAt(i).status),
                     static_cast<int>(OrganizeScreen::EntryStatus::Pending));
        }
    }

    // ── runDryRun ─────────────────────────────────────────────

    void testRunDryRunWithNoDest()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        populateTestData(app, tmp.path() + "/roms");

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        // No destination set — entries stay Pending
        screen.runDryRun();

        for (int i = 0; i < screen.entryCount(); ++i) {
            QCOMPARE(static_cast<int>(screen.entryAt(i).status),
                     static_cast<int>(OrganizeScreen::EntryStatus::Pending));
        }
    }

    void testRunDryRunWithDestPopulatesNewPath()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QTemporaryDir dest;
        QVERIFY(dest.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        populateTestData(app, tmp.path() + "/roms");

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        // Set destination via public setter and run dry run
        screen.setDestination(dest.path().toStdString());
        screen.runDryRun();

        QCOMPARE(screen.entryCount(), 2);
        for (int i = 0; i < screen.entryCount(); ++i) {
            QCOMPARE(static_cast<int>(screen.entryAt(i).status),
                     static_cast<int>(OrganizeScreen::EntryStatus::Preview));
            QVERIFY(!screen.entryAt(i).newPath.empty());
            QVERIFY(screen.entryAt(i).newPath.find(dest.path().toStdString()) != std::string::npos);
        }
    }

    // ── Multi-disc co-move ───────────────────────────────────

    void testLoadFromDatabaseCollectsLinkedFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));
        QString romDir = tmp.path() + "/roms";
        QDir().mkpath(romDir);

        // Create primary CUE + linked BIN
        { QFile f(romDir + "/Game.cue"); f.open(QIODevice::WriteOnly); f.write("FILE Game.bin\n"); }
        { QFile f(romDir + "/Game.bin"); f.open(QIODevice::WriteOnly); f.write("data"); }

        Database &db = app.db();
        int libId  = db.insertLibrary(romDir, "Test Library");
        int psxId  = db.getSystemId("PlayStation");
        if (psxId <= 0) psxId = db.getSystemId("NES"); // fallback for test

        // Insert primary file (CUE)
        FileRecord frCue;
        frCue.libraryId    = libId;
        frCue.filename     = "Game.cue";
        frCue.originalPath = romDir + "/Game.cue";
        frCue.currentPath  = frCue.originalPath;
        frCue.extension    = ".cue";
        frCue.systemId     = psxId;
        frCue.isPrimary    = true;
        frCue.hashCalculated = true;
        frCue.crc32        = "11223344";
        int cueId = db.insertFile(frCue);
        QVERIFY(cueId > 0);

        // Insert linked BIN file
        FileRecord frBin;
        frBin.libraryId    = libId;
        frBin.filename     = "Game.bin";
        frBin.originalPath = romDir + "/Game.bin";
        frBin.currentPath  = frBin.originalPath;
        frBin.extension    = ".bin";
        frBin.systemId     = psxId;
        frBin.isPrimary    = false;
        frBin.parentFileId = cueId;
        frBin.hashCalculated = true;
        frBin.crc32        = "55667788";
        int binId = db.insertFile(frBin);
        QVERIFY(binId > 0);

        // Insert game + match for primary file
        int gameId = db.insertGame("CUE Game", psxId, "USA",
                                   "Publisher", "Developer", "2000-01-01",
                                   "Desc", "RPG", "1", 8.0f);
        db.insertMatch(cueId, gameId, 95.0f, "hash");

        // Load and verify linked files are collected
        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        QCOMPARE(screen.entryCount(), 1);
        const auto &entry = screen.entryAt(0);
        QCOMPARE(static_cast<int>(entry.linkedFileIds.size()), 1);
        QCOMPARE(entry.linkedFileIds[0], binId);
    }

    // ── isRunning ────────────────────────────────────────────

    void testIsRunningFalseInitially()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));

        OrganizeScreen screen(app);
        QVERIFY(!screen.isRunning());
    }

    // ── Empty DB ─────────────────────────────────────────────

    void testEmptyDatabaseYieldsZeroEntries()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/org.db"));

        OrganizeScreen screen(app);
        screen.loadFromDatabase();

        QCOMPARE(screen.entryCount(), 0);
    }
};

QTEST_MAIN(TestOrganizeScreen)
#include "test_organize_screen.moc"
