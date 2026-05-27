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
               "release_date TEXT, "
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
    void mameListXmlParsedWithoutArcadeRows_upsertsSourceOnly();
    void gametdbTitleStripMatches_enrichesGame();
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

void CompendiumEnrichmentLocalSmokeTest::mameListXmlParsedWithoutArcadeRows_upsertsSourceOnly()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString listxmlPath = tempDir.filePath(QStringLiteral("listxml.xml"));

    // Write a minimal but valid MAME listxml with one runnable machine.
    QFile xmlFile(listxmlPath);
    QVERIFY(xmlFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&xmlFile);
    out << "<?xml version=\"1.0\"?>\n"
        << "<mame build=\"0.258\">\n"
        << "<machine name=\"sf2\" isdevice=\"no\" runnable=\"yes\">\n"
        << "<description>Street Fighter II: The World Warrior (World 910522)</description>\n"
        << "<year>1991</year>\n"
        << "<manufacturer>Capcom</manufacturer>\n"
        << "<input players=\"2\" coins=\"2\"/>\n"
        << "</machine>\n"
        << "</mame>\n";
    xmlFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_mame_listxml");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // Non-arcade game row (system_id = 1): the listxml enricher only touches
        // system_id = ID_ARCADE (39), so this row must remain untouched.
        QVERIFY(execSql(db, QStringLiteral(
            "INSERT INTO games (game_id, system_id, canonical_title) VALUES ('g1', 1, 'sf2')")));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromMameListXml(
            db,
            listxmlPath,
            games,
            facts,
            error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = 'mame-listxml'")), 1);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM source_snapshots WHERE source_id = 'mame-listxml'")), 1);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

// Verify that the GameTDB enricher's title fallback strips trailing parenthetical
// groups so that a DAT title like "Foo Game (Europe) (En,Fr) (Rev 1)" matches
// the GameTDB entry whose stored title is just "Foo Game".
void CompendiumEnrichmentLocalSmokeTest::gametdbTitleStripMatches_enrichesGame()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Write a minimal GameTDB XML.  The filename must match a known prefix so
    // GameTDBProvider::loadDatabases() assigns a platform type.
    const QString xmlPath = tempDir.filePath(QStringLiteral("wiitdb.xml"));
    QFile xmlFile(xmlPath);
    QVERIFY(xmlFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&xmlFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<datafile>\n"
        << "<game name=\"Foo Game (USA) (EN)\">\n"
        << "  <id>FGME</id>\n"
        << "  <type>WiiU</type>\n"
        << "  <region>NTSC-U</region>\n"
        << "  <locale lang=\"EN\"><title>Foo Game</title></locale>\n"
        << "  <developer>Foo Dev</developer>\n"
        << "  <publisher>Foo Pub</publisher>\n"
        << "  <date year=\"2020\" month=\"6\" day=\"1\"/>\n"
        << "  <genre>action</genre>\n"
        << "</game>\n"
        << "</datafile>\n";
    xmlFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_gametdb_strip");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // A game whose title has trailing parenthetical groups that prevent a
        // direct title lookup — only the strip fallback can match "Foo Game".
        QVERIFY(execSql(db, QStringLiteral(
            "INSERT INTO games (game_id, system_id, canonical_title) "
            "VALUES ('g1', 56, 'Foo Game (Europe) (En,Fr) (Rev 1)')")));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromGameTDB(
            db, tempDir.path(), games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QVERIFY2(games >= 1,
                 qPrintable(QStringLiteral("Expected at least 1 enriched game, got %1").arg(games)));

        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT developer, publisher, release_year, genre FROM games WHERE game_id = 'g1'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Foo Dev"));
        QCOMPARE(q.value(1).toString(), QStringLiteral("Foo Pub"));
        QCOMPARE(q.value(2).toInt(), 2020);
        QCOMPARE(q.value(3).toString(), QStringLiteral("action"));

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumEnrichmentLocalSmokeTest)

#include "test_compendium_enrichment_local_smoke.moc"
