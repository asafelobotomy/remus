#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "../src/cli/compendium_enrichment_sql.h"

using namespace CompendiumEnrichmentSql;

namespace {

bool createFactsSchema(QSqlDatabase &db)
{
    QSqlQuery q(db);
    return q.exec(QStringLiteral(
        "CREATE TABLE game_facts ("
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

QSqlDatabase openMemoryDb(const QString &connectionName)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    return db;
}

} // namespace

class CompendiumEnrichmentSqlHelpersTest : public QObject
{
    Q_OBJECT

private slots:
    void nullableHelpers_returnNullOrValue();
    void insertGameFact_insertsRowAndReportsInserted();
    void insertGameFact_emptyValueNoop();
    void insertGameFact_duplicateReturnsInsertedFalse();
};

void CompendiumEnrichmentSqlHelpersTest::nullableHelpers_returnNullOrValue()
{
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

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_insertsRowAndReportsInserted()
{
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_insert");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFactsSchema(db));

        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        const FactInsertSpec spec{
            QStringLiteral("test-source"),
            QStringLiteral("snap-1"),
            70,
            0.8,
        };

        QString error;
        bool inserted = false;
        QVERIFY(insertGameFact(factQ,
                               spec,
                               QStringLiteral("g1"),
                               QStringLiteral("genre"),
                               QStringLiteral("Action"),
                               QStringLiteral("text"),
                               error,
                               QStringLiteral("test"),
                               &inserted));
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

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_emptyValueNoop()
{
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_noop");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFactsSchema(db));

        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        const FactInsertSpec spec{
            QStringLiteral("test-source"),
            QStringLiteral("snap-1"),
            70,
            0.8,
        };

        QString error;
        bool inserted = true;
        QVERIFY(insertGameFact(factQ,
                               spec,
                               QStringLiteral("g1"),
                               QStringLiteral("genre"),
                               QString(),
                               QStringLiteral("text"),
                               error,
                               QStringLiteral("test"),
                               &inserted));
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

void CompendiumEnrichmentSqlHelpersTest::insertGameFact_duplicateReturnsInsertedFalse()
{
    const QString connectionName = QStringLiteral("enrichment_sql_helpers_duplicate");
    {
        QSqlDatabase db = openMemoryDb(connectionName);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createFactsSchema(db));

        QSqlQuery factQ(db);
        factQ.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        const FactInsertSpec spec{
            QStringLiteral("test-source"),
            QStringLiteral("snap-1"),
            70,
            0.8,
        };

        QString error;
        bool inserted = false;
        QVERIFY(insertGameFact(factQ,
                               spec,
                               QStringLiteral("g1"),
                               QStringLiteral("genre"),
                               QStringLiteral("Action"),
                               QStringLiteral("text"),
                               error,
                               QStringLiteral("test"),
                               &inserted));
        QVERIFY(inserted);

        inserted = true;
        QVERIFY(insertGameFact(factQ,
                               spec,
                               QStringLiteral("g1"),
                               QStringLiteral("genre"),
                               QStringLiteral("Action"),
                               QStringLiteral("text"),
                               error,
                               QStringLiteral("test"),
                               &inserted));
        QVERIFY(!inserted);

        QSqlQuery countQ(db);
        QVERIFY(countQ.exec(QStringLiteral("SELECT COUNT(*) FROM game_facts")));
        QVERIFY(countQ.next());
        QCOMPARE(countQ.value(0).toInt(), 1);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumEnrichmentSqlHelpersTest)

#include "test_compendium_enrichment_sql_helpers.moc"
