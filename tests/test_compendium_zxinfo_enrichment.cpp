// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

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
                      "genre TEXT, "
                      "publisher TEXT, "
                      "release_year INTEGER)"))
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
                      "checksum_sha256 TEXT)"))
        && execSql(db, QStringLiteral(
                      "CREATE TABLE game_facts ("
                      "fact_id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "game_id TEXT NOT NULL, "
                      "field_name TEXT NOT NULL, "
                      "field_value TEXT NOT NULL, "
                      "value_type TEXT NOT NULL DEFAULT 'text', "
                      "source_id TEXT NOT NULL, "
                      "snapshot_id TEXT NOT NULL, "
                      "source_priority INTEGER NOT NULL DEFAULT 100, "
                      "confidence REAL NOT NULL DEFAULT 1.0, "
                      "UNIQUE (game_id, field_name, source_id))"));
}

bool seedGame(QSqlDatabase &db,
              const QString &gameId,
              int systemId,
              const QString &title,
              const QVariant &genre,
              const QVariant &publisher,
              const QVariant &releaseYear)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO games (game_id, system_id, canonical_title, genre, publisher, release_year) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    q.addBindValue(gameId);
    q.addBindValue(systemId);
    q.addBindValue(title);
    q.addBindValue(genre);
    q.addBindValue(publisher);
    q.addBindValue(releaseYear);
    return q.exec();
}

int scalarInt(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return -1;
    return q.value(0).toInt();
}

} // namespace

class CompendiumZxInfoEnrichmentTest : public QObject
{
    Q_OBJECT

private slots:
    void noZxSpectrumRows_returnsWithoutWrites();
    void zxRowsAlreadyComplete_returnsWithoutWrites();
};

void CompendiumZxInfoEnrichmentTest::noZxSpectrumRows_returnsWithoutWrites()
{
    const QString connName = QStringLiteral("zxinfo_test_no_rows");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QVERIFY(createSchema(db));

    // Non-ZX game only; ZXInfo pass should exit before network/provider work.
    QVERIFY(seedGame(db,
                     QStringLiteral("g1"),
                     Remus::Constants::Systems::ID_NES,
                     QStringLiteral("Some NES Game"),
                     QVariant(),
                     QVariant(),
                     QVariant(QMetaType(QMetaType::Int))));

    int games = 0;
    int facts = 0;
    QString error;
    const bool ok = CompendiumEnrichment::enrichFromZXInfo(db, games, facts, error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(games, 0);
    QCOMPARE(facts, 0);
    QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources")), 0);
    QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM source_snapshots")), 0);
    QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void CompendiumZxInfoEnrichmentTest::zxRowsAlreadyComplete_returnsWithoutWrites()
{
    const QString connName = QStringLiteral("zxinfo_test_complete_rows");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(db.open());
    QVERIFY(createSchema(db));

    // ZX row exists but all enrichable fields are already filled.
    QVERIFY(seedGame(db,
                     QStringLiteral("zx1"),
                     Remus::Constants::Systems::ID_ZX_SPECTRUM,
                     QStringLiteral("Jet Set Willy"),
                     QVariant(QStringLiteral("Platform")),
                     QVariant(QStringLiteral("Software Projects")),
                     QVariant(1984)));

    int games = 0;
    int facts = 0;
    QString error;
    const bool ok = CompendiumEnrichment::enrichFromZXInfo(db, games, facts, error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(games, 0);
    QCOMPARE(facts, 0);
    QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM sources")), 0);
    QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM source_snapshots")), 0);
    QCOMPARE(scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM game_facts")), 0);

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

QTEST_MAIN(CompendiumZxInfoEnrichmentTest)
#include "test_compendium_zxinfo_enrichment.moc"
