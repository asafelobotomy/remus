#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "../src/cli/compendium_enrichment_sql.h"

using namespace CompendiumEnrichmentSql;

namespace {

bool createFactsSchema(QSqlDatabase &db) {
    QSqlQuery q(db);
    return q.exec(QStringLiteral("CREATE TABLE game_facts ("
                                 "fact_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                 "game_id TEXT NOT NULL, "
                                 "field_name TEXT NOT NULL, "
                                 "field_value TEXT NOT NULL, "
                                 "value_type TEXT NOT NULL DEFAULT 'text', "
                                 "source_id TEXT NOT NULL, "
                                 "snapshot_id TEXT NOT NULL DEFAULT '', "
                                 "source_priority INTEGER NOT NULL DEFAULT 100, "
                                 "confidence REAL NOT NULL DEFAULT 1.0, "
                                 "UNIQUE (game_id, field_name, source_id))"));
}

QSqlDatabase openMemoryDb(const QString &connectionName) {
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    return db;
}

bool createFkFactsSchema(QSqlDatabase &db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA foreign_keys = ON")))
        return false;
    if (!q.exec(QStringLiteral("CREATE TABLE games (game_id TEXT PRIMARY KEY)")))
        return false;
    if (!q.exec(QStringLiteral("CREATE TABLE game_facts ("
                               "fact_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "game_id TEXT NOT NULL, "
                               "field_name TEXT NOT NULL, "
                               "field_value TEXT NOT NULL, "
                               "value_type TEXT NOT NULL DEFAULT 'text', "
                               "source_id TEXT NOT NULL, "
                               "snapshot_id TEXT NOT NULL DEFAULT '', "
                               "source_priority INTEGER NOT NULL DEFAULT 100, "
                               "confidence REAL NOT NULL DEFAULT 1.0, "
                               "UNIQUE (game_id, field_name, field_value, source_id, snapshot_id), "
                               "FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE)")))
        return false;
    if (!q.exec(QStringLiteral("CREATE TABLE canonical_resolution ("
                               "game_id TEXT NOT NULL, "
                               "field_name TEXT NOT NULL, "
                               "selected_fact_id INTEGER NOT NULL, "
                               "PRIMARY KEY (game_id, field_name), "
                               "FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE, "
                               "FOREIGN KEY (selected_fact_id) REFERENCES game_facts(fact_id))")))
        return false;
    if (!q.exec(QStringLiteral("CREATE TABLE merge_conflicts ("
                               "conflict_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "game_id TEXT NOT NULL, "
                               "field_name TEXT NOT NULL, "
                               "fact_ids_json TEXT NOT NULL, "
                               "resolution_status TEXT NOT NULL DEFAULT 'unresolved', "
                               "chosen_fact_id INTEGER, "
                               "FOREIGN KEY (game_id) REFERENCES games(game_id) ON DELETE CASCADE, "
                               "FOREIGN KEY (chosen_fact_id) REFERENCES game_facts(fact_id))")))
        return false;
    return q.exec(QStringLiteral("INSERT INTO games (game_id) VALUES ('g1')"));
}

} // namespace

class CompendiumEnrichmentSqlHelpersTest : public QObject {
    Q_OBJECT

private slots:
    void nullableHelpers_returnNullOrValue();
    void insertGameFact_insertsRowAndReportsInserted();
    void insertGameFact_emptyValueNoop();
    void insertGameFact_replacesPriorFactFromSameSource();
    void insertGameFact_clearsBlockersWhenCanonicalReferencesDuplicateFacts();
    void normalizeMetadataTitle_stripsStackedSuffixes();
};

