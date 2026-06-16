#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/core/database.h"
#include "../src/core/disc_set_utils.h"

using namespace Remus;

class DatabaseDiscSetsTest : public QObject {
    Q_OBJECT

private slots:
    void rebuildDiscSetsForLibrary_groupsFilenameSets();
    void rebuildDiscSetsForLibrary_mergesConfirmedGame();
};

void DatabaseDiscSetsTest::rebuildDiscSetsForLibrary_groupsFilenameSets() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    FileRecord disc1;
    disc1.libraryId = libId;
    disc1.filename = QStringLiteral("Final Fantasy VII (Disc 1).7z");
    disc1.originalPath = QStringLiteral("/roms/ff7-1.7z");
    disc1.currentPath = disc1.originalPath;
    disc1.extension = QStringLiteral(".7z");
    FileRecord disc2 = disc1;
    disc2.filename = QStringLiteral("Final Fantasy VII (Disc 2).7z");
    disc2.originalPath = QStringLiteral("/roms/ff7-2.7z");
    disc2.currentPath = disc2.originalPath;

    const int id1 = db.insertFile(disc1);
    const int id2 = db.insertFile(disc2);
    QVERIFY(id1 > 0);
    QVERIFY(id2 > 0);
    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    const FileRecord updated1 = db.getFileById(id1);
    const FileRecord updated2 = db.getFileById(id2);
    QVERIFY(!updated1.discSetKey.isEmpty());
    QCOMPARE(updated1.discSetKey, updated2.discSetKey);
    QCOMPARE(updated1.discNumber, 1);
    QCOMPARE(updated2.discNumber, 2);
}

void DatabaseDiscSetsTest::rebuildDiscSetsForLibrary_mergesConfirmedGame() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    const int libId = db.insertLibrary("/roms", "Test");
    const int sysId = db.getSystemId(QStringLiteral("PlayStation"));
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    FileRecord disc1;
    disc1.libraryId = libId;
    disc1.systemId = sysId;
    disc1.filename = QStringLiteral("ff7_d1.chd");
    disc1.originalPath = QStringLiteral("/roms/ff7_d1.chd");
    disc1.currentPath = disc1.originalPath;
    disc1.extension = QStringLiteral(".chd");
    FileRecord disc2 = disc1;
    disc2.filename = QStringLiteral("ff7_d2.chd");
    disc2.originalPath = QStringLiteral("/roms/ff7_d2.chd");
    disc2.currentPath = disc2.originalPath;

    const int id1 = db.insertFile(disc1);
    const int id2 = db.insertFile(disc2);
    QVERIFY(id1 > 0);
    QVERIFY(id2 > 0);

    const int gameId = db.insertGame(QStringLiteral("Final Fantasy VII"), sysId);
    QVERIFY(gameId > 0);
    QVERIFY(db.insertMatch(id1, gameId, 100.0f, QStringLiteral("test")));
    QVERIFY(db.insertMatch(id2, gameId, 100.0f, QStringLiteral("test")));
    QVERIFY(db.confirmMatch(id1));
    QVERIFY(db.confirmMatch(id2));

    const FileRecord updated1 = db.getFileById(id1);
    const FileRecord updated2 = db.getFileById(id2);
    QCOMPARE(updated1.discSetKey, DiscSetUtils::gameDiscSetKey(gameId, sysId));
    QCOMPARE(updated2.discSetKey, updated1.discSetKey);
}

QTEST_MAIN(DatabaseDiscSetsTest)
#include "test_database_disc_sets.moc"
