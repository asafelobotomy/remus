#include <QtTest/QtTest>

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/metadata/compendium_source_purge.h"

using namespace Remus::Compendium;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery q(db);
    return q.exec(sql);
}

QString openPurgeDb(QSqlDatabase &db) {
    const QString name = QStringLiteral("purge_test_%1").arg(QDateTime::currentMSecsSinceEpoch());
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(QStringLiteral(":memory:"));
    if (!db.open())
        return { };

    const bool ok = execSql(db, QStringLiteral("CREATE TABLE games (game_id TEXT PRIMARY KEY)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE source_items ("
                           "source_item_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " source_id TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_signatures ("
                           "signature_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " source_id TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_serials ("
                           "serial_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " source_id TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_names ("
                           "game_name_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " source_id TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_facts ("
                           "fact_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " source_id TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE canonical_resolution ("
                           "game_id TEXT NOT NULL,"
                           " field_name TEXT NOT NULL,"
                           " selected_fact_id INTEGER NOT NULL,"
                           " PRIMARY KEY (game_id, field_name))"))
        && execSql(db,
            QStringLiteral("CREATE TABLE merge_conflicts ("
                           "conflict_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " field_name TEXT NOT NULL,"
                           " chosen_fact_id INTEGER)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_disc_sets ("
                           "disc_set_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " game_id TEXT NOT NULL,"
                           " source_id TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_disc_tracks ("
                           "track_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                           " disc_set_id INTEGER NOT NULL,"
                           " signature_id INTEGER)"));

    return ok ? name : QString { };
}

int countForSource(QSqlDatabase &db, const QString &table, const QString &sourceId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE source_id = ?").arg(table));
    q.addBindValue(sourceId);
    if (!q.exec() || !q.next())
        return -1;
    return q.value(0).toInt();
}

} // namespace

class CompendiumSourcePurgeTest : public QObject {
    Q_OBJECT

private slots:
    void purgeSourceIngestData_removesIngestRowsButKeepsGames();
    void purgeSourceIngestData_rejectsEmptySourceId();
};

void CompendiumSourcePurgeTest::purgeSourceIngestData_removesIngestRowsButKeepsGames() {
    QSqlDatabase db;
    const QString connName = openPurgeDb(db);
    QVERIFY(!connName.isEmpty());

    const QString keepSource = QStringLiteral("keep-source");
    const QString purgeSource = QStringLiteral("purge-source");

    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games VALUES ('game-1')")));
    QVERIFY(execSql(
        db, QStringLiteral("INSERT INTO source_items (source_id) VALUES ('%1'), ('%2')").arg(keepSource, purgeSource)));
    QVERIFY(execSql(db,
        QStringLiteral("INSERT INTO game_signatures (game_id, source_id) VALUES ('game-1', '%1'), ('game-1', '%2')")
            .arg(keepSource, purgeSource)));
    QVERIFY(execSql(db,
        QStringLiteral("INSERT INTO game_serials (game_id, source_id) VALUES ('game-1', '%1'), ('game-1', '%2')")
            .arg(keepSource, purgeSource)));
    QVERIFY(execSql(db,
        QStringLiteral("INSERT INTO game_names (game_id, source_id) VALUES ('game-1', '%1'), ('game-1', '%2')")
            .arg(keepSource, purgeSource)));
    QVERIFY(execSql(db,
        QStringLiteral("INSERT INTO game_facts (game_id, source_id) VALUES ('game-1', '%1'), ('game-1', '%2')")
            .arg(keepSource, purgeSource)));
    QVERIFY(execSql(db,
        QStringLiteral("INSERT INTO game_disc_sets (game_id, source_id) VALUES ('game-1', '%1'), ('game-1', '%2')")
            .arg(keepSource, purgeSource)));

    QSqlQuery discSetQ(db);
    QVERIFY(discSetQ.exec(
        QStringLiteral("SELECT disc_set_id FROM game_disc_sets WHERE source_id = '%1'").arg(purgeSource)));
    QVERIFY(discSetQ.next());
    const int purgeDiscSetId = discSetQ.value(0).toInt();
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO game_disc_tracks (disc_set_id) VALUES (%1)").arg(purgeDiscSetId)));

    QString error;
    QVERIFY2(purgeSourceIngestData(db, purgeSource, error), qPrintable(error));

    QCOMPARE(countForSource(db, QStringLiteral("source_items"), purgeSource), 0);
    QCOMPARE(countForSource(db, QStringLiteral("game_signatures"), purgeSource), 0);
    QCOMPARE(countForSource(db, QStringLiteral("game_serials"), purgeSource), 0);
    QCOMPARE(countForSource(db, QStringLiteral("game_names"), purgeSource), 0);
    QCOMPARE(countForSource(db, QStringLiteral("game_facts"), purgeSource), 0);
    QCOMPARE(countForSource(db, QStringLiteral("game_disc_sets"), purgeSource), 0);

    QSqlQuery trackQ(db);
    QVERIFY(trackQ.exec(QStringLiteral("SELECT COUNT(*) FROM game_disc_tracks")));
    QVERIFY(trackQ.next());
    QCOMPARE(trackQ.value(0).toInt(), 0);

    QCOMPARE(countForSource(db, QStringLiteral("source_items"), keepSource), 1);
    QCOMPARE(countForSource(db, QStringLiteral("game_signatures"), keepSource), 1);

    QSqlQuery gameQ(db);
    QVERIFY(gameQ.exec(QStringLiteral("SELECT COUNT(*) FROM games")));
    QVERIFY(gameQ.next());
    QCOMPARE(gameQ.value(0).toInt(), 1);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumSourcePurgeTest::purgeSourceIngestData_rejectsEmptySourceId() {
    QSqlDatabase db;
    const QString connName = openPurgeDb(db);
    QVERIFY(!connName.isEmpty());

    QString error;
    QVERIFY(!purgeSourceIngestData(db, QString(), error));
    QVERIFY2(!error.isEmpty(), "Expected error for empty source_id");

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

QTEST_MAIN(CompendiumSourcePurgeTest)
#include "test_compendium_source_purge.moc"
