#include <QtTest>

#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "../src/core/constants/match_methods.h"
#include "../src/core/system_resolver.h"
#include "../src/metadata/compendium_provider.h"
#include "../src/metadata/provider_orchestrator.h"

using namespace Remus;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery query(db);
    return query.exec(sql);
}

bool createSchema(QSqlDatabase &db) {
    return execSql(db,
               QStringLiteral("CREATE TABLE systems (system_id INTEGER PRIMARY KEY, internal_name TEXT NOT NULL "
                              "UNIQUE, display_name TEXT NOT NULL, libretro_name TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE games (game_id TEXT PRIMARY KEY, system_id INTEGER NOT NULL, canonical_title "
                           "TEXT NOT NULL, primary_region_code TEXT, release_date TEXT, release_year INTEGER, "
                           "developer TEXT, publisher TEXT, genre TEXT, players_max INTEGER, description TEXT, rating "
                           "REAL, canonical_confidence REAL NOT NULL DEFAULT 0)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_signatures (signature_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT "
                           "NOT NULL, hash_type TEXT NOT NULL, hash_value TEXT NOT NULL, source_id TEXT NOT NULL "
                           "DEFAULT 'test', snapshot_id TEXT, source_entry_key TEXT, confidence REAL NOT NULL, "
                           "is_primary INTEGER NOT NULL DEFAULT 0)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE game_serials (serial_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT "
                           "NULL, serial_value TEXT NOT NULL, source_id TEXT NOT NULL DEFAULT 'test', snapshot_id "
                           "TEXT, source_entry_key TEXT, confidence REAL NOT NULL)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE source_items (source_item_id INTEGER PRIMARY KEY AUTOINCREMENT, source_id "
                           "TEXT NOT NULL, snapshot_id TEXT NOT NULL DEFAULT '', external_key TEXT NOT NULL, "
                           "system_hint TEXT, title_raw TEXT, region_raw TEXT, payload_json TEXT)"));
}

} // namespace

class CompendiumMultiSignalTest : public QObject {
    Q_OBJECT

private slots:
    void matchROM_filenameSizeFallback();
    void matchROM_serialOnly();
    void orchestratorUsesMultiSignalWhenHashMisses();
};

void CompendiumMultiSignalTest::orchestratorUsesMultiSignalWhenHashMisses() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("orch-multi.db"));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("orch-multi-test"));
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
    QVERIFY(createSchema(db));

    const QString entryKey = QStringLiteral("Nintendo - NES|Super Mario Bros.|Super Mario Bros. (World).nes");
    const QString payload
        = QStringLiteral(R"({"rom_name":"Super Mario Bros. (World).nes","size":40976,"crc32":"222A6A53"})");

    const int nesSystemId = SystemResolver::systemIdByName(QStringLiteral("NES"));
    QVERIFY(nesSystemId > 0);

    QVERIFY(execSql(db, QStringLiteral("INSERT OR REPLACE INTO systems (system_id, internal_name, display_name) "
                                        "VALUES (%1, 'NES', 'Nintendo Entertainment System')")
                        .arg(nesSystemId)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, canonical_confidence) "
                                        "VALUES ('smb', %1, 'Super Mario Bros.', 0.99)")
                        .arg(nesSystemId)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO source_items (source_id, external_key, payload_json) "
                                        "VALUES ('test', '%1', '%2')")
                        .arg(entryKey, payload)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value, source_entry_key, "
                                        "confidence, is_primary) "
                                        "VALUES ('smb', 'crc32', '222A6A53', '%1', 1.0, 1)")
                        .arg(entryKey)));

    auto *provider = new CompendiumProvider();
    QVERIFY(provider->openDatabase(dbPath));

    ProviderOrchestrator orchestrator;
    orchestrator.addProvider(QStringLiteral("compendium"), provider, 210);

    QSignalSpy trySpy(&orchestrator, &ProviderOrchestrator::tryingProvider);

    const GameMetadata result = orchestrator.searchWithFallback(QStringLiteral("BADHASH"),
        QStringLiteral("Super Mario Bros. (World).nes"), QStringLiteral("NES"), QString(), QString(), QString(),
        QString(), 40976);

    QCOMPARE(result.title, QStringLiteral("Super Mario Bros."));
    QVERIFY(!result.matchMethod.isEmpty());

    bool sawMultiSignal = false;
    for (int i = 0; i < trySpy.count(); ++i) {
        if (trySpy.at(i).at(0).toString() == QLatin1String("compendium")
            && trySpy.at(i).at(1).toString() == QLatin1String("multi_signal")) {
            sawMultiSignal = true;
        }
    }
    QVERIFY(sawMultiSignal);
}

