#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTextStream>

#include "../src/cli/compendium_enrichment.h"
#include "../src/cli/cli_compendium_build_phases.h"
#include "../src/core/constants/system_ids.h"

using namespace Remus;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery q(db);
    return q.exec(sql);
}

bool createSchema(QSqlDatabase &db) {
    return execSql(db,
               QStringLiteral("CREATE TABLE games ("
                              "game_id TEXT PRIMARY KEY, "
                              "system_id INTEGER NOT NULL, "
                              "canonical_title TEXT NOT NULL, "
                              "description TEXT, "
                              "genre TEXT, "
                              "developer TEXT, "
                              "publisher TEXT, "
                              "release_year INTEGER, "
                              "release_date TEXT, "
                              "players_max INTEGER, "
                              "age_rating TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE systems ("
                           "system_id INTEGER PRIMARY KEY, "
                           "display_name TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_signatures ("
                           "sig_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "game_id TEXT NOT NULL, "
                           "hash_type TEXT NOT NULL, "
                           "hash_value TEXT NOT NULL, "
                           "source_entry_key TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE source_items ("
                           "item_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "source_id TEXT NOT NULL, "
                           "external_key TEXT NOT NULL UNIQUE, "
                           "payload_json TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_serials ("
                           "serial_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "game_id TEXT NOT NULL, "
                           "serial_value TEXT NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_names ("
                           "game_name_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                           "game_id TEXT NOT NULL, "
                           "name_text TEXT NOT NULL, "
                           "alias_type TEXT NOT NULL, "
                           "locale TEXT NOT NULL DEFAULT '', "
                           "source_id TEXT, "
                           "snapshot_id TEXT NOT NULL DEFAULT '', "
                           "confidence REAL NOT NULL DEFAULT 0)"))
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
                           "confidence REAL NOT NULL DEFAULT 1.0, "
                           "UNIQUE (game_id, field_name, source_id))"))
        && execSql(db,
            QStringLiteral("CREATE TABLE canonical_resolution ("
                           "game_id TEXT NOT NULL, "
                           "field_name TEXT NOT NULL, "
                           "selected_fact_id INTEGER NOT NULL, "
                           "resolved_by_rule TEXT NOT NULL, "
                           "resolved_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
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
                           "resolved_at TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE sources ("
                           "source_id TEXT PRIMARY KEY, "
                           "display_name TEXT, "
                           "source_type TEXT, "
                           "license_id TEXT, "
                           "license_url TEXT, "
                           "attribution_required INTEGER NOT NULL DEFAULT 0, "
                           "priority INTEGER NOT NULL DEFAULT 100, "
                           "enabled INTEGER NOT NULL DEFAULT 1)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE source_snapshots ("
                           "snapshot_id TEXT PRIMARY KEY, "
                           "source_id TEXT NOT NULL, "
                           "snapshot_label TEXT, "
                           "snapshot_ref TEXT, "
                           "fetched_at TEXT, "
                           "checksum_sha256 TEXT)"));
}

bool createLaunchBoxSchema(QSqlDatabase &db) {
    return createSchema(db);
}

int scalarInt(QSqlDatabase &db, const QString &sql) {
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return -1;
    return q.value(0).toInt();
}

QString fixturePath(const QString &name) {
    const QStringList candidates = {
        QDir::currentPath() + "/tests/fixtures/" + name,
        QDir::currentPath() + "/../tests/fixtures/" + name,
        QCoreApplication::applicationDirPath() + "/../../tests/fixtures/" + name,
        QCoreApplication::applicationDirPath() + "/../../../tests/fixtures/" + name,
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path))
            return QDir::cleanPath(path);
    }
    return { };
}

} // namespace

class CompendiumEnrichmentLocalSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void libretroMissingDir_skipsWithoutWrites();
    void gametdbMissingDir_skipsWithoutWrites();
    void openvgdbMissingFile_skipsWithoutWrites();
    void mameCatverParsedWithoutArcadeRows_upsertsSourceOnly();
    void mameListXmlParsedWithoutArcadeRows_upsertsSourceOnly();
    void igdbMissingCredentials_skipsWithoutWrites();
    void launchBoxMissingFile_skipsWithoutWrites();
    void launchBoxFixture_enrichesLinkedGame();
    void launchBoxProductionFixture_enrichesByTitle();
    void launchBoxSkipAfterFourSourceFacts();
    void gametdbEmptyStringGenre_fillsGenre();
    void theGamesDbMissingBudget_skipsWithoutWrites();
    void gametdbTitleStripMatches_enrichesGame();
    void knownSourceKeys_noBlankNoDuplicates();
    void knownSourceKeys_containsAllExpectedEnrichers();
};

