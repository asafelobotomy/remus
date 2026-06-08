#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "../src/metadata/compendium_provider.h"
#include "../src/core/constants/providers.h"

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
            QStringLiteral("CREATE TABLE game_names (game_name_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT "
                           "NULL, name_text TEXT NOT NULL, alias_type TEXT NOT NULL, locale TEXT NOT NULL DEFAULT '', "
                           "snapshot_id TEXT NOT NULL DEFAULT '', confidence REAL NOT NULL DEFAULT 0)"))
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
            QStringLiteral(
                "CREATE TABLE game_facts (fact_id INTEGER PRIMARY KEY AUTOINCREMENT, game_id TEXT NOT NULL, field_name "
                "TEXT NOT NULL, field_value TEXT NOT NULL, value_type TEXT NOT NULL DEFAULT 'text', source_id TEXT NOT "
                "NULL DEFAULT 'test', snapshot_id TEXT NOT NULL DEFAULT '', source_item_id INTEGER, source_priority "
                "INTEGER NOT NULL DEFAULT 100, confidence REAL NOT NULL DEFAULT 1.0)"))
        && execSql(db,
            QStringLiteral(
                "CREATE TABLE canonical_resolution (game_id TEXT NOT NULL, field_name TEXT NOT NULL, selected_fact_id "
                "INTEGER NOT NULL, resolved_by_rule TEXT NOT NULL, PRIMARY KEY (game_id, field_name))"));
}

int insertFact(QSqlDatabase &db, const QString &gameId, const QString &fieldName, const QString &fieldValue) {
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO game_facts (game_id, field_name, field_value, value_type, source_id, source_priority, confidence) "
        "VALUES (?, ?, ?, 'text', 'test', 100, 1.0)"));
    insert.addBindValue(gameId);
    insert.addBindValue(fieldName);
    insert.addBindValue(fieldValue);
    if (!insert.exec()) {
        return 0;
    }
    return insert.lastInsertId().toInt();
}

bool resolveFact(QSqlDatabase &db, const QString &gameId, const QString &fieldName, int factId) {
    QSqlQuery insert(db);
    insert.prepare(
        QStringLiteral("INSERT INTO canonical_resolution (game_id, field_name, selected_fact_id, resolved_by_rule) "
                       "VALUES (?, ?, ?, 'test_rule')"));
    insert.addBindValue(gameId);
    insert.addBindValue(fieldName);
    insert.addBindValue(factId);
    return insert.exec();
}

