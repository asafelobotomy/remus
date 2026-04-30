#include "cli_commands.h"
#include "cli_helpers.h"
#include "compendium_enrichment.h"
#include "compendium_sql_utilities.h"

#include "../core/compendium_manifest_parser.h"
#include "../metadata/compendium_compiler_service.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace CompendiumSqlUtilities;
using namespace Remus;

int handleBuildCompendiumCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("build-compendium")) return 0;

    const QString manifestPath = ctx.parser.value("compendium-manifest").trimmed();
    const QString outputPath = ctx.parser.value("compendium-output").trimmed();

    if (manifestPath.isEmpty()) {
        qCritical() << "✗ Missing required option: --compendium-manifest <path>";
        return 1;
    }
    if (outputPath.isEmpty()) {
        qCritical() << "✗ Missing required option: --compendium-output <path>";
        return 1;
    }

    const QString buildManifestPath = QFileInfo(manifestPath).absoluteFilePath();
    const QFileInfo manifestInfo(buildManifestPath);
    if (!manifestInfo.exists() || !manifestInfo.isFile()) {
        qCritical() << "✗ Manifest file not found:" << manifestPath;
        return 1;
    }

    QFileInfo outputInfo(outputPath);
    if (!outputInfo.dir().exists() && !QDir().mkpath(outputInfo.dir().absolutePath())) {
        qCritical() << "✗ Failed to create output directory:" << outputInfo.dir().absolutePath();
        return 1;
    }

    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (compendiumDir.isEmpty()) {
        qCritical() << "✗ Could not locate data/compendium directory";
        return 1;
    }

    const QStringList sqlScripts = {
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0001_phase1_canonical_schema.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0002_patch_catalog.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("seeds/0001_regions.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("seeds/0002_systems.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("seeds/0003_merge_policy.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0003_systems_libretro_name.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0004_fts5_search_index.sql")),
    };

    QString buildId;
    int schemaVersion = 0;
    QString manifestJson;
    QJsonArray sourceObjects;
    QList<CompendiumSourceDescriptor> sources;
    QString error;
    if (!parseManifest(buildManifestPath, buildId, schemaVersion, manifestJson, sourceObjects, sources, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        return 1;
    }

    QFile::remove(outputInfo.absoluteFilePath());

    const QString connectionName = QStringLiteral("compendium-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QDateTime startedAt = QDateTime::currentDateTimeUtc();
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(outputInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open output database:" << database.lastError().text();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    QSqlQuery pragmaQuery(database);
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        qCritical() << "✗ Failed to enable foreign keys:" << pragmaQuery.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    // Bulk-build performance: WAL mode avoids fsync stalls and keeps the DB
    // consistent on crash (unlike MEMORY journal which corrupts on OOM/crash).
    // synchronous=OFF skips fsync on WAL frames — safe for a scratch build.
    const QStringList buildPragmas = {
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = OFF"),
        QStringLiteral("PRAGMA temp_store = MEMORY"),
        QStringLiteral("PRAGMA cache_size = -131072"),  // 128 MiB
    };
    for (const QString &pragma : buildPragmas) {
        if (!pragmaQuery.exec(pragma)) {
            qWarning() << "[buildCompendium] PRAGMA hint failed (non-fatal):" << pragma
                       << pragmaQuery.lastError().text();
        }
    }

    for (const QString &scriptPath : sqlScripts) {
        if (!executeSqlScript(database, scriptPath, error)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    if (!database.transaction()) {
        qCritical() << "✗ Failed to start transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    QSqlQuery buildQuery(database);
    buildQuery.prepare(QStringLiteral(
        "INSERT INTO compendium_builds (build_id, schema_version, built_at, source_manifest_json, notes) "
        "VALUES (?, ?, ?, ?, ?)"));
    buildQuery.addBindValue(buildId);
    buildQuery.addBindValue(schemaVersion);
    buildQuery.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    buildQuery.addBindValue(manifestJson);
    buildQuery.addBindValue(QStringLiteral("Phase 1 bootstrap compiler run"));
    if (!execPrepared(buildQuery, error, QStringLiteral("Insert compendium build"))) {
        database.rollback();
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    for (const CompendiumSourceDescriptor &source : sources) {
        QSqlQuery sourceQuery(database);
        sourceQuery.prepare(QStringLiteral(
            "INSERT INTO sources (source_id, display_name, source_type, license_id, license_url, attribution_required, priority, enabled) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        sourceQuery.addBindValue(source.sourceId);
        sourceQuery.addBindValue(source.displayName);
        sourceQuery.addBindValue(source.sourceType);
        sourceQuery.addBindValue(source.licenseId.isEmpty() ? QVariant() : QVariant(source.licenseId));
        sourceQuery.addBindValue(source.licenseUrl.isEmpty() ? QVariant() : QVariant(source.licenseUrl));
        sourceQuery.addBindValue(source.attributionRequired ? 1 : 0);
        sourceQuery.addBindValue(source.priority);
        sourceQuery.addBindValue(source.enabled ? 1 : 0);
        if (!execPrepared(sourceQuery, error, QStringLiteral("Insert source %1").arg(source.sourceId))) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }

        QSqlQuery snapshotQuery(database);
        snapshotQuery.prepare(QStringLiteral(
            "INSERT INTO source_snapshots (snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        snapshotQuery.addBindValue(source.snapshotId);
        snapshotQuery.addBindValue(source.sourceId);
        snapshotQuery.addBindValue(source.snapshotLabel);
        snapshotQuery.addBindValue(source.snapshotRef.isEmpty() ? QVariant() : QVariant(source.snapshotRef));
        snapshotQuery.addBindValue(source.fetchedAt.isEmpty() ? QVariant() : QVariant(source.fetchedAt));
        snapshotQuery.addBindValue(source.checksumSha256.isEmpty() ? QVariant() : QVariant(source.checksumSha256));
        if (!execPrepared(snapshotQuery, error, QStringLiteral("Insert snapshot %1").arg(source.snapshotId))) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    if (!database.commit()) {
        qCritical() << "✗ Failed to commit compendium metadata:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    // ── Run compiler service (extraction → linking → persistence → merge) ──────
    Remus::Compendium::CompendiumBuildConfig buildConfig;
    buildConfig.buildId       = buildId;
    buildConfig.schemaVersion = schemaVersion;
    buildConfig.manifestJson  = manifestJson;
    for (const CompendiumSourceDescriptor &src : sources) {
        Remus::Compendium::CompendiumSourceConfig cfg;
        cfg.sourceId             = src.sourceId;
        cfg.displayName          = src.displayName;
        cfg.sourceType           = src.sourceType;
        cfg.snapshotId           = src.snapshotId;
        cfg.filePath             = src.path;
        cfg.priority             = src.priority;
        cfg.enabled              = src.enabled;
        cfg.licenseId            = src.licenseId;
        cfg.licenseUrl           = src.licenseUrl;
        cfg.attributionRequired  = src.attributionRequired;
        buildConfig.sources.append(cfg);
    }

    if (!database.transaction()) {
        qCritical() << "✗ Failed to start ingestion transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    Remus::Compendium::CompendiumCompilerService service;

    // ── Progress tracking: <output>.progress.json — query with cat or jq ─────
    const QString progressPath = outputInfo.absoluteFilePath() + QStringLiteral(".progress.json");
    int totalEnabled = 0;
    for (const auto &s : std::as_const(buildConfig.sources)) { if (s.enabled) ++totalEnabled; }

    auto writeProgress = [&](const QString &status, int current,
                              const QString &srcId, const Remus::Compendium::CompilerStats &s) {
        const QJsonObject obj {
            {QStringLiteral("status"),           status},
            {QStringLiteral("current"),          current},
            {QStringLiteral("total"),            totalEnabled},
            {QStringLiteral("current_source"),   srcId},
            {QStringLiteral("records_ingested"), s.recordsIngested},
            {QStringLiteral("games_created"),    s.gamesCreated},
            {QStringLiteral("elapsed_ms"),       static_cast<qint64>(timer.elapsed())},
            {QStringLiteral("started_at"),       startedAt.toString(Qt::ISODate)},
            {QStringLiteral("updated_at"),       QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        };
        QFile pf(progressPath);
        if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            pf.write(QJsonDocument(obj).toJson());
    };
    Remus::Compendium::ProgressCallback onProgress = [&](int current, int /*total*/,
                                                          const QString &srcId,
                                                          const Remus::Compendium::CompilerStats &s) {
        qInfo().noquote() << QStringLiteral("[%1/%2] \u2714 %3")
            .arg(current, 3).arg(totalEnabled, 3).arg(srcId);
        writeProgress(QStringLiteral("in_progress"), current, srcId, s);
    };
    writeProgress(QStringLiteral("in_progress"), 0, {}, {});

    const Remus::Compendium::CompilerStats stats = service.run(buildConfig, database, error, onProgress);
    if (!error.isEmpty()) {
        database.rollback();
        qCritical().noquote() << QStringLiteral("✗ Compiler service failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    if (!database.commit()) {
        qCritical() << "✗ Failed to commit ingestion transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }
    writeProgress(QStringLiteral("enriching"), totalEnabled, {}, stats);

    // ── Enrichment pass 1: Libretro metadata DATs ─────────────────────────────
    int metadataGamesEnriched = 0;
    int metadataFactsInserted = 0;
    const QString metadataDir = findDataSubdir(QStringLiteral("metadata"));
    if (!metadataDir.isEmpty()) {
        if (!database.transaction()) {
            qCritical() << "✗ Failed to start libretro enrichment transaction:" << database.lastError().text();
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        if (!CompendiumEnrichment::enrichFromLibretroMetadata(database,
                                                              metadataDir,
                                                              metadataGamesEnriched,
                                                              metadataFactsInserted,
                                                              error)) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ Libretro metadata enrichment failed: %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        if (!database.commit()) {
            qCritical() << "✗ Failed to commit libretro enrichment transaction:" << database.lastError().text();
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    // ── Enrichment pass 2: GameTDB XML databases ───────────────────────────────
    int gametdbGamesEnriched = 0;
    int gametdbFactsInserted = 0;
    const QString gametdbDir = findDataSubdir(QStringLiteral("gametdb"));
    if (!gametdbDir.isEmpty()) {
        if (!database.transaction()) {
            qCritical() << "✗ Failed to start GameTDB enrichment transaction:" << database.lastError().text();
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        if (!CompendiumEnrichment::enrichFromGameTDB(database,
                                                     gametdbDir,
                                                     gametdbGamesEnriched,
                                                     gametdbFactsInserted,
                                                     error)) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ GameTDB enrichment failed: %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        if (!database.commit()) {
            qCritical() << "✗ Failed to commit GameTDB enrichment transaction:" << database.lastError().text();
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    // ── FTS search index population ────────────────────────────────────────────
    {
        qInfo() << "[buildCompendium] Populating FTS search index...";
        if (!database.transaction()) {
            qWarning() << "[buildCompendium] Could not start FTS transaction (non-fatal)";
        } else {
            QSqlQuery ftsQ(database);
            const bool ok1 = ftsQ.exec(QStringLiteral(
                "INSERT INTO games_search(title, game_id, system_id, region_code) "
                "SELECT canonical_title, game_id, system_id, "
                "       COALESCE(primary_region_code, '') FROM games"));
            if (!ok1) {
                qWarning() << "[buildCompendium] FTS canonical title insert failed (non-fatal):"
                           << ftsQ.lastError().text();
                database.rollback();
            } else {
                const bool ok2 = ftsQ.exec(QStringLiteral(
                    "INSERT INTO games_search(title, game_id, system_id, region_code) "
                    "SELECT gn.name_text, gn.game_id, g.system_id, "
                    "       COALESCE(g.primary_region_code, '') "
                    "FROM game_names gn JOIN games g ON g.game_id = gn.game_id"));
                if (!ok2) {
                    qWarning() << "[buildCompendium] FTS alias insert failed (non-fatal):"
                               << ftsQ.lastError().text();
                    database.rollback();
                } else {
                    database.commit();
                    ftsQ.exec(QStringLiteral(
                        "INSERT INTO games_search(games_search) VALUES('optimize')"));
                    qInfo() << "[buildCompendium] FTS index populated and optimized.";
                }
            }
        }
    }

    int systemsCount = scalarCount(database, QStringLiteral("SELECT COUNT(*) FROM systems"), error);
    if (systemsCount < 0) {
        qCritical().noquote() << QStringLiteral("✗ Failed to count systems: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    int conflictsCount = scalarCount(database,
                                     QStringLiteral("SELECT COUNT(*) FROM merge_conflicts WHERE resolution_status = 'unresolved'"),
                                     error);
    if (conflictsCount < 0) {
        qCritical().noquote() << QStringLiteral("✗ Failed to count unresolved conflicts: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    if (!integrityCheckOk(database, error)) {
        qCritical().noquote() << QStringLiteral("✗ Integrity check failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    const QString reportPath = reportPathForDatabase(outputInfo.absoluteFilePath());
    QJsonObject report;
    report.insert(QStringLiteral("build_id"), buildId);
    report.insert(QStringLiteral("schema_version"), schemaVersion);
    report.insert(QStringLiteral("input_sources"), sourceObjects);
    report.insert(QStringLiteral("records_ingested"), stats.recordsIngested);
    report.insert(QStringLiteral("games_created"), stats.gamesCreated);
    report.insert(QStringLiteral("signatures_created"), stats.signaturesCreated);
    report.insert(QStringLiteral("serials_created"), stats.serialsCreated);
    report.insert(QStringLiteral("facts_created"), stats.factsCreated);
    report.insert(QStringLiteral("resolved_fields"), stats.resolvedFields);
    report.insert(QStringLiteral("unresolved_conflicts"), conflictsCount);
    report.insert(QStringLiteral("metadata_games_enriched"), metadataGamesEnriched);
    report.insert(QStringLiteral("metadata_facts_inserted"), metadataFactsInserted);
    report.insert(QStringLiteral("gametdb_games_enriched"), gametdbGamesEnriched);
    report.insert(QStringLiteral("gametdb_facts_inserted"), gametdbFactsInserted);
    report.insert(QStringLiteral("duration_ms"), static_cast<qint64>(timer.elapsed()));

    if (!writeReport(reportPath, report, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Build Compendium ===";
    qInfo() << "Manifest:" << manifestInfo.absoluteFilePath();
    qInfo() << "Output:" << outputInfo.absoluteFilePath();
    qInfo() << "Report:" << reportPath;
    qInfo() << "Build ID:" << buildId;
    qInfo() << "Sources recorded:" << sources.size();
    qInfo() << "Seeded systems:" << systemsCount;
    qInfo() << "Unresolved conflicts:" << conflictsCount;

    writeProgress(QStringLiteral("complete"), totalEnabled, {}, stats);

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return conflictsCount > 0 ? 2 : 0;
}