void CompendiumEnrichmentLocalSmokeTest::libretroMissingDir_skipsWithoutWrites() {
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
            db, QStringLiteral("/nonexistent/remus-libretro-metadata-dir"), games, facts, error);
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

void CompendiumEnrichmentLocalSmokeTest::gametdbMissingDir_skipsWithoutWrites() {
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
            db, QStringLiteral("/nonexistent/remus-gametdb-dir"), games, facts, error);
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

void CompendiumEnrichmentLocalSmokeTest::openvgdbMissingFile_skipsWithoutWrites() {
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
            db, QStringLiteral("/nonexistent/remus-openvgdb.sqlite"), QString(), games, facts, error);
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

void CompendiumEnrichmentLocalSmokeTest::mameCatverParsedWithoutArcadeRows_upsertsSourceOnly() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString catverPath = tempDir.filePath(QStringLiteral("catver.ini"));

    QFile catverFile(catverPath);
    QVERIFY(catverFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&catverFile);
    out << "[Category]\n";
    out << "sf2=Fighting\n";
    out.flush();
    catverFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_mame");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // Non-arcade game row: should not be touched by the MAME pass.
        QVERIFY(execSql(
            db, QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) VALUES ('g1', 1, 'sf2')")));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromMameCatver(db, catverPath, games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = 'mame-catver'")), 1);
        QCOMPARE(
            scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM source_snapshots WHERE source_id = 'mame-catver'")), 1);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::mameListXmlParsedWithoutArcadeRows_upsertsSourceOnly() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString listxmlPath = tempDir.filePath(QStringLiteral("listxml.xml"));

    // Write a minimal but valid MAME listxml with one runnable machine.
    QFile xmlFile(listxmlPath);
    QVERIFY(xmlFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray payload
        = QByteArray("<?xml version=\"1.0\"?>\n"
                     "<mame build=\"0.258\">\n"
                     "<machine name=\"sf2\" isdevice=\"no\" runnable=\"yes\">\n"
                     "<description>Street Fighter II: The World Warrior (World 910522)</description>\n"
                     "<year>1991</year>\n"
                     "<manufacturer>Capcom</manufacturer>\n"
                     "<input players=\"2\" coins=\"2\"/>\n"
                     "</machine>\n"
                     "</mame>\n");
    QCOMPARE(xmlFile.write(payload), payload.size());
    xmlFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_mame_listxml");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // Non-arcade game row (system_id = 1): the listxml enricher only touches
        // system_id = ID_ARCADE (39), so this row must remain untouched.
        QVERIFY(execSql(
            db, QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) VALUES ('g1', 1, 'sf2')")));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromMameListXml(db, listxmlPath, games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(db.commit());

        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = 'mame-listxml'")), 1);
        QCOMPARE(
            scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM source_snapshots WHERE source_id = 'mame-listxml'")), 1);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::igdbMissingCredentials_skipsWithoutWrites() {
    const QString connectionName = QStringLiteral("compendium_local_smoke_igdb");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(db.transaction());
        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromIGDB(
            db, QStringLiteral("/nonexistent/igdb-credentials.json"), games, facts, error);
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

void CompendiumEnrichmentLocalSmokeTest::launchBoxMissingFile_skipsWithoutWrites() {
    const QString connectionName = QStringLiteral("compendium_local_smoke_launchbox");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromLaunchBox(
            db, QStringLiteral("/nonexistent/Metadata.xml"), games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::launchBoxFixture_enrichesLinkedGame() {
    const QString xmlPath = fixturePath(QStringLiteral("launchbox_metadata_sample.xml"));
    QVERIFY2(!xmlPath.isEmpty(), "LaunchBox fixture not found");

    const QString connectionName = QStringLiteral("compendium_local_smoke_launchbox_fixture");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createLaunchBoxSchema(db));

        const int systemId = Constants::Systems::ID_ZX_SPECTRUM;
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO systems (system_id, display_name) VALUES (%1, 'ZX Spectrum')").arg(systemId)));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) "
                           "VALUES ('g-jsw', %1, 'Jet Set Willy')")
                .arg(systemId)));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO source_items (source_id, external_key, payload_json) "
                           "VALUES ('test-dat', 'rom-jsw', "
                           "'{\"rom_name\":\"Jet Set Willy (1984)(Software Projects).tzx\"}')")));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_signatures "
                           "(game_id, hash_type, hash_value, source_entry_key) "
                           "VALUES ('g-jsw', 'crc32', 'deadbeef', 'rom-jsw')")));

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromLaunchBox(db, xmlPath, games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(games, 1);
        QVERIFY(facts > 0);

        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT developer, genre, release_year FROM games WHERE game_id = 'g-jsw'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Software Projects"));
        QCOMPARE(q.value(1).toString(), QStringLiteral("Platform"));
        QCOMPARE(q.value(2).toInt(), 1984);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::launchBoxProductionFixture_enrichesByTitle() {
    const QString xmlPath = fixturePath(QStringLiteral("launchbox_metadata_production_sample.xml"));
    QVERIFY2(!xmlPath.isEmpty(), "LaunchBox production fixture not found");

    const QString connectionName = QStringLiteral("compendium_local_smoke_launchbox_production");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createLaunchBoxSchema(db));

        const int systemId = Constants::Systems::ID_N64;
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO systems (system_id, display_name) VALUES (%1, 'Nintendo 64')").arg(systemId)));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) "
                           "VALUES ('g-mario', %1, 'Super Mario 64 (USA)')")
                .arg(systemId)));

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromLaunchBox(db, xmlPath, games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(games, 1);
        QVERIFY(facts > 0);

        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT genre, developer FROM games WHERE game_id = 'g-mario'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Platform"));
        QCOMPARE(q.value(1).toString(), QStringLiteral("Nintendo EAD"));

        QVERIFY(q.exec(QStringLiteral("SELECT field_value FROM game_facts "
                                      "WHERE game_id = 'g-mario' AND field_name = 'launchbox_id'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("12345"));

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::launchBoxSkipAfterFourSourceFacts() {
    const QString xmlPath = fixturePath(QStringLiteral("launchbox_metadata_production_sample.xml"));
    QVERIFY2(!xmlPath.isEmpty(), "LaunchBox production fixture not found");

    const QString connectionName = QStringLiteral("compendium_local_smoke_launchbox_skip");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createLaunchBoxSchema(db));

        const int systemId = Constants::Systems::ID_N64;
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO systems (system_id, display_name) VALUES (%1, 'Nintendo 64')").arg(systemId)));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, genre) "
                           "VALUES ('g-mario', %1, 'Super Mario 64 (USA)', '')")
                .arg(systemId)));
        for (const QString &field : { QStringLiteral("genre"), QStringLiteral("developer"), QStringLiteral("publisher"),
                 QStringLiteral("description") }) {
            QVERIFY(execSql(db,
                QStringLiteral("INSERT INTO game_facts "
                               "(game_id, field_name, field_value, value_type, source_id) "
                               "VALUES ('g-mario', '%1', 'x', 'text', 'launchbox')")
                    .arg(field)));
        }

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromLaunchBox(db, xmlPath, games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);
        QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources")), 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::gametdbEmptyStringGenre_fillsGenre() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString xmlPath = tempDir.filePath(QStringLiteral("wiitdb.xml"));
    QFile xmlFile(xmlPath);
    QVERIFY(xmlFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&xmlFile);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<datafile>\n"
        << "<game name=\"Bar Game (USA)\">\n"
        << "  <id>BGME</id>\n"
        << "  <type>WiiU</type>\n"
        << "  <region>NTSC-U</region>\n"
        << "  <locale lang=\"EN\"><title>Bar Game</title></locale>\n"
        << "  <genre>puzzle</genre>\n"
        << "</game>\n"
        << "</datafile>\n";
    out.flush();
    xmlFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_gametdb_empty_genre");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, genre) "
                           "VALUES ('g1', 56, 'Bar Game', '')")));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value) "
                           "VALUES ('g1', 'crc32', '00000001')")));

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromGameTDB(db, tempDir.path(), games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(games >= 1);

        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT genre FROM games WHERE game_id = 'g1'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("puzzle"));

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumEnrichmentLocalSmokeTest::theGamesDbMissingBudget_skipsWithoutWrites() {
    const QString connectionName = QStringLiteral("compendium_local_smoke_thegamesdb");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromTheGamesDB(
            db, QStringLiteral("/nonexistent/thegamesdb-credentials.json"), games, facts, error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(games, 0);
        QCOMPARE(facts, 0);

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

// Verify that the GameTDB enricher's title fallback strips trailing parenthetical
// groups so that a DAT title like "Foo Game (Europe) (En,Fr) (Rev 1)" matches
// the GameTDB entry whose stored title is just "Foo Game".
void CompendiumEnrichmentLocalSmokeTest::gametdbTitleStripMatches_enrichesGame() {
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
    out.flush();
    xmlFile.close();

    const QString connectionName = QStringLiteral("compendium_local_smoke_gametdb_strip");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QVERIFY(createSchema(db));

        // A game whose title has trailing parenthetical groups that prevent a
        // direct title lookup — only the strip fallback can match "Foo Game".
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) "
                           "VALUES ('g1', 56, 'Foo Game (Europe) (En,Fr) (Rev 1)')")));
        QVERIFY(execSql(db,
            QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value) "
                           "VALUES ('g1', 'crc32', '00000000')")));

        int games = 0;
        int facts = 0;
        QString error;
        const bool ok = CompendiumEnrichment::enrichFromGameTDB(db, tempDir.path(), games, facts, error);
        QVERIFY2(ok, qPrintable(error));

        QVERIFY2(games >= 1, qPrintable(QStringLiteral("Expected at least 1 enriched game, got %1").arg(games)));

        QSqlQuery q(db);
        QVERIFY(
            q.exec(QStringLiteral("SELECT developer, publisher, release_year, genre FROM games WHERE game_id = 'g1'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Foo Dev"));
        QCOMPARE(q.value(1).toString(), QStringLiteral("Foo Pub"));
        QCOMPARE(q.value(2).toInt(), 2020);
        QCOMPARE(q.value(3).toString(), QStringLiteral("action"));

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

// ── Source-key registry tests ────────────────────────────────────────────────
// These tests guard against drift between the knownEnrichmentSourceKeys() list
// and the EnrichmentPassSpec array.  If a new enricher is added to the pipeline
// without also adding its source key to the list, the "containsAllExpected"
// test fails immediately.

void CompendiumEnrichmentLocalSmokeTest::knownSourceKeys_noBlankNoDuplicates() {
    const QStringList keys = knownEnrichmentSourceKeys();

    QVERIFY2(!keys.isEmpty(), "knownEnrichmentSourceKeys() returned an empty list");

    // No blank entries.
    for (const QString &k : keys)
        QVERIFY2(!k.trimmed().isEmpty(), qPrintable("Blank source key found in knownEnrichmentSourceKeys()"));

    // No duplicates.
    const QSet<QString> unique(keys.cbegin(), keys.cend());
    QCOMPARE(unique.size(), keys.size());
}

void CompendiumEnrichmentLocalSmokeTest::knownSourceKeys_containsAllExpectedEnrichers() {
    // This list is the contract: one entry per enrichment pass in
    // cli_compendium_build_phases.cpp.  Add a new entry here whenever a new
    // enricher is added to the pipeline and the corresponding sourceKey is
    // registered in knownEnrichmentSourceKeys().
    const QStringList expected = {
        QStringLiteral("libretro"),
        QStringLiteral("gametdb"),
        QStringLiteral("openvgdb"),
        QStringLiteral("launchbox"),
        QStringLiteral("wikidata"),
        QStringLiteral("thegamesdb"),
        QStringLiteral("screenscraper"),
        QStringLiteral("igdb"),
        QStringLiteral("ra"),
        QStringLiteral("hasheous"),
        QStringLiteral("playmatch"),
        QStringLiteral("mame-catver"),
        QStringLiteral("mame-listxml"),
        QStringLiteral("zxinfo"),
        QStringLiteral("remus-thumbnails"),
    };

    const QStringList actual = knownEnrichmentSourceKeys();

    QCOMPARE(actual.size(), expected.size());
    for (const QString &k : expected)
        QVERIFY2(actual.contains(k), qPrintable(QStringLiteral("Missing expected key: ") + k));
}

QTEST_MAIN(CompendiumEnrichmentLocalSmokeTest)

#include "test_compendium_enrichment_local_smoke.moc"
