#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "../src/cli/compendium_sql_utilities.h"
#include "../src/metadata/compendium_dat_extractor.h"
#include "../src/metadata/compendium_disc_set_backfill.h"
#include "../src/metadata/compendium_fact_inserter.h"
#include "../src/metadata/compendium_identity_linker.h"
#include "../src/metadata/compendium_normalizer.h"

using namespace Remus;
using namespace Remus::Compendium;

class DiscSetValidationTest : public QObject {
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
                               "VALUES ('snap-ff7', 'redump', 'test')"))
            && execSql(db,
                QStringLiteral("INSERT INTO systems (system_id, internal_name, display_name, "
                               "preferred_hash, is_disc_based) "
                               "VALUES (14, 'PlayStation', 'Sony PlayStation', 'MD5', 1)"))
            && execSql(db,
                QStringLiteral("INSERT INTO regions (region_code, display_name, group_code) "
                               "VALUES ('USA', 'USA', 'USA')"));
    }

    static int countValidationStatus(QSqlDatabase &db, const QString &sqlPath, const QString &status) {
        QFile file(sqlPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return -1;

        QString sql = QString::fromUtf8(file.readAll());
        const int marker = sql.indexOf(QStringLiteral("-- Diagnostic details"));
        if (marker >= 0)
            sql = sql.left(marker);

        int count = 0;
        for (const QString &statement : CompendiumSqlUtilities::splitSqlStatements(sql)) {
            if (statement.trimmed().isEmpty())
                continue;

            QSqlQuery query(db);
            if (!query.exec(statement))
                return -1;

            while (query.next()) {
                if (query.value(QStringLiteral("status")).toString() == status)
                    ++count;
            }
        }
        return count;
    }

    static bool ingestFf7Fixture(QSqlDatabase &db, QString &gameId, QString &error) {
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
        if (!datFile.open()) {
            error = QStringLiteral("failed to create temporary DAT file");
            return false;
        }
        datFile.write(datContent.toUtf8());
        datFile.close();

        QList<SourceRecordEnvelope> records
            = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), error);
        if (records.isEmpty())
            return false;

        CompendiumNormalizer normalizer;
        for (SourceRecordEnvelope &rec : records)
            normalizer.normalize(rec);

        IdentityLinker linker;
        linker.link(records);
        gameId = records[0].linkedGameId;

        CompilerStats stats;
        const FactInserter inserter;
        return inserter.insert(records, db, stats, error);
    }

private slots:
    void populatedDb_passesSchemaAndIngestChecks();
    void badDiscCount_failsSchemaValidation();
    void backfill_restoresTopologyAfterClear();
};

void DiscSetValidationTest::populatedDb_passesSchemaAndIngestChecks() {
    const QString root = repoRoot();
    QVERIFY2(!root.isEmpty(), "repository root not found");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("disc_set_validation_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    QString gameId;
    QString ingestError;
    QVERIFY2(ingestFf7Fixture(db, gameId, ingestError), qPrintable(ingestError));
    QVERIFY(!gameId.isEmpty());

    const QString schemaChecks = root + QStringLiteral("/data/compendium/validation/0004_disc_set_checks.sql");
    const QString ingestChecks = root + QStringLiteral("/data/compendium/validation/0005_disc_set_ingest_checks.sql");

    QCOMPARE(countValidationStatus(db, schemaChecks, QStringLiteral("FAIL")), 0);
    QCOMPARE(countValidationStatus(db, ingestChecks, QStringLiteral("FAIL")), 0);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void DiscSetValidationTest::badDiscCount_failsSchemaValidation() {
    const QString root = repoRoot();
    QVERIFY2(!root.isEmpty(), "repository root not found");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("disc_set_validation_bad_count_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    QString gameId;
    QString ingestError;
    QVERIFY2(ingestFf7Fixture(db, gameId, ingestError), qPrintable(ingestError));

    QVERIFY(execSql(db, QStringLiteral("UPDATE game_disc_sets SET disc_count = 1 WHERE disc_number = 3")));

    const QString schemaChecks = root + QStringLiteral("/data/compendium/validation/0004_disc_set_checks.sql");
    QVERIFY(countValidationStatus(db, schemaChecks, QStringLiteral("FAIL")) >= 1);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void DiscSetValidationTest::backfill_restoresTopologyAfterClear() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("disc_set_backfill_test");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(tempDir.path() + QStringLiteral("/compendium.db"));
    QVERIFY(db.open());

    QString seedError;
    QVERIFY2(seedMinimalCompendium(db, seedError), qPrintable(seedError));

    QString gameId;
    QString ingestError;
    QVERIFY2(ingestFf7Fixture(db, gameId, ingestError), qPrintable(ingestError));

    QVERIFY(execSql(db, QStringLiteral("DELETE FROM game_disc_tracks")));
    QVERIFY(execSql(db, QStringLiteral("DELETE FROM game_disc_sets")));

    CompilerStats stats;
    QString backfillError;
    QVERIFY2(DiscSetBackfill::backfillDiscSets(db, false, stats, backfillError), qPrintable(backfillError));
    QCOMPARE(stats.discSetsCreated, 3);
    QCOMPARE(stats.tracksCreated, 3);

    QSqlQuery query(db);
    QVERIFY(query.exec(
        QStringLiteral("SELECT COUNT(*) FROM game_disc_sets WHERE game_id = '") + gameId + QLatin1Char('\'')));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(DiscSetValidationTest)
#include "test_disc_set_validation.moc"