void CompendiumMultiSignalTest::matchROM_filenameSizeFallback() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("multi-signal.db"));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("multi-signal-test"));
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
    QVERIFY(createSchema(db));

    const QString entryKey = QStringLiteral("Sega - Mega Drive - Genesis|Sonic The Hedgehog (USA, Europe)|Sonic.md");
    const QString payload = QStringLiteral(
        R"({"rom_name":"Sonic The Hedgehog (USA, Europe).md","size":524288,"crc32":"F9394E97"})");

    const int genesisSystemId = SystemResolver::systemIdByName(QStringLiteral("Genesis"));
    QVERIFY(genesisSystemId > 0);

    QVERIFY(execSql(db, QStringLiteral("INSERT OR REPLACE INTO systems (system_id, internal_name, display_name) "
                                        "VALUES (%1, 'Genesis', 'Sega Genesis')")
                        .arg(genesisSystemId)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, canonical_confidence) "
                                        "VALUES ('sonic-1', %1, 'Sonic The Hedgehog', 0.95)")
                        .arg(genesisSystemId)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO source_items (source_id, external_key, payload_json) "
                                        "VALUES ('test', '%1', '%2')")
                        .arg(entryKey, payload)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value, source_entry_key, "
                                        "confidence, is_primary) "
                                        "VALUES ('sonic-1', 'crc32', 'DEADBEEF', '%1', 1.0, 1)")
                        .arg(entryKey)));

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    ROMSignals input;
    input.filename = QStringLiteral("Sonic The Hedgehog (USA, Europe).md");
    input.fileSize = 524288;

    const QList<CompendiumMultiSignalMatch> matches = provider.matchROM(input, QStringLiteral("Genesis"));
    QVERIFY(!matches.isEmpty());
    QCOMPARE(matches.first().gameId, QStringLiteral("sonic-1"));
    QVERIFY(matches.first().filenameMatch);
    QVERIFY(matches.first().sizeMatch);
    QCOMPARE(matches.first().confidencePercent(), 40);

    const GameMetadata metadata = provider.metadataFromMatch(matches.first(), QStringLiteral("Genesis"));
    QCOMPARE(metadata.title, QStringLiteral("Sonic The Hedgehog (USA, Europe)"));
    QCOMPARE(metadata.matchMethod, QString(Constants::MatchMethods::FUZZY));
}

void CompendiumMultiSignalTest::matchROM_serialOnly() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("serial-only.db"));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("serial-only-test"));
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());
    QVERIFY(createSchema(db));

    const QString entryKey = QStringLiteral("Nintendo - GameCube|Mario Kart: Double Dash!!|Mario Kart - Double Dash!! "
                                            "(USA).iso");

    const int gcSystemId = SystemResolver::systemIdByName(QStringLiteral("GameCube"));
    QVERIFY(gcSystemId > 0);

    QVERIFY(execSql(db, QStringLiteral("INSERT OR REPLACE INTO systems (system_id, internal_name, display_name) "
                                        "VALUES (%1, 'GameCube', 'Nintendo GameCube')")
                        .arg(gcSystemId)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, canonical_confidence) "
                                        "VALUES ('mkdd', %1, 'Mario Kart: Double Dash!!', 0.95)")
                        .arg(gcSystemId)));
    QVERIFY(execSql(db, QStringLiteral("INSERT INTO game_serials (game_id, serial_value, source_entry_key, confidence) "
                                        "VALUES ('mkdd', 'GM4E01', '%1', 1.0)")
                        .arg(entryKey)));

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    ROMSignals input;
    input.serial = QStringLiteral("GM4E01");
    input.fileSize = 1459978240;

    const QList<CompendiumMultiSignalMatch> matches = provider.matchROM(input, QStringLiteral("GameCube"));
    QVERIFY(!matches.isEmpty());
    QVERIFY(matches.first().serialMatch);
    QCOMPARE(matches.first().gameId, QStringLiteral("mkdd"));

    const GameMetadata metadata = provider.metadataFromMatch(matches.first(), QStringLiteral("GameCube"));
    QCOMPARE(metadata.matchMethod, QStringLiteral("serial"));
    QVERIFY(metadata.matchScore >= 0.65f);
}

QTEST_MAIN(CompendiumMultiSignalTest)
#include "test_compendium_multi_signal.moc"