bool seedCompendium(QSqlDatabase &db) {
    if (!execSql(db,
            QStringLiteral("INSERT INTO systems (system_id, internal_name, display_name, libretro_name) VALUES (4, "
                           "'GameCube', 'Nintendo GameCube', 'Nintendo - Nintendo GameCube')"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, primary_region_code, release_year, "
                           "canonical_confidence) VALUES ('game-1', 4, 'Paper Mario: The Thousand-Year Door', 'USA', "
                           "2004, 0.95)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, primary_region_code, release_year, "
                           "canonical_confidence) VALUES ('game-2', 4, 'Paper Mario', 'USA', 2001, 0.93)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral("INSERT INTO game_names (game_id, name_text, alias_type, confidence) VALUES ('game-1', "
                           "'TTYD', 'alt_name', 0.95)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral("INSERT INTO game_names (game_id, name_text, alias_type, confidence) VALUES ('game-1', "
                           "'Paper Mario: The Thousand-Year Door', 'official', 1.0)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral("INSERT INTO game_names (game_id, name_text, alias_type, confidence) VALUES ('game-2', "
                           "'Paper Mario', 'official', 1.0)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value, confidence, is_primary) "
                           "VALUES ('game-1', 'md5', '0123456789abcdef0123456789abcdef', 1.0, 1)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral(
                "INSERT INTO game_serials (game_id, serial_value, confidence) VALUES ('game-1', 'G8ME01', 1.0)"))) {
        return false;
    }
    if (!execSql(db,
            QStringLiteral(
                "INSERT INTO game_serials (game_id, serial_value, confidence) VALUES ('game-1', 'G8ME01-2', 1.0)"))) {
        return false;
    }

    const int titleFact = insertFact(
        db, QStringLiteral("game-1"), QStringLiteral("title"), QStringLiteral("Paper Mario: The Thousand-Year Door"));
    const int publisherFact
        = insertFact(db, QStringLiteral("game-1"), QStringLiteral("publisher"), QStringLiteral("Nintendo"));
    const int developerFact
        = insertFact(db, QStringLiteral("game-1"), QStringLiteral("developer"), QStringLiteral("Intelligent Systems"));
    const int releaseYearFact
        = insertFact(db, QStringLiteral("game-1"), QStringLiteral("release_year"), QStringLiteral("2004"));
    const int playersFact
        = insertFact(db, QStringLiteral("game-1"), QStringLiteral("players_max"), QStringLiteral("1"));
    const int descriptionFact = insertFact(db, QStringLiteral("game-1"), QStringLiteral("description"),
        QStringLiteral("A turn-based adventure across the Mushroom Kingdom."));
    const int genreFact
        = insertFact(db, QStringLiteral("game-1"), QStringLiteral("genre"), QStringLiteral("Role-Playing"));
    const int ratingFact = insertFact(db, QStringLiteral("game-1"), QStringLiteral("rating"), QStringLiteral("9.0"));

    return titleFact > 0 && publisherFact > 0 && developerFact > 0 && releaseYearFact > 0 && playersFact > 0
        && descriptionFact > 0 && genreFact > 0 && ratingFact > 0
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("title"), titleFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("publisher"), publisherFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("developer"), developerFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("release_year"), releaseYearFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("players_max"), playersFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("description"), descriptionFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("genre"), genreFact)
        && resolveFact(db, QStringLiteral("game-1"), QStringLiteral("rating"), ratingFact);
}

QString createFixtureDatabase() {
    QTemporaryDir dir;
    dir.setAutoRemove(false);
    const QString dbPath = dir.filePath(QStringLiteral("remus_compendium_test.db"));

    const QString connectionName
        = QStringLiteral("compendium_fixture_%1").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            return { };
        }

        if (!createSchema(db) || !seedCompendium(db)) {
            db.close();
            QSqlDatabase::removeDatabase(connectionName);
            return { };
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
    return dbPath;
}

} // namespace

class CompendiumProviderTest : public QObject {
    Q_OBJECT

private slots:
    void getByHashReturnsCanonicalMetadata();
    void searchByNameUsesAliasAndFilters();
    void searchByNameWithOfficialTitleReturnsSameGame();
    void searchByNameKeepsBestMatchFirst();
    void getBySerialReturnsCanonicalMetadata();
    void getByIdReturnsExternalIds();
    void getByIdSetsMatchFields();
    void openDatabaseFailureReturnsFalse();
    void getByHashReturnsAllSerials();
    void getArtworkBuildsLibretroThumbnailUrls();
    void compendiumPriorityExceedsLocalDatabase();
    void searchByName_shortQuery_usesLikeFallback();
    void searchByName_returnsNoDuplicateGameIds();
};

void CompendiumProviderTest::getByHashReturnsCanonicalMetadata() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());
    QVERIFY(QFileInfo::exists(dbPath));

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const GameMetadata metadata
        = provider.getByHash(QStringLiteral("0123456789ABCDEF0123456789ABCDEF"), QStringLiteral("GameCube"));

    QCOMPARE(metadata.id, QStringLiteral("game-1"));
    QCOMPARE(metadata.title, QStringLiteral("Paper Mario: The Thousand-Year Door"));
    QCOMPARE(metadata.system, QStringLiteral("GameCube"));
    QCOMPARE(metadata.region, QStringLiteral("USA"));
    QCOMPARE(metadata.publisher, QStringLiteral("Nintendo"));
    QCOMPARE(metadata.developer, QStringLiteral("Intelligent Systems"));
    QCOMPARE(metadata.releaseDate, QStringLiteral("2004"));
    QCOMPARE(metadata.players, 1);
    QCOMPARE(metadata.genres, QStringList { QStringLiteral("Role-Playing") });
    QCOMPARE(metadata.rating, 9.0f);
    QCOMPARE(metadata.providerId, QStringLiteral("compendium"));
    QCOMPARE(metadata.matchScore, 1.0f);
    QCOMPARE(metadata.matchMethod, QStringLiteral("hash"));
}

