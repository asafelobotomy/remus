// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "../src/cli/compendium_enrichment.h"

namespace {

bool execSql(QSqlDatabase &db, const QString &sql, QString *errOut = nullptr) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        if (errOut)
            *errOut = q.lastError().text();
        return false;
    }
    return true;
}

bool createSchema(QSqlDatabase &db) {
    const QStringList ddl = {
        QStringLiteral("CREATE TABLE games ("
                       "game_id TEXT PRIMARY KEY, "
                       "system_id INTEGER NOT NULL, "
                       "canonical_title TEXT NOT NULL, "
                       "description TEXT, "
                       "genre TEXT, "
                       "developer TEXT, "
                       "publisher TEXT, "
                       "release_year INTEGER, "
                       "rating REAL, "
                       "igdb_id TEXT, "
                       "ra_game_id TEXT, "
                       "achievement_count INTEGER)"),
        QStringLiteral("CREATE TABLE game_signatures ("
                       "sig_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "game_id TEXT NOT NULL, "
                       "hash_type TEXT NOT NULL, "
                       "hash_value TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE game_facts ("
                       "fact_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "game_id TEXT NOT NULL, "
                       "field_name TEXT NOT NULL, "
                       "field_value TEXT NOT NULL, "
                       "value_type TEXT NOT NULL DEFAULT 'text', "
                       "source_id TEXT NOT NULL DEFAULT 'test', "
                       "snapshot_id TEXT NOT NULL DEFAULT '', "
                       "source_priority INTEGER NOT NULL DEFAULT 100, "
                       "confidence REAL NOT NULL DEFAULT 1.0, "
                       "UNIQUE (game_id, field_name, source_id))"),
        QStringLiteral("CREATE TABLE sources ("
                       "source_id TEXT PRIMARY KEY, "
                       "display_name TEXT, "
                       "source_type TEXT, "
                       "license_id TEXT, "
                       "license_url TEXT, "
                       "attribution_required INTEGER NOT NULL DEFAULT 0, "
                       "priority INTEGER NOT NULL DEFAULT 100, "
                       "enabled INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral("CREATE TABLE source_snapshots ("
                       "snapshot_id TEXT PRIMARY KEY, "
                       "source_id TEXT NOT NULL, "
                       "snapshot_label TEXT, "
                       "snapshot_ref TEXT, "
                       "fetched_at TEXT, "
                       "checksum_sha256 TEXT)"),
    };
    for (const QString &stmt : ddl) {
        if (!execSql(db, stmt))
            return false;
    }
    return true;
}

} // namespace

class CompendiumHasheousEnrichmentTest : public QObject {
    Q_OBJECT
private slots:
    void noPendingSignatures_returnsEarlyWithNoWrite();
    void existingIgdbFact_isExcludedFromPendingSet();
    void igdbFactFromOtherSource_isExcludedFromPendingSet();
};

void CompendiumHasheousEnrichmentTest::noPendingSignatures_returnsEarlyWithNoWrite() {
    const QString connName = QStringLiteral("hasheous_test_no_sigs");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QVERIFY(createSchema(db));
    execSql(db,
        QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) "
                       "VALUES ('g1', 1, 'Test Game')"));

    int games = 0;
    int facts = 0;
    QString error;
    const bool ok
        = CompendiumEnrichment::enrichFromHasheous(db, QStringLiteral("/nonexistent/creds.json"), games, facts, error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(games, 0);
    QCOMPARE(facts, 0);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumHasheousEnrichmentTest::existingIgdbFact_isExcludedFromPendingSet() {
    const QString connName = QStringLiteral("hasheous_test_existing_fact");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QVERIFY(createSchema(db));
    execSql(db,
        QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) "
                       "VALUES ('g1', 1, 'Test Game')"));
    execSql(db,
        QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value) "
                       "VALUES ('g1', 'md5', 'aabbccdd001122334455667788990011')"));
    execSql(db,
        QStringLiteral("INSERT INTO game_facts (game_id, field_name, field_value, value_type, source_id, "
                       "snapshot_id, source_priority, confidence) "
                       "VALUES ('g1', 'igdb_id', '123', 'text', 'hasheous', 'snap', 88, 1.0)"));

    int games = 0;
    int facts = 0;
    int apiNeeded = -1;
    int apiPerformed = -1;
    QString error;
    const bool ok
        = CompendiumEnrichment::enrichFromHasheous(db, QString(), games, facts, error, &apiNeeded, &apiPerformed);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(games, 0);
    QCOMPARE(facts, 0);
    QCOMPARE(apiNeeded, 0);
    QCOMPARE(apiPerformed, 0);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumHasheousEnrichmentTest::igdbFactFromOtherSource_isExcludedFromPendingSet() {
    const QString connName = QStringLiteral("hasheous_test_other_source_fact");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QVERIFY(createSchema(db));
    execSql(db,
        QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title) "
                       "VALUES ('g1', 1, 'Test Game')"));
    execSql(db,
        QStringLiteral("INSERT INTO game_signatures (game_id, hash_type, hash_value) "
                       "VALUES ('g1', 'md5', 'aabbccdd001122334455667788990011')"));
    execSql(db,
        QStringLiteral("INSERT INTO game_facts (game_id, field_name, field_value, value_type, source_id, "
                       "snapshot_id, source_priority, confidence) "
                       "VALUES ('g1', 'igdb_id', '456', 'text', 'playmatch', 'snap', 91, 1.0)"));

    int games = 0;
    int facts = 0;
    int apiNeeded = -1;
    QString error;
    const bool ok = CompendiumEnrichment::enrichFromHasheous(db, QString(), games, facts, error, &apiNeeded);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(games, 0);
    QCOMPARE(facts, 0);
    QCOMPARE(apiNeeded, 0);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

QTEST_MAIN(CompendiumHasheousEnrichmentTest)
#include "test_compendium_hasheous_enrichment.moc"
