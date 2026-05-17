#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "../src/metadata/compendium_merge_resolver.h"

using namespace Remus::Compendium;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    return query.exec(sql);
}

bool createSchema(QSqlDatabase &db)
{
    return execSql(db, QStringLiteral(
               "CREATE TABLE games ("
               "game_id TEXT PRIMARY KEY, "
               "system_id INTEGER NOT NULL DEFAULT 1, "
               "canonical_title TEXT NOT NULL, "
               "primary_region_code TEXT, "
               "release_date TEXT, "
               "release_year INTEGER, "
               "developer TEXT, "
               "publisher TEXT, "
               "genre TEXT, "
               "players_max INTEGER, "
               "description TEXT, "
               "rating REAL, "
               "canonical_confidence REAL NOT NULL DEFAULT 0)"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE game_facts ("
               "fact_id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "game_id TEXT NOT NULL, "
               "field_name TEXT NOT NULL, "
               "field_value TEXT NOT NULL, "
               "value_type TEXT NOT NULL DEFAULT 'text', "
               "source_id TEXT NOT NULL DEFAULT 'test', "
               "snapshot_id TEXT NOT NULL DEFAULT '', "
               "source_item_id INTEGER, "
               "source_priority INTEGER NOT NULL DEFAULT 100, "
               "confidence REAL NOT NULL DEFAULT 1.0)"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE canonical_resolution ("
               "game_id TEXT NOT NULL, "
               "field_name TEXT NOT NULL, "
               "selected_fact_id INTEGER NOT NULL, "
               "resolved_by_rule TEXT NOT NULL, "
               "PRIMARY KEY (game_id, field_name))"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE merge_conflicts ("
               "conflict_id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "game_id TEXT NOT NULL, "
               "field_name TEXT NOT NULL, "
               "fact_ids_json TEXT NOT NULL, "
               "resolution_status TEXT NOT NULL DEFAULT 'unresolved', "
               "chosen_fact_id INTEGER, "
               "notes TEXT, "
               "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
               "resolved_at TEXT)"));
}

bool seedConflictingFacts(QSqlDatabase &db)
{
    if (!execSql(db, QStringLiteral(
            "INSERT INTO games (game_id, canonical_title) VALUES ('game-1', 'Test Game')"))) {
        return false;
    }

    return execSql(db, QStringLiteral(
               "INSERT INTO game_facts (game_id, field_name, field_value, source_priority, confidence) "
               "VALUES ('game-1', 'developer', 'Studio A', 100, 1.0)"))
        && execSql(db, QStringLiteral(
               "INSERT INTO game_facts (game_id, field_name, field_value, source_priority, confidence) "
               "VALUES ('game-1', 'developer', 'Studio B', 90, 0.8)"));
}

int scalarInt(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

} // namespace

class CompendiumMergeResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void resolve_replacesConflictRowsOnRepeatedRuns();
};

void CompendiumMergeResolverTest::resolve_replacesConflictRowsOnRepeatedRuns()
{
    const QString connectionName = QStringLiteral("compendium_merge_resolver_test");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));
        QVERIFY(seedConflictingFacts(db));

        MergeResolver resolver;
        CompilerStats firstStats;
        QString error;
        QVERIFY2(resolver.resolve(db, firstStats, error), qPrintable(error));
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM merge_conflicts")), 1);
        QCOMPARE(firstStats.unresolvedConflicts, 0);

        CompilerStats secondStats;
        error.clear();
        QVERIFY2(resolver.resolve(db, secondStats, error), qPrintable(error));
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM merge_conflicts")), 1);
        QCOMPARE(secondStats.unresolvedConflicts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM canonical_resolution WHERE field_name = 'developer'")), 1);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumMergeResolverTest)

#include "test_compendium_merge_resolver.moc"