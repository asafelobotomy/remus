#include <QtTest/QtTest>

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "../src/cli/compendium_sql_utilities.h"

class CompendiumSqlUtilitiesTest : public QObject
{
    Q_OBJECT

private slots:
    void executeSqlScript_ignoresSemicolonInsideLineComments();
};

void CompendiumSqlUtilitiesTest::executeSqlScript_ignoresSemicolonInsideLineComments()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.path() + QStringLiteral("/compendium.db");
    const QString scriptPath = tempDir.path() + QStringLiteral("/migration.sql");

    QFile scriptFile(scriptPath);
    QVERIFY(scriptFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptFile.write(R"SQL(
        CREATE TABLE test_entries (id INTEGER PRIMARY KEY, name TEXT);
        -- this comment includes a semicolon; parser must not split here
        INSERT INTO test_entries (name) VALUES ('alpha');
        INSERT INTO test_entries (name) VALUES ('beta;inside-string');
    )SQL");
    scriptFile.close();

    const QString connectionName = QStringLiteral("compendium_sql_utilities_test_conn");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbPath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QString error;
        QVERIFY2(CompendiumSqlUtilities::executeSqlScript(database, scriptPath, error), qPrintable(error));

        QSqlQuery countQuery(database);
        QVERIFY(countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM test_entries")));
        QVERIFY(countQuery.next());
        QCOMPARE(countQuery.value(0).toInt(), 2);

        QSqlQuery valueQuery(database);
        QVERIFY(valueQuery.exec(QStringLiteral("SELECT name FROM test_entries WHERE id = 2")));
        QVERIFY(valueQuery.next());
        QCOMPARE(valueQuery.value(0).toString(), QStringLiteral("beta;inside-string"));

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumSqlUtilitiesTest)

#include "test_compendium_sql_utilities.moc"
