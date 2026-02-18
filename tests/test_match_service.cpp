/**
 * @file test_match_service.cpp
 * @brief Unit tests for MatchService (confirmMatch, rejectMatch, getAllMatches)
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/services/match_service.h"
#include "../src/core/database.h"

using namespace Remus;

class TestMatchService : public QObject
{
    Q_OBJECT

private:
    /// Populate a DB with a file + match, return {fileId, gameId}.
    std::pair<int, int> populateFixture(Database &db)
    {
        int libId = db.insertLibrary("/tmp/roms");
        Q_ASSERT(libId > 0);

        // Use pre-populated default system
        int sysId = db.getSystemId("NES");
        Q_ASSERT(sysId > 0);

        FileRecord fr;
        fr.libraryId = libId;
        fr.filename = "TestRom.nes";
        fr.originalPath = "/tmp/roms/TestRom.nes";
        fr.currentPath = fr.originalPath;
        fr.extension = ".nes";
        fr.systemId = sysId;
        fr.crc32 = "AABB1122";
        fr.hashCalculated = true;
        int fileId = db.insertFile(fr);
        Q_ASSERT(fileId > 0);

        int gameId = db.insertGame("Test Game", sysId, "USA",
                                   "Pub", "Dev", "1990-01-01",
                                   "Desc", "Action", "1", 7.5f);
        Q_ASSERT(gameId > 0);

        db.insertMatch(fileId, gameId, 90.0f, "hash");
        return {fileId, gameId};
    }

private slots:

    void testConfirmMatch()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Database db;
        QVERIFY(db.initialize(tmp.path() + "/confirm.db"));
        auto [fileId, gameId] = populateFixture(db);

        MatchService svc;
        QVERIFY(svc.confirmMatch(&db, fileId));

        auto mr = db.getMatchForFile(fileId);
        QVERIFY(mr.isConfirmed);
        QVERIFY(!mr.isRejected);
    }

    void testRejectMatch()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Database db;
        QVERIFY(db.initialize(tmp.path() + "/reject.db"));
        auto [fileId, gameId] = populateFixture(db);

        MatchService svc;
        QVERIFY(svc.rejectMatch(&db, fileId));

        auto mr = db.getMatchForFile(fileId);
        QVERIFY(mr.isRejected);
        QVERIFY(!mr.isConfirmed);
    }

    void testGetAllMatches()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Database db;
        QVERIFY(db.initialize(tmp.path() + "/getall.db"));
        auto [fileId, gameId] = populateFixture(db);

        MatchService svc;
        auto matches = svc.getAllMatches(&db);
        QVERIFY(!matches.isEmpty());
        QVERIFY(matches.contains(fileId));
        QCOMPARE(matches[fileId].gameTitle, QString("Test Game"));
    }

    void testGetMatchForFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Database db;
        QVERIFY(db.initialize(tmp.path() + "/getone.db"));
        auto [fileId, gameId] = populateFixture(db);

        MatchService svc;
        auto mr = svc.getMatchForFile(&db, fileId);
        QVERIFY(mr.matchId > 0);
        QVERIFY(qFuzzyCompare(mr.confidence, 90.0f));
    }

    void testGetMatchForNonexistentFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Database db;
        QVERIFY(db.initialize(tmp.path() + "/nomatch.db"));
        populateFixture(db);

        MatchService svc;
        auto mr = svc.getMatchForFile(&db, 99999);
        QCOMPARE(mr.matchId, 0);
    }

    void testConfirmNonexistentFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Database db;
        QVERIFY(db.initialize(tmp.path() + "/confirm_bad.db"));

        MatchService svc;
        // Should not crash
        bool result = svc.confirmMatch(&db, 99999);
        Q_UNUSED(result);
    }
};

QTEST_MAIN(TestMatchService)
#include "test_match_service.moc"
