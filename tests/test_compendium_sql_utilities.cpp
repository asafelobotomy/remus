#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "../src/cli/compendium_sql_utilities.h"

class CompendiumSqlUtilitiesTest : public QObject {
    Q_OBJECT

private slots:
    void executeSqlScript_ignoresSemicolonInsideLineComments();
    void discSetMigration_appliesOnUpgradedBootstrap();
    void materializedCoverageMigration_createsSnapshotTables();
};

static QString repoRootPath() {
#ifdef REMUS_SOURCE_DIR
    return QStringLiteral(REMUS_SOURCE_DIR);
#else
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("data/compendium/migrations/0007_disc_sets.sql")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
#endif
}

static bool tableExists(QSqlDatabase &database, const QString &tableName) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?"));
    query.addBindValue(tableName);
    return query.exec() && query.next() && query.value(0).toInt() == 1;
}

void CompendiumSqlUtilitiesTest::discSetMigration_appliesOnUpgradedBootstrap() {
    const QString rootPath = repoRootPath();
    QVERIFY2(!rootPath.isEmpty(), "Could not locate repository root for compendium migrations");

    const QString compendiumDir = rootPath + QStringLiteral("/data/compendium");
    const QStringList bootstrapScripts = {
        compendiumDir + QStringLiteral("/migrations/0001_phase1_canonical_schema.sql"),
        compendiumDir + QStringLiteral("/migrations/0002_patch_catalog.sql"),
        compendiumDir + QStringLiteral("/seeds/0001_regions.sql"),
        compendiumDir + QStringLiteral("/seeds/0002_systems.sql"),
        compendiumDir + QStringLiteral("/seeds/0003_merge_policy.sql"),
        compendiumDir + QStringLiteral("/migrations/0003_systems_libretro_name.sql"),
        compendiumDir + QStringLiteral("/migrations/0004_fts5_search_index.sql"),
        compendiumDir + QStringLiteral("/migrations/0005_game_external_ids.sql"),
        compendiumDir + QStringLiteral("/migrations/0006_game_achievement_count.sql"),
    };
    const QString discSetMigration = compendiumDir + QStringLiteral("/migrations/0007_disc_sets.sql");
    const QString factsIndexMigration = compendiumDir + QStringLiteral("/migrations/0008_game_facts_lookup_index.sql");
    const QString sigEntryMigration
        = compendiumDir + QStringLiteral("/migrations/0009_game_signatures_source_entry_key.sql");

    for (const QString &scriptPath : bootstrapScripts) {
        QVERIFY2(QFileInfo::exists(scriptPath), qPrintable(QStringLiteral("Missing migration: %1").arg(scriptPath)));
    }
    QVERIFY(QFileInfo::exists(discSetMigration));
    QVERIFY(QFileInfo::exists(factsIndexMigration));
    QVERIFY(QFileInfo::exists(sigEntryMigration));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.path() + QStringLiteral("/compendium.db");
    const QString connectionName = QStringLiteral("compendium_disc_set_migration_test");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbPath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QString error;
        for (const QString &scriptPath : bootstrapScripts) {
            QVERIFY2(CompendiumSqlUtilities::executeSqlScript(database, scriptPath, error), qPrintable(error));
        }
        QVERIFY(!tableExists(database, QStringLiteral("game_disc_sets")));
        QVERIFY(!tableExists(database, QStringLiteral("game_disc_tracks")));

        QVERIFY2(CompendiumSqlUtilities::executeSqlScript(database, discSetMigration, error), qPrintable(error));
        QVERIFY(tableExists(database, QStringLiteral("game_disc_sets")));
        QVERIFY(tableExists(database, QStringLiteral("game_disc_tracks")));

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumSqlUtilitiesTest::materializedCoverageMigration_createsSnapshotTables() {
    const QString rootPath = repoRootPath();
    QVERIFY2(!rootPath.isEmpty(), "Could not locate repository root for compendium migrations");

    const QString compendiumDir = rootPath + QStringLiteral("/data/compendium");
    const QString coverageMigration = compendiumDir + QStringLiteral("/migrations/0011_materialized_coverage.sql");
    QVERIFY(QFileInfo::exists(coverageMigration));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.path() + QStringLiteral("/compendium.db");
    const QString connectionName = QStringLiteral("compendium_coverage_migration_test");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbPath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QString error;
        QVERIFY2(CompendiumSqlUtilities::executeSqlScript(database, coverageMigration, error), qPrintable(error));
        QVERIFY(tableExists(database, QStringLiteral("compendium_coverage_stats")));
        QVERIFY(tableExists(database, QStringLiteral("compendium_source_coverage")));

        database.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumSqlUtilitiesTest::executeSqlScript_ignoresSemicolonInsideLineComments() {
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