void CompendiumProviderTest::searchByNameUsesAliasAndFilters() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const QList<SearchResult> results
        = provider.searchByName(QStringLiteral("TTYD"), QStringLiteral("GameCube"), QStringLiteral("USA"));

    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, QStringLiteral("game-1"));
    QCOMPARE(results.first().title, QStringLiteral("Paper Mario: The Thousand-Year Door"));
    QCOMPARE(results.first().system, QStringLiteral("GameCube"));
    QCOMPARE(results.first().region, QStringLiteral("USA"));
    QCOMPARE(results.first().releaseYear, 2004);

    const QList<SearchResult> wrongRegion
        = provider.searchByName(QStringLiteral("TTYD"), QStringLiteral("GameCube"), QStringLiteral("JPN"));
    QVERIFY(wrongRegion.isEmpty());
}

void CompendiumProviderTest::getByIdReturnsExternalIds() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const GameMetadata metadata = provider.getById(QStringLiteral("game-1"));
    QCOMPARE(metadata.id, QStringLiteral("game-1"));
    QCOMPARE(metadata.externalIds.value(QStringLiteral("md5")), QStringLiteral("0123456789abcdef0123456789abcdef"));
    QVERIFY(metadata.serials.contains(QStringLiteral("G8ME01")));
}

void CompendiumProviderTest::searchByNameWithOfficialTitleReturnsSameGame() {
    // Searching by official title should resolve the same game as an alias search.
    // Verifies that both alias types in game_names map to a single result.
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const QList<SearchResult> byAlias
        = provider.searchByName(QStringLiteral("TTYD"), QStringLiteral("GameCube"), QStringLiteral("USA"));

    const QList<SearchResult> byTitle = provider.searchByName(
        QStringLiteral("Paper Mario: The Thousand-Year Door"), QStringLiteral("GameCube"), QStringLiteral("USA"));

    QCOMPARE(byTitle.size(), 1);
    QCOMPARE(byTitle.first().id, QStringLiteral("game-1"));

    // Both search paths must yield the same canonical game id.
    QCOMPARE(byTitle.first().id, byAlias.first().id);
}

void CompendiumProviderTest::searchByNameKeepsBestMatchFirst() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const QList<SearchResult> results
        = provider.searchByName(QStringLiteral("Paper Mario"), QStringLiteral("GameCube"), QStringLiteral("USA"));

    QVERIFY(results.size() >= 2);
    QCOMPARE(results.first().id, QStringLiteral("game-2"));
    QCOMPARE(results.first().title, QStringLiteral("Paper Mario"));

    bool foundLongTitle = false;
    for (const SearchResult &result : results) {
        if (result.id == QStringLiteral("game-1")) {
            foundLongTitle = true;
            break;
        }
    }
    QVERIFY(foundLongTitle);
}

void CompendiumProviderTest::getBySerialReturnsCanonicalMetadata() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const GameMetadata metadata = provider.getBySerial(QStringLiteral(" G8ME01 "), QStringLiteral("GameCube"));
    QCOMPARE(metadata.id, QStringLiteral("game-1"));
    QCOMPARE(metadata.title, QStringLiteral("Paper Mario: The Thousand-Year Door"));
    QCOMPARE(metadata.system, QStringLiteral("GameCube"));
    QCOMPARE(metadata.matchMethod, QStringLiteral("serial"));
    QCOMPARE(metadata.matchScore, 0.9f);
}

