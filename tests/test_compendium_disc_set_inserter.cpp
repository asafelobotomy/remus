#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "../src/cli/compendium_sql_utilities.h"
#include "../src/core/disc_set_key.h"
#include "../src/metadata/compendium_dat_extractor.h"
#include "../src/metadata/compendium_fact_inserter.h"
#include "../src/metadata/compendium_identity_linker.h"
#include "../src/metadata/compendium_normalizer.h"

using namespace Remus;
using namespace Remus::Compendium;

class CompendiumDiscSetInserterTest : public QObject {
    Q_OBJECT

private:
    static QString repoRoot() {
#ifdef REMUS_SOURCE_DIR
        return QStringLiteral(REMUS_SOURCE_DIR);
#else
        QDir dir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 8; ++i) {
            if (QFileInfo::exists(dir.filePath(QStringLiteral("data/compendium/migrations/0007_disc_sets.sql"))))
                return dir.absolutePath();
            if (!dir.cdUp())
                break;
        }
        return QString();
#endif
    }

    static bool execSql(QSqlDatabase &db, const QString &sql) {
        QSqlQuery query(db);
        return query.exec(sql);
    }

    static bool seedMinimalCompendium(QSqlDatabase &db, QString &error) {
        const QString root = repoRoot();
        if (root.isEmpty()) {
            error = QStringLiteral("repository root not found");
            return false;
        }

        const QString migration0001
            = root + QStringLiteral("/data/compendium/migrations/0001_phase1_canonical_schema.sql");
        const QString migration0007 = root + QStringLiteral("/data/compendium/migrations/0007_disc_sets.sql");
        if (!CompendiumSqlUtilities::executeSqlScript(db, migration0001, error))
            return false;
        if (!CompendiumSqlUtilities::executeSqlScript(db, migration0007, error))
            return false;

        return execSql(db,
                   QStringLiteral("INSERT INTO sources (source_id, display_name, source_type, priority) "
                                  "VALUES ('redump', 'Redump', 'dat', 10)"))
            && execSql(db,
                QStringLiteral("INSERT INTO source_snapshots (snapshot_id, source_id, snapshot_label) "
                               "VALUES ('snap-ff7', 'redump', 'test'), ('snap-chd', 'redump', 'test-chd')"))
            && execSql(db,
                QStringLiteral("INSERT INTO systems (system_id, internal_name, display_name, "
                               "preferred_hash, is_disc_based) "
                               "VALUES (14, 'PlayStation', 'Sony PlayStation', 'MD5', 1)"))
            && execSql(db,
                QStringLiteral("INSERT INTO regions (region_code, display_name, group_code) "
                               "VALUES ('USA', 'USA', 'USA')"));
    }

private slots:
    void ff7MultiDisc_createsSharedSetKeyAndTracks();
    void chdDisk_setsPrimaryContentSha1();
    void shenmueVariants_sameSetKeyDifferentVariant();
    void residentEvilSplitPath_differentSetKeys();
};

