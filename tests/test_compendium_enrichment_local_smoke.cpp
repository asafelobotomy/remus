#include <QtTest/QtTest>

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

#include "../src/cli/compendium_enrichment.h"
#include "../src/core/constants/system_ids.h"

namespace {

bool execSql(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    return q.exec(sql);
}

bool createSchema(QSqlDatabase &db)
{
    return execSql(db, QStringLiteral(
               "CREATE TABLE games ("
               "game_id TEXT PRIMARY KEY, "
               "system_id INTEGER NOT NULL, "
               "canonical_title TEXT NOT NULL, "
               "description TEXT, "
               "genre TEXT, "
               "developer TEXT, "
               "publisher TEXT, "
               "release_year INTEGER, "
               "players_max INTEGER)"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE game_signatures ("
               "sig_id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "game_id TEXT NOT NULL, "
               "hash_type TEXT NOT NULL, "
               "hash_value TEXT NOT NULL)"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE game_serials ("
               "serial_id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "game_id TEXT NOT NULL, "
               "serial_value TEXT NOT NULL)"))
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
               "confidence REAL NOT NULL DEFAULT 1.0, "
               "UNIQUE (game_id, field_name, source_id))"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE sources ("
               "source_id TEXT PRIMARY KEY, "
               "display_name TEXT, "
               "source_type TEXT, "
               "license_id TEXT, "
               "license_url TEXT, "
               "attribution_required INTEGER NOT NULL DEFAULT 0, "
               "priority INTEGER NOT NULL DEFAULT 100, "
               "enabled INTEGER NOT NULL DEFAULT 1)"))
        && execSql(db, QStringLiteral(
               "CREATE TABLE source_snapshots ("
               "snapshot_id TEXT PRIMARY KEY, "
               "source_id TEXT NOT NULL, "
               "snapshot_label TEXT, "
               "snapshot_ref TEXT, "
               "fetched_at TEXT, "
               "checksum_sha256 TEXT)"));
}

int scalarInt(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return -1;
    return q.value(0).toInt();
}

} // namespace

class CompendiumEnrichmentLocalSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void libretroMissingDir_skipsWithoutWrites();
    void gametdbMissingDir_skipsWithoutWrites();
    void openvgdbMissingFile_skipsWithoutWrites();
    void mameCatverParsedWithoutArcadeRows_upsertsSourceOnly();
};

void CompendiumEnrichmentLocalSmokeTest::libretroMissingDir_skipsWithoutWrites()
{
    const QString connectionName = QStringLiteral("compendium_local_smoke_libretro");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromLibretroMetadata(
            db,
            QStringLiteral("/nonexistent/remus-libretro-metadata-dir"),
            games,
            facts,
            error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources")), 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::gametdbMissingDir_skipsWithoutWrites()
{
    const QString connectionName = QStringLiteral("compendium_local_smoke_gametdb");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromGameTDB(
            db,
            QStringLiteral("/nonexistent/remus-gametdb-dir"),
            games,
            facts,
            error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources")), 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::openvgdbMissingFile_skipsWithoutWrites()
{
    const QString connectionName = QStringLiteral("compendium_local_smoke_openvgdb");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromOpenVGDB(
            db,
            QStringLiteral("/nonexistent/remus-openvgdb.sqlite"),
            games,
            facts,
            error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources")), 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::mameCatverParsedWithoutArcadeRows_upsertsSourceOnly()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString catverPath = tempDir.filePath(QStringLiteral("catver.ini"));

    QFile catverFile(catverPath);
    QVERIFY(catverFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&catverFile);
    out << "[Category]\n";
    out << "sf2=Fighting\n";
    catverFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_mame");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // Non-arcade game row: should not be touched by the MAME pass.
        QVERIFY(execSql(db, QStringLiteral(
            "INSERT INTO games (game_id, system_id, canonical_title) VALUES ('g1', 1, 'sf2')")));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromMameCatver(
            db,
            catverPath,
            games,
            facts,
            error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = 'mame-catver'")), 1);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM source_snapshots WHERE source_id = 'mame-catver'")), 1);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumEnrichmentLocalSmokeTest)

#include "test_compendium_enrichment_local_smoke.moc"