void CompendiumProviderTest::getArtworkBuildsLibretroThumbnailUrls() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const ArtworkUrls artwork = provider.getArtwork(QStringLiteral("game-1"));
    QVERIFY(artwork.boxFront.isValid());
    QVERIFY(artwork.screenshot.isValid());
    QVERIFY(artwork.titleScreen.isValid());

    QCOMPARE(artwork.boxFront.toString(),
        QStringLiteral("https://thumbnails.libretro.com/Nintendo - Nintendo GameCube/Named_Boxarts/Paper Mario_ The "
                       "Thousand-Year Door.png"));
    QCOMPARE(artwork.screenshot.toString(),
        QStringLiteral("https://thumbnails.libretro.com/Nintendo - Nintendo GameCube/Named_Snaps/Paper Mario_ The "
                       "Thousand-Year Door.png"));
    QCOMPARE(artwork.titleScreen.toString(),
        QStringLiteral("https://thumbnails.libretro.com/Nintendo - Nintendo GameCube/Named_Titles/Paper Mario_ The "
                       "Thousand-Year Door.png"));
}

void CompendiumProviderTest::getByIdSetsMatchFields() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const GameMetadata metadata = provider.getById(QStringLiteral("game-1"));
    QCOMPARE(metadata.matchScore, 1.0f);
    QCOMPARE(metadata.matchMethod, QStringLiteral("id"));
}

void CompendiumProviderTest::openDatabaseFailureReturnsFalse() {
    CompendiumProvider provider;
    QVERIFY(!provider.openDatabase(QStringLiteral("/nonexistent/path/remus_test.db")));
    QVERIFY(!provider.isAvailable());
}

void CompendiumProviderTest::getByHashReturnsAllSerials() {
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const GameMetadata metadata
        = provider.getByHash(QStringLiteral("0123456789ABCDEF0123456789ABCDEF"), QStringLiteral("GameCube"));

    QCOMPARE(metadata.id, QStringLiteral("game-1"));
    QCOMPARE(metadata.serials.size(), 2);
    QVERIFY(metadata.serials.contains(QStringLiteral("G8ME01")));
    QVERIFY(metadata.serials.contains(QStringLiteral("G8ME01-2")));
}

void CompendiumProviderTest::compendiumPriorityExceedsLocalDatabase() {
    // Compendium is the highest-priority offline provider; local DAT database
    // is second. Verify the constants enforce this ordering.
    QVERIFY(Constants::Providers::Priority::COMPENDIUM > Constants::Providers::Priority::LOCAL_DATABASE);
}

void CompendiumProviderTest::searchByName_shortQuery_usesLikeFallback() {
    // Queries shorter than 3 characters must still return results via the LIKE
    // fallback (trigram FTS is skipped since it requires >= 3 characters).
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    // "Pa" is a 2-character query — both Paper Mario games should match.
    const QList<SearchResult> results
        = provider.searchByName(QStringLiteral("Pa"), QStringLiteral("GameCube"), QStringLiteral(""));

    QVERIFY2(!results.isEmpty(), "Short query must return results via LIKE fallback");
    // Verify both seeded games are found.
    QStringList ids;
    for (const SearchResult &r : results)
        ids.append(r.id);
    QVERIFY(ids.contains(QStringLiteral("game-1")));
    QVERIFY(ids.contains(QStringLiteral("game-2")));
}

void CompendiumProviderTest::searchByName_returnsNoDuplicateGameIds() {
    // A game with multiple aliases must appear at most once in results.
    // (game-1 has both "TTYD" and "Paper Mario: The Thousand-Year Door" in game_names.)
    const QString dbPath = createFixtureDatabase();
    QVERIFY(!dbPath.isEmpty());

    CompendiumProvider provider;
    QVERIFY(provider.openDatabase(dbPath));

    const QList<SearchResult> results
        = provider.searchByName(QStringLiteral("Paper Mario"), QStringLiteral("GameCube"), QStringLiteral(""));

    QSet<QString> seen;
    for (const SearchResult &r : results) {
        QVERIFY2(!seen.contains(r.id), qPrintable(QStringLiteral("Duplicate game_id in results: %1").arg(r.id)));
        seen.insert(r.id);
    }
}

QTEST_MAIN(CompendiumProviderTest)
#include "test_compendium_provider.moc"