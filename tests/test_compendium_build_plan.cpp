#include <QtTest/QtTest>

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "../src/cli/cli_compendium_build_phases.h"
#include "../src/core/compendium_manifest_parser.h"

using namespace Remus;

namespace {

bool execSql(QSqlDatabase &db, const QString &sql) {
    QSqlQuery q(db);
    return q.exec(sql);
}

QString notesWithFingerprint(const QString &fingerprint) {
    const QJsonObject notes {
        { QStringLiteral("enrichment_inputs_fingerprint"), fingerprint },
    };
    return QString::fromUtf8(QJsonDocument(notes).toJson(QJsonDocument::Compact));
}

CompendiumSourceDescriptor makeDatSource(const QString &sourceId, const QString &checksum, bool enabled = true) {
    CompendiumSourceDescriptor source;
    source.sourceId = sourceId;
    source.displayName = sourceId;
    source.sourceType = QStringLiteral("dat");
    source.snapshotId = sourceId + QStringLiteral("-snap");
    source.snapshotLabel = QStringLiteral("Test snapshot");
    source.path = QStringLiteral("/tmp/unused.dat");
    source.checksumSha256 = checksum;
    source.enabled = enabled;
    source.priority = 10;
    return source;
}

bool seedPlanDatabase(
    const QString &dbPath, const QString &storedFingerprint, const QString &storedChecksum, int schemaVersion = 1) {
    if (QFile::exists(dbPath))
        QFile::remove(dbPath);

    const QString connName = QStringLiteral("seed-plan-db-%1").arg(QDateTime::currentMSecsSinceEpoch());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(dbPath);
    if (!db.open())
        return false;

    bool ok = execSql(db,
                  QStringLiteral("CREATE TABLE compendium_builds ("
                                 "build_id TEXT PRIMARY KEY,"
                                 " schema_version INTEGER NOT NULL,"
                                 " built_at TEXT NOT NULL,"
                                 " source_manifest_json TEXT NOT NULL,"
                                 " notes TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE sources ("
                           "source_id TEXT PRIMARY KEY,"
                           " display_name TEXT NOT NULL,"
                           " source_type TEXT NOT NULL,"
                           " license_id TEXT,"
                           " license_url TEXT,"
                           " attribution_required INTEGER NOT NULL DEFAULT 0,"
                           " priority INTEGER NOT NULL,"
                           " enabled INTEGER NOT NULL DEFAULT 1)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE source_snapshots ("
                           "snapshot_id TEXT PRIMARY KEY,"
                           " source_id TEXT NOT NULL,"
                           " snapshot_label TEXT NOT NULL,"
                           " snapshot_ref TEXT,"
                           " fetched_at TEXT,"
                           " checksum_sha256 TEXT)"))
        && execSql(db,
            QStringLiteral("CREATE TABLE games ("
                           "game_id TEXT PRIMARY KEY,"
                           " system_id INTEGER NOT NULL,"
                           " canonical_title TEXT NOT NULL)"))
        && execSql(db, QStringLiteral("INSERT INTO games VALUES ('game-1', 1, 'Test Game')"))
        && execSql(db,
            QStringLiteral("INSERT INTO compendium_builds VALUES "
                           "('build-1', %1, '2026-01-01T00:00:00Z', '{}', '')")
                .arg(schemaVersion))
        && execSql(db,
            QStringLiteral("INSERT INTO sources VALUES "
                           "('src-a', 'Source A', 'dat', NULL, NULL, 0, 10, 1)"))
        && execSql(db,
            QStringLiteral("INSERT INTO source_snapshots VALUES "
                           "('src-a-snap', 'src-a', 'Snap', NULL, NULL, NULL)"));

    if (ok) {
        {
            QSqlQuery notesQ(db);
            notesQ.prepare(QStringLiteral("UPDATE compendium_builds SET notes = ?"));
            notesQ.addBindValue(notesWithFingerprint(storedFingerprint));
            ok = notesQ.exec();
        }
    }
    if (ok) {
        {
            QSqlQuery checksumQ(db);
            checksumQ.prepare(QStringLiteral("UPDATE source_snapshots SET checksum_sha256 = ?"));
            checksumQ.addBindValue(storedChecksum);
            ok = checksumQ.exec();
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
    return ok;
}

} // namespace

class CompendiumBuildPlanTest : public QObject {
    Q_OBJECT

private slots:
    void plan_skipsWhenChecksumAndFingerprintMatch();
    void plan_enrichmentOnlyWhenFingerprintDiffers();
    void plan_fullWhenReportMissingButChecksumsMatch();
    void plan_incrementalWhenChecksumDiffers();
    void plan_incrementalWhenSourceMissingFromDatabase();
    void plan_fullWhenDatabaseMissing();
    void plan_fullWhenForceRebuild();
    void syncManifestSourcesToDatabase_upsertsSourceMetadata();
};

void CompendiumBuildPlanTest::plan_skipsWhenChecksumAndFingerprintMatch() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString checksum = QStringLiteral("abc123checksum");
    const QString fingerprint = QStringLiteral("fingerprint-v1");
    const QString dbPath = dir.filePath(QStringLiteral("plan.db"));
    QVERIFY(seedPlanDatabase(dbPath, fingerprint, checksum));

    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources = { makeDatSource(QStringLiteral("src-a"), checksum) };
    QVERIFY(planCompendiumBuild(
        dbPath, 1, sources, fingerprint, /*forceFullRebuild=*/false, /*reportExists=*/true, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::Skip);
    QVERIFY(plan.sourcesToIngest.isEmpty());
}

void CompendiumBuildPlanTest::plan_enrichmentOnlyWhenFingerprintDiffers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString checksum = QStringLiteral("abc123checksum");
    const QString dbPath = dir.filePath(QStringLiteral("plan.db"));
    QVERIFY(seedPlanDatabase(dbPath, QStringLiteral("stored-fp"), checksum));

    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources = { makeDatSource(QStringLiteral("src-a"), checksum) };
    QVERIFY(planCompendiumBuild(dbPath, 1, sources, QStringLiteral("new-fp"), false, true, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::EnrichmentOnly);
    QVERIFY(plan.sourcesToIngest.isEmpty());
}

void CompendiumBuildPlanTest::plan_fullWhenReportMissingButChecksumsMatch() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString checksum = QStringLiteral("abc123checksum");
    const QString fingerprint = QStringLiteral("fingerprint-v1");
    const QString dbPath = dir.filePath(QStringLiteral("plan.db"));
    QVERIFY(seedPlanDatabase(dbPath, fingerprint, checksum));

    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources = { makeDatSource(QStringLiteral("src-a"), checksum) };
    QVERIFY(planCompendiumBuild(dbPath, 1, sources, fingerprint, false, /*reportExists=*/false, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::Full);
    QVERIFY(plan.sourcesToIngest.contains(QStringLiteral("src-a")));
}

void CompendiumBuildPlanTest::plan_incrementalWhenChecksumDiffers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath(QStringLiteral("plan.db"));
    QVERIFY(seedPlanDatabase(dbPath, QStringLiteral("fp"), QStringLiteral("stored-checksum")));

    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources
        = { makeDatSource(QStringLiteral("src-a"), QStringLiteral("manifest-checksum")) };
    QVERIFY(planCompendiumBuild(dbPath, 1, sources, QStringLiteral("fp"), false, true, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::IncrementalIngest);
    QCOMPARE(plan.sourcesToIngest.size(), 1);
    QVERIFY(plan.sourcesToIngest.contains(QStringLiteral("src-a")));
}

void CompendiumBuildPlanTest::plan_incrementalWhenSourceMissingFromDatabase() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString checksum = QStringLiteral("shared-checksum");
    const QString dbPath = dir.filePath(QStringLiteral("plan.db"));
    QVERIFY(seedPlanDatabase(dbPath, QStringLiteral("fp"), checksum));

    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources = {
        makeDatSource(QStringLiteral("src-a"), checksum),
        makeDatSource(QStringLiteral("src-b"), checksum),
    };
    QVERIFY(planCompendiumBuild(dbPath, 1, sources, QStringLiteral("fp"), false, true, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::IncrementalIngest);
    QVERIFY(plan.sourcesToIngest.contains(QStringLiteral("src-b")));
    QVERIFY(!plan.sourcesToIngest.contains(QStringLiteral("src-a")));
}

void CompendiumBuildPlanTest::plan_fullWhenDatabaseMissing() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath(QStringLiteral("missing.db"));
    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources = { makeDatSource(QStringLiteral("src-a"), QStringLiteral("abc")) };
    QVERIFY(planCompendiumBuild(dbPath, 1, sources, QStringLiteral("fp"), false, false, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::Full);
    QVERIFY(plan.sourcesToIngest.contains(QStringLiteral("src-a")));
}

void CompendiumBuildPlanTest::plan_fullWhenForceRebuild() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString checksum = QStringLiteral("abc123checksum");
    const QString dbPath = dir.filePath(QStringLiteral("plan.db"));
    QVERIFY(seedPlanDatabase(dbPath, QStringLiteral("fp"), checksum));

    CompendiumBuildPlan plan;
    QString error;
    const QList<CompendiumSourceDescriptor> sources = { makeDatSource(QStringLiteral("src-a"), checksum) };
    QVERIFY(
        planCompendiumBuild(dbPath, 1, sources, QStringLiteral("fp"), /*forceFullRebuild=*/true, true, plan, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(plan.mode, CompendiumBuildMode::Full);
    QVERIFY(plan.sourcesToIngest.contains(QStringLiteral("src-a")));
}

void CompendiumBuildPlanTest::syncManifestSourcesToDatabase_upsertsSourceMetadata() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString dbPath = dir.filePath(QStringLiteral("sync.db"));
    QVERIFY(seedPlanDatabase(dbPath, QStringLiteral("fp"), QStringLiteral("checksum")));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("sync-manifest-test"));
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());

    CompendiumSourceDescriptor source = makeDatSource(QStringLiteral("src-a"), QStringLiteral("checksum"));
    source.enabled = false;
    source.displayName = QStringLiteral("Renamed Source");

    QString error;
    QVERIFY(syncManifestSourcesToDatabase(
        db, { source }, { }, QStringLiteral("build-2"), 1, QStringLiteral("{\"build_id\":\"build-2\"}"), error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT enabled, display_name FROM sources WHERE source_id = 'src-a'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
    QCOMPARE(q.value(1).toString(), QStringLiteral("Renamed Source"));

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("sync-manifest-test"));
}

QTEST_MAIN(CompendiumBuildPlanTest)
#include "test_compendium_build_plan.moc"
