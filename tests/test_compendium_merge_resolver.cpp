#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "../src/metadata/compendium_merge_resolver.h"

using namespace Remus::Compendium;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery query(db);
    return query.exec(sql);
}

bool createSchema(QSqlDatabase &db) {
    return execSql(db,
               QStringLiteral("CREATE TABLE games ("
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
        && execSql(db,
            QStringLiteral("CREATE TABLE game_facts ("
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
        && execSql(db,
            QStringLiteral("CREATE TABLE canonical_resolution ("
                           "game_id TEXT NOT NULL, "
                           "field_name TEXT NOT NULL, "
                           "selected_fact_id INTEGER NOT NULL, "
                           "resolved_by_rule TEXT NOT NULL, "
                           "PRIMARY KEY (game_id, field_name))"))
        && execSql(db,
            QStringLiteral("CREATE TABLE merge_conflicts ("
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

bool seedConflictingFacts(QSqlDatabase &db) {
    if (!execSql(db, QStringLiteral("INSERT INTO games (game_id, canonical_title) VALUES ('game-1', 'Test Game')"))) {
        return false;
    }

    return execSql(db,
               QStringLiteral("INSERT INTO game_facts (game_id, field_name, field_value, source_priority, confidence) "
                              "VALUES ('game-1', 'developer', 'Studio A', 100, 1.0)"))
        && execSql(db,
            QStringLiteral("INSERT INTO game_facts (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-1', 'developer', 'Studio B', 90, 0.8)"));
}

int scalarInt(QSqlDatabase &db, const QString &sql) {
    QSqlQuery query(db);
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

} // namespace

class CompendiumMergeResolverTest : public QObject {
    Q_OBJECT

private slots:
    void resolve_replacesConflictRowsOnRepeatedRuns();
    void resolve_materializesCanonicalTitleFromTitleFact();
    void resolve_genericPassDoesNotOverwriteTitle();
    void resolve_releaseYearDerivedFromReleaseDateWhenNoYearFact();
    void resolve_playersMax_rejectsNonPureNumeric();
};

void CompendiumMergeResolverTest::resolve_replacesConflictRowsOnRepeatedRuns() {
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
        QCOMPARE(
            scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM canonical_resolution WHERE field_name = 'developer'")),
            1);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumMergeResolverTest::resolve_materializesCanonicalTitleFromTitleFact() {
    // Verifies that a 'title' fact with higher confidence wins over the
    // first-writer-wins ensureGame title and updates games.canonical_title
    // and games.canonical_confidence via the merge resolver.
    const QString connectionName = QStringLiteral("compendium_merge_resolver_title_test");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // Seed a game whose canonical_title came from first-writer (low confidence).
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, canonical_title, canonical_confidence) "
                           "VALUES ('game-t1', 'Bad Title', 0)")));
        // Insert a 'title' fact with high confidence — should win and update the game row.
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_facts "
                           "  (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-t1', 'title', 'Good Title', 100, 1.0)")));

        MergeResolver resolver;
        CompilerStats stats;
        QString error;
        QVERIFY2(resolver.resolve(db, stats, error), qPrintable(error));

        // canonical_resolution must record the 'title' field entry.
        QCOMPARE(
            scalarInt(db,
                QStringLiteral(
                    "SELECT COUNT(*) FROM canonical_resolution WHERE game_id = 'game-t1' AND field_name = 'title'")),
            1);

        // games.canonical_title must be updated to the winning fact value.
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT canonical_title, canonical_confidence FROM games WHERE game_id = 'game-t1'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Good Title"));
        QVERIFY(q.value(1).toDouble() > 0.0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumMergeResolverTest::resolve_genericPassDoesNotOverwriteTitle() {
    // R1: The step-7 generic resolver uses INSERT OR REPLACE and historically
    // excluded only 'canonical_title' from its field_name filter.  Since title
    // facts are stored under field_name='title', step 7 would re-run and replace
    // the step-1 result with a plain highest-priority pick — losing confidence-
    // weighted and source-priority tie-breaking done in step 1.  This test
    // verifies that a 'title' fact resolved in step 1 is NOT overwritten.
    const QString connectionName = QStringLiteral("resolver_generic_no_overwrite_title");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, canonical_title, canonical_confidence) "
                           "VALUES ('game-r1', 'Initial Placeholder', 0)")));
        // One 'title' fact — step 1 should resolve it; step 7 must leave it alone.
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_facts "
                           "  (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-r1', 'title', 'Correct Title', 100, 1.0)")));

        MergeResolver resolver;
        CompilerStats stats;
        QString error;
        QVERIFY2(resolver.resolve(db, stats, error), qPrintable(error));

        // canonical_resolution must have exactly one row for field_name='title',
        // and it must point at the fact we inserted (not a generic-pass duplicate).
        QCOMPARE(scalarInt(db,
                     QStringLiteral("SELECT COUNT(*) FROM canonical_resolution "
                                    "WHERE game_id = 'game-r1' AND field_name = 'title'")),
            1);

        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT canonical_title FROM games WHERE game_id = 'game-r1'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Correct Title"));

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumMergeResolverTest::resolve_releaseYearDerivedFromReleaseDateWhenNoYearFact() {
    // R2: When a game has only a release_date fact (e.g. '2001-06-15') and no
    // separate release_year fact, steps 3 and 4 both fail to produce a
    // canonical release_year because they require a matching release_year fact.
    // The resolver must derive games.release_year from the first four chars of
    // games.release_date after materialization.
    const QString connectionName = QStringLiteral("resolver_year_from_date_test");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(execSql(
            db, QStringLiteral("INSERT INTO games (game_id, canonical_title) VALUES ('game-r2', 'Date Game')")));
        // Only a release_date fact — no release_year fact exists.
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_facts "
                           "  (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-r2', 'release_date', '2001-06-15', 100, 1.0)")));

        MergeResolver resolver;
        CompilerStats stats;
        QString error;
        QVERIFY2(resolver.resolve(db, stats, error), qPrintable(error));

        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT release_date, release_year FROM games WHERE game_id = 'game-r2'"));
        QVERIFY(q.next());
        // release_date must be materialized from the fact.
        QCOMPARE(q.value(0).toString(), QStringLiteral("2001-06-15"));
        // release_year must be derived as the first 4 chars.
        QCOMPARE(q.value(1).toInt(), 2001);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumMergeResolverTest::resolve_playersMax_rejectsNonPureNumeric() {
    // R6: SQLite's CAST coerces '2-4' to 2 and '4 players' to 4, which both
    // pass the BETWEEN 1 AND 16 check.  The resolver must require the field
    // value to be a pure integer string (no leading/trailing characters).
    const QString connectionName = QStringLiteral("resolver_players_max_numeric_test");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(execSql(
            db, QStringLiteral("INSERT INTO games (game_id, canonical_title) VALUES ('game-r6', 'Multi Game')")));
        // Composite and free-text values that should be rejected.
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_facts "
                           "  (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-r6', 'players_max', '2-4', 100, 1.0)")));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_facts "
                           "  (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-r6', 'players_max', '4 players', 90, 1.0)")));
        // One valid pure-integer value.
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_facts "
                           "  (game_id, field_name, field_value, source_priority, confidence) "
                           "VALUES ('game-r6', 'players_max', '2', 80, 1.0)")));

        MergeResolver resolver;
        CompilerStats stats;
        QString error;
        QVERIFY2(resolver.resolve(db, stats, error), qPrintable(error));

        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT players_max FROM games WHERE game_id = 'game-r6'"));
        QVERIFY(q.next());
        // Only '2' is a valid pure-integer string in range; '2-4' and '4 players' must be rejected.
        QCOMPARE(q.value(0).toInt(), 2);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumMergeResolverTest)

#include "test_compendium_merge_resolver.moc"