void CompendiumDiscSetInserterTest::ff7MultiDisc_createsSharedSetKeyAndTracks() {
    const QString datContent
        = QStringLiteral("clrmamepro (\n"
                         "    name \"Sony - PlayStation\"\n"
                         ")\n"
                         "game (\n"
                         "    name \"Final Fantasy VII (USA) (Disc 1)\"\n"
                         "    rom ( name \"Final Fantasy VII (USA) (Disc 1).bin\" size 100 crc AAAAAAAA )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Final Fantasy VII (USA) (Disc 2)\"\n"
                         "    rom ( name \"Final Fantasy VII (USA) (Disc 2).bin\" size 100 crc BBBBBBBB )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Final Fantasy VII (USA) (Disc 3)\"\n"
                         "    rom ( name \"Final Fantasy VII (USA) (Disc 3).bin\" size 100 crc CCCCCCCC )\n"
                         ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 3);
    QCOMPARE(records[0].parsedDiscNumber, 1);
    QCOMPARE(records[1].parsedDiscNumber, 2);
    QCOMPARE(records[2].parsedDiscNumber, 3);
    QCOMPARE(records[0].parsedDiscCount, 3);

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);
    QCOMPARE(records[0].resolvedSystemId, 14);
    QCOMPARE(records[0].resolvedRegionCode, QStringLiteral("USA"));

    IdentityLinker linker;
    const int created = linker.link(records);
    QCOMPARE(created, 1);
    const QString gameId = records[0].linkedGameId;
    QVERIFY(!gameId.isEmpty());
    QCOMPARE(records[1].linkedGameId, gameId);
    QCOMPARE(records[2].linkedGameId, gameId);
    QVERIFY(!records[0].datGameBlockName.isEmpty());

    const QString expectedSetKey
        = DiscSetKey::compute(14, QStringLiteral("Final Fantasy VII (USA) (Disc 1)"), QStringLiteral("USA"));
    QCOMPARE(DiscSetKey::compute(14, records[1].datGameBlockName, QStringLiteral("USA")), expectedSetKey);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_inserter_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));
    QCOMPARE(stats.discSetsCreated, 3);
    QCOMPARE(stats.tracksCreated, 3);

    QSqlQuery query(db);
    QVERIFY(query.exec(
        QStringLiteral("SELECT COUNT(*) FROM game_disc_sets WHERE game_id = '") + gameId + QLatin1Char('\'')));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    QVERIFY(query.exec(
        QStringLiteral("SELECT DISTINCT set_key FROM game_disc_sets WHERE game_id = '") + gameId + QLatin1Char('\'')));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), expectedSetKey);
    QVERIFY(!query.next());

    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM game_disc_tracks")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumDiscSetInserterTest::chdDisk_setsPrimaryContentSha1() {
    const QString datContent = QStringLiteral("clrmamepro (\n"
                                              "    name \"Sony - PlayStation\"\n"
                                              ")\n"
                                              "game (\n"
                                              "    name \"Metal Gear Solid (USA) (Disc 1)\"\n"
                                              "    rom ( name \"Metal Gear Solid (USA) (Disc 1).chd\" size 100 "
                                              "sha1 abcdef0123456789abcdef0123456789abcdef01 )\n"
                                              ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-chd"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].primaryContentSha1, QStringLiteral("abcdef0123456789abcdef0123456789abcdef01"));

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);

    IdentityLinker linker;
    linker.link(records);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_chd_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));

    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("SELECT primary_content_sha1 FROM game_disc_sets LIMIT 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("abcdef0123456789abcdef0123456789abcdef01"));

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumDiscSetInserterTest::shenmueVariants_sameSetKeyDifferentVariant() {
    const QString datContent
        = QStringLiteral("clrmamepro (\n"
                         "    name \"Sony - PlayStation\"\n"
                         ")\n"
                         "game (\n"
                         "    name \"Shenmue (USA) (Disc 3) [!][1S]\"\n"
                         "    rom ( name \"Shenmue (USA) (Disc 3) [!][1S].bin\" size 100 crc AAAAAAAA )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Shenmue (USA) (Disc 3) [!][2S]\"\n"
                         "    rom ( name \"Shenmue (USA) (Disc 3) [!][2S].bin\" size 100 crc BBBBBBBB )\n"
                         ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 2);
    QVERIFY(records[0].parsedSetVariant.contains(QStringLiteral("1S")));
    QVERIFY(records[1].parsedSetVariant.contains(QStringLiteral("2S")));

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);

    IdentityLinker linker;
    linker.link(records);

    const QString setKeyOne = DiscSetKey::compute(14, records[0].datGameBlockName, QStringLiteral("USA"));
    const QString setKeyTwo = DiscSetKey::compute(14, records[1].datGameBlockName, QStringLiteral("USA"));
    QCOMPARE(setKeyOne, setKeyTwo);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_variant_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));
    QCOMPARE(stats.discSetsCreated, 2);

    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("SELECT set_key, set_variant FROM game_disc_sets ORDER BY set_variant")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), setKeyOne);
    QVERIFY(query.value(1).toString().contains(QStringLiteral("1S")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), setKeyTwo);
    QVERIFY(query.value(1).toString().contains(QStringLiteral("2S")));
    QVERIFY(!query.next());

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumDiscSetInserterTest::residentEvilSplitPath_differentSetKeys() {
    const QString datContent
        = QStringLiteral("clrmamepro (\n"
                         "    name \"Sony - PlayStation\"\n"
                         ")\n"
                         "game (\n"
                         "    name \"Resident Evil 2 (USA) (Disc 1) (Leon)\"\n"
                         "    rom ( name \"Resident Evil 2 (USA) (Disc 1) (Leon).bin\" size 100 crc AAAAAAAA )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Resident Evil 2 (USA) (Disc 2) (Claire)\"\n"
                         "    rom ( name \"Resident Evil 2 (USA) (Disc 2) (Claire).bin\" size 100 crc BBBBBBBB )\n"
                         ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 2);

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);

    IdentityLinker linker;
    linker.link(records);

    const QString leonKey = DiscSetKey::compute(14, records[0].datGameBlockName, QStringLiteral("USA"));
    const QString claireKey = DiscSetKey::compute(14, records[1].datGameBlockName, QStringLiteral("USA"));
    QVERIFY(leonKey != claireKey);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_re2_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));
    QCOMPARE(stats.discSetsCreated, 2);

    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(DISTINCT set_key) FROM game_disc_sets")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumDiscSetInserterTest)
#include "test_compendium_disc_set_inserter.moc"
