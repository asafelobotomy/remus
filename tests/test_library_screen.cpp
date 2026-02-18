/**
 * @file test_library_screen.cpp
 * @brief Unit tests for LibraryScreen data logic (loadFromDatabase, applyFilter, confirm/reject)
 *
 * Creates a TuiApp with an in-memory database (no notcurses initialisation)
 * and verifies the screen's grouping, filtering, and confirmation logic.
 * All assertions use the public query API — no friend class or direct member access.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/tui/app.h"
#include "../src/tui/library_screen.h"
#include "../src/core/database.h"

using namespace Remus;

class TestLibraryScreen : public QObject
{
    Q_OBJECT

private:
    /// Helper: populate the app's DB with test data across two systems.
    /// Creates actual stub files on disk so getExistingFiles() finds them.
    /// Returns the file IDs inserted (NES file 1, NES file 2, SNES file 1).
    QVector<int> populateTestData(TuiApp &app, const QString &romDir)
    {
        // Create stub ROM files so the screen's getExistingFiles() path-exists check passes
        QDir().mkpath(romDir);
        QFile(romDir + "/Mario.nes").open(QIODevice::WriteOnly);
        QFile(romDir + "/Zelda.nes").open(QIODevice::WriteOnly);
        QFile(romDir + "/DonkeyKong.sfc").open(QIODevice::WriteOnly);

        Database &db = app.db();

        int libId = db.insertLibrary(romDir, "Test Library");
        Q_ASSERT(libId > 0);

        // Use pre-populated default system IDs
        int nesId = db.getSystemId("NES");
        Q_ASSERT(nesId > 0);
        int snesId = db.getSystemId("SNES");
        Q_ASSERT(snesId > 0);

        QVector<int> ids;

        // NES file 1
        FileRecord f1;
        f1.libraryId = libId;
        f1.filename = "Mario.nes";
        f1.originalPath = romDir + "/Mario.nes";
        f1.currentPath = f1.originalPath;
        f1.extension = ".nes";
        f1.systemId = nesId;
        f1.hashCalculated = true;
        f1.crc32 = "AABB1122";
        ids.append(db.insertFile(f1));

        // NES file 2
        FileRecord f2;
        f2.libraryId = libId;
        f2.filename = "Zelda.nes";
        f2.originalPath = romDir + "/Zelda.nes";
        f2.currentPath = f2.originalPath;
        f2.extension = ".nes";
        f2.systemId = nesId;
        f2.hashCalculated = true;
        f2.crc32 = "CCDD3344";
        ids.append(db.insertFile(f2));

        // SNES file 1
        FileRecord f3;
        f3.libraryId = libId;
        f3.filename = "DonkeyKong.sfc";
        f3.originalPath = romDir + "/DonkeyKong.sfc";
        f3.currentPath = f3.originalPath;
        f3.extension = ".sfc";
        f3.systemId = snesId;
        f3.hashCalculated = true;
        f3.crc32 = "EEFF5566";
        ids.append(db.insertFile(f3));

        // Add a match for NES file 1
        int gameId = db.insertGame("Super Mario Bros.", nesId, "USA",
                                   "Nintendo", "Nintendo", "1985-09-13",
                                   "A classic platformer", "Platform", "1", 9.0f);
        Q_ASSERT(gameId > 0);
        db.insertMatch(ids[0], gameId, 95.0f, "hash");

        // Add a match for SNES file
        int gameId2 = db.insertGame("Donkey Kong Country", snesId, "USA",
                                    "Rare", "Rare", "1994-11-21",
                                    "Platformer with pre-rendered graphics",
                                    "Platform", "1-2", 8.5f);
        Q_ASSERT(gameId2 > 0);
        db.insertMatch(ids[2], gameId2, 80.0f, "fuzzy");

        return ids;
    }

private slots:

    // ── loadFromDatabase ──────────────────────────────────

    void testLoadFromDatabaseGrouping()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_screen.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));

        auto ids = populateTestData(app, tmp.path() + "/roms");
        QVERIFY(ids.size() == 3);

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Verify grouped entries: should have headers + files
        // Two systems → 2 headers + 3 files = 5 entries
        QCOMPARE(screen.allEntryCount(), 5);

        // First entry should be a system header
        QVERIFY(screen.allEntryAt(0).isHeader);

        // Verify that headers and non-headers alternate correctly
        int headerCount = 0;
        int fileCount = 0;
        for (int i = 0; i < screen.allEntryCount(); ++i) {
            if (screen.allEntryAt(i).isHeader) headerCount++;
            else fileCount++;
        }
        QCOMPARE(headerCount, 2);
        QCOMPARE(fileCount, 3);

        // The filtered list should also have 5 entries (no filter active)
        QCOMPARE(screen.entryCount(), 5);
        QCOMPARE(screen.fileCount(), 3);
        QCOMPARE(screen.systemCount(), 2);
    }

    void testLoadFromDatabaseMatchStatus()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_match_status.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));

        auto ids = populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Find the matched NES file (id == ids[0]) — should have "match ✓"
        bool foundMatched = false;
        bool foundUnmatched = false;
        for (int i = 0; i < screen.allEntryCount(); ++i) {
            const auto &e = screen.allEntryAt(i);
            if (e.isHeader) continue;
            if (e.fileId == ids[0]) {
                QVERIFY(e.confidence >= 90);
                QCOMPARE(e.matchStatus, std::string("match ✓"));
                foundMatched = true;
            }
            if (e.fileId == ids[1]) {
                // NES file 2 — no match inserted
                QCOMPARE(e.matchStatus, std::string("pending"));
                foundUnmatched = true;
            }
        }
        QVERIFY(foundMatched);
        QVERIFY(foundUnmatched);
    }

    // ── applyFilter ───────────────────────────────────────

    void testApplyFilterBySystem()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_filter_sys.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Filter by "super nintendo" (case-insensitive)
        screen.setFilter("super nintendo");
        screen.applyFilter();

        // Should have 1 header + 1 SNES file = 2 entries
        QCOMPARE(screen.entryCount(), 2);
        QVERIFY(screen.entryAt(0).isHeader);
        QCOMPARE(screen.entryAt(1).filename, std::string("DonkeyKong.sfc"));
    }

    void testApplyFilterByFilename()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_filter_fname.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Filter by "zelda" (matches filename)
        screen.setFilter("zelda");
        screen.applyFilter();

        // Should have 1 header + 1 file
        QCOMPARE(screen.entryCount(), 2);
        QCOMPARE(screen.entryAt(1).filename, std::string("Zelda.nes"));
    }

    void testApplyFilterByTitle()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_filter_title.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Filter by "donkey" (matches game title via match)
        screen.setFilter("donkey");
        screen.applyFilter();

        // Should match the SNES file (title = "Donkey Kong Country")
        int nonHeaderCount = 0;
        for (int i = 0; i < screen.entryCount(); ++i)
            if (!screen.entryAt(i).isHeader) nonHeaderCount++;
        QCOMPARE(nonHeaderCount, 1);
    }

    void testApplyFilterNoMatch()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_filter_none.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        screen.setFilter("xyznonexistentxyz");
        screen.applyFilter();

        QCOMPARE(screen.entryCount(), 0);
    }

    void testApplyFilterClearRestoresAll()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_filter_clear.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();
        int fullCount = screen.entryCount();

        // Apply filter
        screen.setFilter("zelda");
        screen.applyFilter();
        QVERIFY(screen.entryCount() < fullCount);

        // Clear filter
        screen.clearFilter();
        screen.applyFilter();
        QCOMPARE(screen.entryCount(), fullCount);
    }

    // ── confirmMatch / rejectMatch ────────────────────────

    void testConfirmMatch()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_confirm.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        auto ids = populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Select the matched NES file (ids[0])
        // Find its index in filtered entries
        int targetIdx = -1;
        for (int i = 0; i < screen.entryCount(); ++i) {
            if (screen.entryAt(i).fileId == ids[0]) {
                targetIdx = i;
                break;
            }
        }
        QVERIFY(targetIdx >= 0);
        screen.setSelectedIndex(targetIdx);

        screen.confirmMatch();

        // Verify in-memory update
        QCOMPARE(screen.entryAt(targetIdx).confirmStatus,
                 LibraryScreen::ConfirmationStatus::Confirmed);

        // Verify DB update
        auto mr = app.db().getMatchForFile(ids[0]);
        QVERIFY(mr.isConfirmed);
    }

    void testRejectMatch()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_reject.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        auto ids = populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Find the SNES matched file (ids[2])
        int targetIdx = -1;
        for (int i = 0; i < screen.entryCount(); ++i) {
            if (screen.entryAt(i).fileId == ids[2]) {
                targetIdx = i;
                break;
            }
        }
        QVERIFY(targetIdx >= 0);
        screen.setSelectedIndex(targetIdx);

        screen.rejectMatch();

        // Verify in-memory update
        QCOMPARE(screen.entryAt(targetIdx).confirmStatus,
                 LibraryScreen::ConfirmationStatus::Rejected);

        // Verify DB update
        auto mr = app.db().getMatchForFile(ids[2]);
        QVERIFY(mr.isRejected);
    }

    void testConfirmOnHeaderIsNoop()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString dbPath = tmp.path() + "/test_confirm_header.db";

        TuiApp app;
        QVERIFY(app.db().initialize(dbPath));
        populateTestData(app, tmp.path() + "/roms");

        LibraryScreen screen(app);
        screen.loadFromDatabase();

        // Select the first header
        int headerIdx = -1;
        for (int i = 0; i < screen.entryCount(); ++i) {
            if (screen.entryAt(i).isHeader) {
                headerIdx = i;
                break;
            }
        }
        QVERIFY(headerIdx >= 0);
        screen.setSelectedIndex(headerIdx);

        // Should be a no-op (no crash, no DB change)
        screen.confirmMatch();
        // Verify header is still a header
        QVERIFY(screen.entryAt(headerIdx).isHeader);
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestLibraryScreen t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_library_screen.moc"
