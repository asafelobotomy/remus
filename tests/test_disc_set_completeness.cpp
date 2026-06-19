#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "../src/core/compendium_disc_bridge.h"
#include "../src/core/database.h"
#include "../src/core/disc_set_key.h"
#include "../src/core/verification_engine.h"

using namespace Remus;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery query(db);
    return query.exec(sql);
}

QString createCompendiumFixture() {
    QTemporaryFile file;
    file.setAutoRemove(false);
    if (!file.open())
        return { };
    file.close();

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("disc_complete_compendium"));
    db.setDatabaseName(file.fileName());
    if (!db.open())
        return { };

    const QString setKey
        = DiscSetKey::compute(14, QStringLiteral("Final Fantasy VII (USA) (Disc 1)"), QStringLiteral("USA"));
    const QString entry1 = QStringLiteral("Sony - PlayStation|Final Fantasy VII (USA) (Disc 1)|FF7 Disc 1.bin");
    const QString entry2 = QStringLiteral("Sony - PlayStation|Final Fantasy VII (USA) (Disc 2)|FF7 Disc 2.bin");
    const QString entry3 = QStringLiteral("Sony - PlayStation|Final Fantasy VII (USA) (Disc 3)|FF7 Disc 3.bin");

    const bool ok = execSql(db,
                        QStringLiteral("CREATE TABLE systems (system_id INTEGER PRIMARY KEY, internal_name TEXT NOT "
                                       "NULL, display_name TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE games (game_id TEXT PRIMARY KEY, system_id INTEGER NOT NULL, canonical_title "
                           "TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_signatures (signature_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT "
                           "NOT NULL, hash_type TEXT NOT NULL, hash_value TEXT NOT NULL, source_entry_key TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_disc_sets (disc_set_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT "
                           "NOT NULL, set_key TEXT NOT NULL, disc_number INTEGER NOT NULL, disc_count INTEGER NOT "
                           "NULL, set_variant TEXT NOT NULL DEFAULT '', set_role TEXT NOT NULL DEFAULT 'game', "
                           "title_disc TEXT NOT NULL, source_id TEXT NOT NULL, snapshot_id TEXT NOT NULL DEFAULT '')"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_disc_tracks (track_id INTEGER PRIMARY KEY AUTOINCREMENT, disc_set_id "
                           "INTEGER NOT NULL, track_index INTEGER NOT NULL, rom_name TEXT NOT NULL, signature_id "
                           "INTEGER, source_entry_key TEXT NOT NULL UNIQUE)"))
        && execSql(db, QStringLiteral("INSERT INTO systems VALUES (14, 'PlayStation', 'Sony PlayStation')"))
        && execSql(db, QStringLiteral("INSERT INTO games VALUES ('ff7', 14, 'Final Fantasy VII')"))
        && execSql(db,
            QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value, source_entry_key) VALUES "
                           "('ff7', 'md5', '11111111111111111111111111111111', '%1'), "
                           "('ff7', 'md5', '22222222222222222222222222222222', '%2'), "
                           "('ff7', 'md5', '33333333333333333333333333333333', '%3')")
                .arg(entry1, entry2, entry3))
        && execSql(db,
            QStringLiteral("INSERT INTO game_disc_sets (game_id, set_key, disc_number, disc_count, title_disc, "
                           "source_id, snapshot_id) VALUES "
                           "('ff7', '%1', 1, 3, 'Final Fantasy VII (USA) (Disc 1)', 'redump', 'snap'), "
                           "('ff7', '%1', 2, 3, 'Final Fantasy VII (USA) (Disc 2)', 'redump', 'snap'), "
                           "('ff7', '%1', 3, 3, 'Final Fantasy VII (USA) (Disc 3)', 'redump', 'snap')")
                .arg(setKey))
        && execSql(db,
            QStringLiteral("INSERT INTO game_disc_tracks (disc_set_id, track_index, rom_name, signature_id, "
                           "source_entry_key) VALUES "
                           "(1, 1, 'FF7 Disc 1.bin', 1, '%1'), "
                           "(2, 1, 'FF7 Disc 2.bin', 2, '%2'), "
                           "(3, 1, 'FF7 Disc 3.bin', 3, '%3')")
                .arg(entry1, entry2, entry3));

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("disc_complete_compendium"));
    return ok ? file.fileName() : QString();
}

} // namespace

