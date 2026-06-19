#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/metadata/compendium_compiler_service.h"

using namespace Remus::Compendium;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery q(db);
    return q.exec(sql);
}

// Creates a minimal in-memory database containing the tables used by
// deduplicateGames.  Returns the open connection name so the caller can remove
// it after the test.
QString openDedupDb(QSqlDatabase &db) {
    const QString name = QStringLiteral("dedup_test_%1").arg(QDateTime::currentMSecsSinceEpoch());
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(QStringLiteral(":memory:"));
    if (!db.open())
        return { };

    const bool ok = execSql(db,
                        QStringLiteral("CREATE TABLE games ("
                                       "game_id TEXT PRIMARY KEY,"
                                       " system_id INTEGER NOT NULL,"
                                       " canonical_title TEXT NOT NULL,"
                                       " primary_region_code TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_names ("
                           "game_name_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " name_text TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_signatures ("
                           "signature_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " hash_value TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_serials ("
                           "serial_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " serial_value TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_facts ("
                           "fact_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " field_name TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_disc_sets ("
                           "disc_set_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " set_key TEXT NOT NULL,"
                           " disc_number INTEGER NOT NULL DEFAULT 0,"
                           " disc_count INTEGER NOT NULL DEFAULT 0,"
                           " set_variant TEXT NOT NULL DEFAULT '',"
                           " set_role TEXT NOT NULL DEFAULT 'game',"
                           " title_disc TEXT NOT NULL,"
                           " source_id TEXT NOT NULL,"
                           " snapshot_id TEXT NOT NULL DEFAULT '')"));

    return ok ? name : QString { };
}

} // namespace

class CompendiumDedupTest : public QObject {
    Q_OBJECT

private slots:
    void deduplicateGames_mergesDuplicatePair();
    void deduplicateGames_reassignsChildRows();
    void deduplicateGames_noOpWhenNoDuplicates();
    void deduplicateGames_mergesSameSerialDifferentTitles();
    void deduplicateGames_reassignsDiscSets();
};

void CompendiumDedupTest::deduplicateGames_mergesDuplicatePair() {
    QSqlDatabase db;
    const QString connName = openDedupDb(db);
    QVERIFY(!connName.isEmpty());

    // Two games with the same system + title → duplicates.
    // dup-2 has one signature so it wins (higher sig_count);
    // dup-1 has none and will be the loser.
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('dup-1', 1, 'Duplicate Game', NULL)")));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('dup-2', 1, 'Duplicate Game', NULL)")));
    QVERIFY(
        execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_value) VALUES ('dup-2', 'abc123')")));

    QString error;
    const int merged = deduplicateGames(db, error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(merged, 1);

    // Only the winner (dup-2) should remain.
    QSqlQuery countQ(db);
    QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM games")));
    QVERIFY(countQ.next());
    QCOMPARE(countQ.value(0).toInt(), 1);

    QSqlQuery winnerQ(db);
    QVERIFY(winnerQ.exec(QStringLiteral("SELECT game_id FROM games")));
    QVERIFY(winnerQ.next());
    QCOMPARE(winnerQ.value(0).toString(), QStringLiteral("dup-2"));

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumDedupTest::deduplicateGames_reassignsChildRows() {
    QSqlDatabase db;
    const QString connName = openDedupDb(db);
    QVERIFY(!connName.isEmpty());

    // dup-1 is the loser (no signatures); dup-2 is the winner (one signature).
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('dup-1', 1, 'Duplicate Game', NULL)")));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('dup-2', 1, 'Duplicate Game', NULL)")));
    QVERIFY(
        execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_value) VALUES ('dup-2', 'abc123')")));
    // A name owned by the loser — should be reassigned to the winner.
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO game_names (game_id, name_text) VALUES ('dup-1', 'Alt Name')")));

    QString error;
    QCOMPARE(deduplicateGames(db, error), 1);
    QVERIFY(error.isEmpty());

    // The name row must now belong to the winner.
    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT game_id FROM game_names WHERE name_text = 'Alt Name'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("dup-2"));

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumDedupTest::deduplicateGames_noOpWhenNoDuplicates() {
    QSqlDatabase db;
    const QString connName = openDedupDb(db);
    QVERIFY(!connName.isEmpty());

    // Two games with distinct titles — nothing to merge.
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('game-a', 1, 'Title Alpha', NULL)")));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('game-b', 1, 'Title Beta', NULL)")));

    QString error;
    QCOMPARE(deduplicateGames(db, error), 0);
    QVERIFY(error.isEmpty());

    QSqlQuery countQ(db);
    QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM games")));
    QVERIFY(countQ.next());
    QCOMPARE(countQ.value(0).toInt(), 2);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumDedupTest::deduplicateGames_mergesSameSerialDifferentTitles() {
    QSqlDatabase db;
    const QString connName = openDedupDb(db);
    QVERIFY(!connName.isEmpty());

    QVERIFY(execSql(
        db, QStringLiteral("INSERT INTO games VALUES ('serial-a', 16, 'Need for Speed - Shift (USA)', 'USA')")));
    QVERIFY(execSql(
        db, QStringLiteral("INSERT INTO games VALUES ('serial-b', 16, 'Need for Speed - Shift (USA) (PSN)', 'USA')")));
    QVERIFY(
        execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_value) VALUES ('serial-a', 'hash-a')")));
    QVERIFY(
        execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_value) VALUES ('serial-b', 'hash-b')")));
    QVERIFY(execSql(
        db, QStringLiteral("INSERT INTO game_serials (game_id, serial_value) VALUES ('serial-a', 'ULUS-10462')")));
    QVERIFY(execSql(
        db, QStringLiteral("INSERT INTO game_serials (game_id, serial_value) VALUES ('serial-b', 'ULUS-10462')")));

    QString error;
    const int merged = deduplicateGames(db, error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(merged, 1);

    QSqlQuery countQ(db);
    QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM games")));
    QVERIFY(countQ.next());
    QCOMPARE(countQ.value(0).toInt(), 1);

    QSqlQuery serialQ(db);
    QVERIFY(serialQ.exec(
        QStringLiteral("SELECT COUNT(DISTINCT game_id) FROM game_serials WHERE serial_value = 'ULUS-10462'")));
    QVERIFY(serialQ.next());
    QCOMPARE(serialQ.value(0).toInt(), 1);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumDedupTest::deduplicateGames_reassignsDiscSets() {
    QSqlDatabase db;
    const QString connName = openDedupDb(db);
    QVERIFY(!connName.isEmpty());

    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('dup-1', 1, 'Duplicate Game', NULL)")));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('dup-2', 1, 'Duplicate Game', NULL)")));
    QVERIFY(
        execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_value) VALUES ('dup-2', 'abc123')")));
    QVERIFY(execSql(db,
        QStringLiteral("INSERT INTO game_disc_sets (game_id, set_key, disc_number, disc_count, "
                       "set_variant, set_role, title_disc, source_id, snapshot_id) "
                       "VALUES ('dup-1', 'setkey1', 1, 1, '', 'game', 'Duplicate Game', 'redump', "
                       "'snap')")));

    QString error;
    QCOMPARE(deduplicateGames(db, error), 1);
    QVERIFY(error.isEmpty());

    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT game_id FROM game_disc_sets WHERE set_key = 'setkey1'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("dup-2"));

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

QTEST_MAIN(CompendiumDedupTest)
#include "test_compendium_dedup.moc"