void CompendiumEnrichmentSqlHelpersTest::nullableHelpers_returnNullOrValue() {
    const QVariant nullText = nullableText(QString());
    QVERIFY(nullText.isNull());

    const QVariant text = nullableText(QStringLiteral("abc"));
    QCOMPARE(text.toString(), QStringLiteral("abc"));

    const QVariant nullIntValue = nullableInt(0);
    QVERIFY(nullIntValue.isNull());

    const QVariant intValue = nullableInt(42);
    QCOMPARE(intValue.toInt(), 42);

    const QVariant nullDoubleValue = nullableDouble(0.0);
    QVERIFY(nullDoubleValue.isNull());

    const QVariant doubleValue = nullableDouble(3.5);
    QCOMPARE(doubleValue.toDouble(), 3.5);
}

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_insertsRowAndReportsInserted() {
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_insert");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFactsSchema(db));

        FactReplaceQueries replaceQueries(db);

        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        QSqlQuery delQ(db);
        delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec spec {
            QStringLiteral("test-source"),
            QStringLiteral("snap-1"),
            70,
            0.8,
        };

        QString error;
        bool inserted = false;
        QVERIFY(insertGameFact(replaceQueries, delQ, factQ, spec, QStringLiteral("g1"), QStringLiteral("genre"),
            QStringLiteral("Action"), QStringLiteral("text"), error, QStringLiteral("test"), &inserted));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(inserted);

        QSqlQuery countQ(db);
        QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM game_facts")));
        QVERIFY(countQ.next());
        QCOMPARE(countQ.value(0).toInt(), 1);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_emptyValueNoop() {
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_noop");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFactsSchema(db));

        FactReplaceQueries replaceQueries(db);

        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        QSqlQuery delQ(db);
        delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec spec {
            QStringLiteral("test-source"),
            QStringLiteral("snap-1"),
            70,
            0.8,
        };

        QString error;
        bool inserted = true;
        QVERIFY(insertGameFact(replaceQueries, delQ, factQ, spec, QStringLiteral("g1"), QStringLiteral("genre"),
            QString(), QStringLiteral("text"), error, QStringLiteral("test"), &inserted));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!inserted);

        QSqlQuery countQ(db);
        QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM game_facts")));
        QVERIFY(countQ.next());
        QCOMPARE(countQ.value(0).toInt(), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_replacesPriorFactFromSameSource() {
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_replace");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFactsSchema(db));

        FactReplaceQueries replaceQueries(db);

        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        QSqlQuery delQ(db);
        delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec spec {
            QStringLiteral("test-source"),
            QStringLiteral("snap-1"),
            70,
            0.8,
        };

        QString error;
        bool inserted = false;
        // First insert: 'Action'
        QVERIFY(insertGameFact(replaceQueries, delQ, factQ, spec, QStringLiteral("g1"), QStringLiteral("genre"),
            QStringLiteral("Action"), QStringLiteral("text"), error, QStringLiteral("test"), &inserted));
        QVERIFY(inserted);

        inserted = false;
        // Second insert with a different value: 'RPG' should replace 'Action'.
        QVERIFY(insertGameFact(replaceQueries, delQ, factQ, spec, QStringLiteral("g1"), QStringLiteral("genre"),
            QStringLiteral("RPG"), QStringLiteral("text"), error, QStringLiteral("test"), &inserted));
        QVERIFY(inserted);

        // Only one row should exist and it must hold the newer value.
        QSqlQuery checkQ(db);
        QVERIFY(checkQ.exec(QStringLiteral("SELECT COUNT(*), field_value FROM game_facts")));
        QVERIFY(checkQ.next());
        QCOMPARE(checkQ.value(0).toInt(), 1);
        QCOMPARE(checkQ.value(1).toString(), QStringLiteral("RPG"));

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_clearsBlockersWhenCanonicalReferencesDuplicateFacts() {
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_fk_blockers");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFkFactsSchema(db));

        QSqlQuery seedQ(db);
        QVERIFY(seedQ.exec(QStringLiteral("INSERT INTO game_facts "
                                          "(game_id, field_name, field_value, value_type, source_id, snapshot_id) "
                                          "VALUES ('g1', 'genre', 'Action', 'text', 'test-source', 'snap-a')")));
        const int firstFactId = seedQ.lastInsertId().toInt();
        QVERIFY(seedQ.exec(QStringLiteral("INSERT INTO game_facts "
                                          "(game_id, field_name, field_value, value_type, source_id, snapshot_id) "
                                          "VALUES ('g1', 'genre', 'Adventure', 'text', 'test-source', 'snap-b')")));
        const int secondFactId = seedQ.lastInsertId().toInt();
        QVERIFY(firstFactId > 0);
        QVERIFY(secondFactId > 0);
        QVERIFY(secondFactId != firstFactId);

        seedQ.prepare(QStringLiteral("INSERT INTO canonical_resolution (game_id, field_name, selected_fact_id) "
                                     "VALUES ('g1', 'genre', ?)"));
        seedQ.addBindValue(secondFactId);
        QVERIFY(seedQ.exec());

        seedQ.prepare(QStringLiteral("INSERT INTO merge_conflicts "
                                     "(game_id, field_name, fact_ids_json, chosen_fact_id) "
                                     "VALUES ('g1', 'genre', '[]', ?)"));
        seedQ.addBindValue(secondFactId);
        QVERIFY(seedQ.exec());

        FactReplaceQueries replaceQueries(db);
        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        QSqlQuery delQ(db);
        delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec spec {
            QStringLiteral("test-source"),
            QStringLiteral("snap-c"),
            70,
            0.8,
        };

        QString error;
        bool inserted = false;
        QVERIFY(insertGameFact(replaceQueries, delQ, factQ, spec, QStringLiteral("g1"), QStringLiteral("genre"),
            QStringLiteral("RPG"), QStringLiteral("text"), error, QStringLiteral("test"), &inserted));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(inserted);

        QSqlQuery countQ(db);
        QVERIFY(
            countQ.exec(QStringLiteral("SELECT COUNT(*), field_value FROM game_facts "
                                       "WHERE game_id = 'g1' AND field_name = 'genre' AND source_id = 'test-source'")));
        QVERIFY(countQ.next());
        QCOMPARE(countQ.value(0).toInt(), 1);
        QCOMPARE(countQ.value(1).toString(), QStringLiteral("RPG"));

        QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM canonical_resolution "
                                           "WHERE game_id = 'g1' AND field_name = 'genre'")));
        QVERIFY(countQ.next());
        QCOMPARE(countQ.value(0).toInt(), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentSqlHelpersTest::normalizeMetadataTitle_stripsStackedSuffixes() {
    // A title with no suffixes must normalize identically to the same title
    // with any number of stacked trailing parenthetical groups.
    const QString bare = normalizeMetadataTitle(QStringLiteral("Foo Game"));
    QCOMPARE(normalizeMetadataTitle(QStringLiteral("Foo Game (Europe)")), bare);
    QCOMPARE(normalizeMetadataTitle(QStringLiteral("Foo Game (Europe) (En,Fr,De)")), bare);
    QCOMPARE(normalizeMetadataTitle(QStringLiteral("Foo Game (Europe) (En,Fr,De) (Rev 1)")), bare);
    QCOMPARE(normalizeMetadataTitle(QStringLiteral("Foo Game (USA) (Beta 3) (Unl)")), bare);

    // Leading article is still stripped after suffix removal.
    const QString bareThe = normalizeMetadataTitle(QStringLiteral("Legend of Foo"));
    QCOMPARE(normalizeMetadataTitle(QStringLiteral("The Legend of Foo (Europe) (En,De)")), bareThe);

    // A title whose only content is inside parentheses must not be reduced to empty.
    const QString onlyParens = normalizeMetadataTitle(QStringLiteral("(Test)"));
    QVERIFY(!onlyParens.isEmpty());
}

QTEST_MAIN(CompendiumEnrichmentSqlHelpersTest)

#include "test_compendium_enrichment_sql_helpers.moc"