class DiscSetCompletenessTest : public QObject {
    Q_OBJECT

private slots:
    void ff7Disc2Only_reportsMissingDiscs();
    void rebuildDiscSets_usesCompendiumSetKey();
    void catalogDiscSetSummary_returnsExpectedCounts();
};

void DiscSetCompletenessTest::ff7Disc2Only_reportsMissingDiscs() {
    const QString compendiumPath = createCompendiumFixture();
    QVERIFY(!compendiumPath.isEmpty());

    Database db;
    QVERIFY(db.initialize(QStringLiteral(":memory:")));
    db.setCompendiumDbPath(compendiumPath);

    const int libId = db.insertLibrary(QStringLiteral("/roms"), QStringLiteral("Test"));
    const int sysId = db.getSystemId(QStringLiteral("PlayStation"));
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    FileRecord disc2;
    disc2.libraryId = libId;
    disc2.systemId = sysId;
    disc2.filename = QStringLiteral("FF7 Disc 2.bin");
    disc2.originalPath = QStringLiteral("/roms/ff7_d2.bin");
    disc2.currentPath = disc2.originalPath;
    disc2.extension = QStringLiteral(".bin");
    disc2.md5 = QStringLiteral("22222222222222222222222222222222");
    disc2.hashCalculated = true;

    const int fileId = db.insertFile(disc2);
    QVERIFY(fileId > 0);
    QVERIFY(db.updateFileHashes(fileId, QString(), disc2.md5, QString()));
    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    const FileRecord updated = db.getFileById(fileId);
    QVERIFY(!updated.discSetKey.isEmpty());
    QCOMPARE(updated.discNumber, 2);

    VerificationEngine engine(&db);
    engine.setCompendiumDb(compendiumPath);

    const DiscSetCompletenessReport report = engine.discSetCompletenessBySetKey(updated.discSetKey, { fileId });
    QCOMPARE(report.discCount, 3);
    QCOMPARE(report.ownedDiscNumbers, QList<int>({ 2 }));
    QCOMPARE(report.missingDiscNumbers, QList<int>({ 1, 3 }));
}

void DiscSetCompletenessTest::rebuildDiscSets_usesCompendiumSetKey() {
    const QString compendiumPath = createCompendiumFixture();
    QVERIFY(!compendiumPath.isEmpty());

    Database db;
    QVERIFY(db.initialize(QStringLiteral(":memory:")));
    db.setCompendiumDbPath(compendiumPath);

    const int libId = db.insertLibrary(QStringLiteral("/roms"), QStringLiteral("Test"));
    const int sysId = db.getSystemId(QStringLiteral("PlayStation"));
    if (sysId == 0)
        QSKIP("PlayStation system not in default DB");

    FileRecord disc2;
    disc2.libraryId = libId;
    disc2.systemId = sysId;
    disc2.filename = QStringLiteral("ff7_d2.bin");
    disc2.originalPath = QStringLiteral("/roms/ff7_d2.bin");
    disc2.currentPath = disc2.originalPath;
    disc2.extension = QStringLiteral(".bin");
    disc2.md5 = QStringLiteral("22222222222222222222222222222222");
    disc2.hashCalculated = true;

    const int fileId = db.insertFile(disc2);
    QVERIFY(fileId > 0);
    QVERIFY(db.updateFileHashes(fileId, QString(), disc2.md5, QString()));
    QVERIFY(db.rebuildDiscSetsForLibrary(libId));

    const QString expectedKey
        = DiscSetKey::compute(14, QStringLiteral("Final Fantasy VII (USA) (Disc 1)"), QStringLiteral("USA"));
    QCOMPARE(db.getFileById(fileId).discSetKey, expectedKey);
}

void DiscSetCompletenessTest::catalogDiscSetSummary_returnsExpectedCounts() {
    const QString compendiumPath = createCompendiumFixture();
    QVERIFY(!compendiumPath.isEmpty());

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("catalog_summary_test"));
    db.setDatabaseName(compendiumPath);
    QVERIFY(db.open());

    const QString setKey
        = DiscSetKey::compute(14, QStringLiteral("Final Fantasy VII (USA) (Disc 1)"), QStringLiteral("USA"));
    CatalogDiscSetSummary summary;
    QVERIFY(lookupCatalogDiscSetSummary(db, setKey, summary));
    QCOMPARE(summary.catalogDiscCount, 3);
    QVERIFY(summary.baseTitle.contains(QStringLiteral("Final Fantasy VII")));

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("catalog_summary_test"));
}

QTEST_MAIN(DiscSetCompletenessTest)
#include "test_disc_set_completeness.moc"
