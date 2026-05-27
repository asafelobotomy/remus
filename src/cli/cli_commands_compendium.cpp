#include "cli_commands.h"
#include "cli_compendium_build_phases.h"
#include "cli_helpers.h"
#include "compendium_sql_utilities.h"

#include "../core/compendium_manifest_parser.h"
#include "../metadata/compendium_compiler_service.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>

using namespace CompendiumSqlUtilities;
using namespace Remus;

namespace {

QString normalizeManifestJson(const QString &manifestJson)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        return manifestJson;
    }
    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

QString makeStagedSiblingPath(const QString &finalPath)
{
    return finalPath + QStringLiteral(".staged-")
           + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool promoteStagedFile(const QString &stagedPath,
                      const QString &finalPath,
                      QString &error)
{
    if (stagedPath == finalPath) {
        return true;
    }

    const QString backupPath = finalPath + QStringLiteral(".bak");
    QFile::remove(backupPath);

    const bool hadExistingTarget = QFile::exists(finalPath);
    if (hadExistingTarget && !QFile::rename(finalPath, backupPath)) {
        error = QStringLiteral("Failed to stage existing output %1 for replacement").arg(finalPath);
        return false;
    }

    if (!QFile::rename(stagedPath, finalPath)) {
        if (hadExistingTarget) {
            QFile::rename(backupPath, finalPath);
        }
        error = QStringLiteral("Failed to promote staged output %1 to %2").arg(stagedPath, finalPath);
        return false;
    }

    if (hadExistingTarget) {
        QFile::remove(backupPath);
    }
    return true;
}

void releaseDatabase(QSqlDatabase &database, const QString &connectionName)
{
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

int handleBuildCompendiumCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("build-compendium")) return 0;

    const QString manifestPath = ctx.parser.value("compendium-manifest").trimmed();

    // --enrich-source filters which enrichment pass(es) to run.
    // When given without an explicit --compendium-output, auto-derive the output name from the key(s).
    const QString sourceFilterArg = ctx.parser.value("enrich-source").trimmed();
    const QStringList sourceFilter = sourceFilterArg.isEmpty()
        ? QStringList{}
        : sourceFilterArg.split(QLatin1Char(','), Qt::SkipEmptyParts);
    {
        const QStringList validKeys = knownEnrichmentSourceKeys();
        for (const QString &key : sourceFilter) {
            if (!validKeys.contains(key)) {
                qWarning().noquote()
                    << QStringLiteral("[enrich] Unknown --enrich-source key '%1' — will be ignored. "
                                      "Valid keys: %2").arg(key, validKeys.join(", "));
            }
        }
    }
    const QString outputPath = [&]() -> QString {
        if (!sourceFilter.isEmpty() && !ctx.parser.isSet("compendium-output")) {
            const QString suffix = sourceFilter.join(QLatin1Char('_')).toUpper()
                                       .replace(QLatin1Char('-'), QLatin1Char('_'));
            return QStringLiteral("data/compendium/remus_compendium_%1.db").arg(suffix);
        }
        return ctx.parser.value("compendium-output").trimmed();
    }();

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
    const QString finalOutputPath = outputInfo.absoluteFilePath();
    const QString stagedOutputPath = makeStagedSiblingPath(finalOutputPath);
    const QString finalReportPath = reportPathForDatabase(finalOutputPath);
    const QString stagedReportPath = reportPathForDatabase(stagedOutputPath);
    QFile::remove(stagedOutputPath);
    QFile::remove(stagedReportPath);

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
    const QString normalizedManifestJson = normalizeManifestJson(manifestJson);

    // Skip only when the persisted manifest contract exactly matches the current
    // build request. This catches disabled/removed sources and identity changes.
    if (QFileInfo::exists(finalOutputPath)) {
        const QString checkConn = QStringLiteral("compendium-check-")
                                  + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSqlDatabase checkDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), checkConn);
        checkDb.setDatabaseName(finalOutputPath);
        bool manifestMatches = checkDb.open();
        if (manifestMatches) {
            QSqlQuery q(checkDb);
            q.prepare(QStringLiteral(
                "SELECT 1 FROM compendium_builds "
                "WHERE build_id = ? AND schema_version = ? AND source_manifest_json = ? LIMIT 1"));
            q.addBindValue(buildId);
            q.addBindValue(schemaVersion);
            q.addBindValue(normalizedManifestJson);
            manifestMatches = q.exec() && q.next();
            checkDb.close();
        }
        checkDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(checkConn);
        if (manifestMatches && QFileInfo::exists(finalReportPath)) {
            qInfo() << "[build-compendium] Existing DB already matches the requested manifest — skipping rebuild.";
            qInfo() << "[build-compendium] Change the manifest or delete the DB to force a full rebuild.";
            return 0;
        }
    }

    const QString connectionName = QStringLiteral("compendium-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QDateTime startedAt = QDateTime::currentDateTimeUtc();
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(stagedOutputPath);
    if (!database.open()) {
        qCritical() << "✗ Failed to open output database:" << database.lastError().text();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    {
        QSqlQuery pragmaQuery(database);
        if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
            qCritical() << "✗ Failed to enable foreign keys:" << pragmaQuery.lastError().text();
            releaseDatabase(database, connectionName);
            return 1;
        }

        const QStringList buildPragmas = {
            QStringLiteral("PRAGMA journal_mode = WAL"),
            QStringLiteral("PRAGMA synchronous = OFF"),
            QStringLiteral("PRAGMA temp_store = MEMORY"),
            QStringLiteral("PRAGMA cache_size = -131072"),
        };
        for (const QString &pragma : buildPragmas) {
            if (!pragmaQuery.exec(pragma)) {
                qWarning() << "[buildCompendium] PRAGMA hint failed (non-fatal):" << pragma
                           << pragmaQuery.lastError().text();
            }
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

    {
        QSqlQuery buildQuery(database);
        buildQuery.prepare(QStringLiteral(
            "INSERT INTO compendium_builds (build_id, schema_version, built_at, source_manifest_json, notes) "
            "VALUES (?, ?, ?, ?, ?)"));
        buildQuery.addBindValue(buildId);
        buildQuery.addBindValue(schemaVersion);
        buildQuery.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        buildQuery.addBindValue(normalizedManifestJson);
        buildQuery.addBindValue(QStringLiteral("Phase 1 bootstrap compiler run"));
        if (!execPrepared(buildQuery, error, QStringLiteral("Insert compendium build"))) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            return 1;
        }
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
    const QString progressPath = finalOutputPath + QStringLiteral(".progress.json");
    int totalEnabled = 0;
    for (const auto &s : std::as_const(buildConfig.sources)) { if (s.enabled) ++totalEnabled; }

    auto writeProgress = [&](const QString &status, int current,
                              const QString &srcId, const Remus::Compendium::CompilerStats &s,
                              int overallPct = -1) {
        // Default: scale ingest progress over 0-10% of the full pipeline.
        const int pct = overallPct >= 0 ? overallPct
                                        : (totalEnabled > 0 ? current * 10 / totalEnabled : 0);
        const QJsonObject obj {
            {QStringLiteral("status"),           status},
            {QStringLiteral("current"),          current},
            {QStringLiteral("total"),            totalEnabled},
            {QStringLiteral("current_source"),   srcId},
            {QStringLiteral("overall_pct"),      pct},
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
    writeProgress(QStringLiteral("enriching"), totalEnabled, {}, stats, /*overallPct=*/10);

    // ── Enrichment passes (Libretro, GameTDB, OpenVGDB, IGDB) + merge resolve ──
    // Fires before each pass to keep progress.json live during the long enrichment phase.
    // overall_pct model: DAT ingest = 0-10%, enrichment passes = 10-95%, FTS = 95-99%, done = 100%.
    EnrichmentProgressCallback onEnrichProgress = [&](int passIdx, int totalPasses, const QString &passName) {
        const int pct = 10 + (passIdx - 1) * 85 / (totalPasses > 0 ? totalPasses : 1);
        const QJsonObject obj {
            {QStringLiteral("status"),                    QStringLiteral("enriching")},
            {QStringLiteral("current"),                   totalEnabled},
            {QStringLiteral("total"),                     totalEnabled},
            {QStringLiteral("current_source"),            QString()},
            {QStringLiteral("enrichment_pass_current"),   passIdx},
            {QStringLiteral("enrichment_pass_total"),     totalPasses},
            {QStringLiteral("enrichment_pass_name"),      passName},
            {QStringLiteral("overall_pct"),               pct},
            {QStringLiteral("records_ingested"),          stats.recordsIngested},
            {QStringLiteral("games_created"),             stats.gamesCreated},
            {QStringLiteral("elapsed_ms"),                static_cast<qint64>(timer.elapsed())},
            {QStringLiteral("started_at"),                startedAt.toString(Qt::ISODate)},
            {QStringLiteral("updated_at"),                QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        };
        QFile pf(progressPath);
        if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            pf.write(QJsonDocument(obj).toJson());
    };

    // ── Enrichment passes (Libretro, GameTDB, OpenVGDB, IGDB) + merge resolve ──
    EnrichmentStats enrichStats;
    {
        const QString metadataDir   = findDataSubdir(QStringLiteral("metadata"));
        const QString gametdbDir    = findDataSubdir(QStringLiteral("gametdb"));
        const QString openvgdbPath  = findOpenVGDBPath();
        const QString mameCatverPath = findMameCatverPath();
        const QString mameListXmlPath = findMameListXmlPath();
        const QString credPath      = outputInfo.dir().filePath(
                                          QStringLiteral("enrichment-credentials.json"));
        if (!runCompendiumEnrichmentPasses(database, metadataDir, gametdbDir,
                                          openvgdbPath, credPath, mameCatverPath,
                                          mameListXmlPath,
                                          enrichStats, error, onEnrichProgress,
                                          sourceFilter)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    {
        QString ftsError;
        if (!populateCompendiumFtsIndex(database, enrichStats.ftsRowsIndexed, ftsError)) {
            qCritical().noquote() << QStringLiteral("✗ FTS rebuild failed: %1").arg(ftsError);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
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

    const QString reportPath = stagedReportPath;
    QJsonObject report;
    report.insert(QStringLiteral("build_id"), buildId);
    report.insert(QStringLiteral("schema_version"), schemaVersion);
    report.insert(QStringLiteral("input_sources"), sourceObjects);
    report.insert(QStringLiteral("records_ingested"), stats.recordsIngested);
    report.insert(QStringLiteral("games_created"), stats.gamesCreated);
    report.insert(QStringLiteral("games_deduplicated"), stats.deduplicatedGames);
    report.insert(QStringLiteral("signatures_created"), stats.signaturesCreated);
    report.insert(QStringLiteral("serials_created"), stats.serialsCreated);
    report.insert(QStringLiteral("facts_created"), stats.factsCreated);
    report.insert(QStringLiteral("resolved_fields"), stats.resolvedFields);
    report.insert(QStringLiteral("unresolved_conflicts"), conflictsCount);
    insertEnrichmentStatsReportFields(report, enrichStats, QStringLiteral("post_enrich_resolved_fields"));
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
    qInfo() << "Output:" << finalOutputPath;
    qInfo() << "Report:" << finalReportPath;
    qInfo() << "Build ID:" << buildId;
    qInfo() << "Sources recorded:" << sources.size();
    qInfo() << "Seeded systems:" << systemsCount;
    qInfo() << "Unresolved conflicts:" << conflictsCount;

    writeProgress(QStringLiteral("complete"), totalEnabled, {}, stats, /*overallPct=*/100);

    releaseDatabase(database, connectionName);

    if (!promoteStagedFile(stagedOutputPath, finalOutputPath, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        QFile::remove(stagedOutputPath);
        QFile::remove(stagedReportPath);
        return 1;
    }
    if (!promoteStagedFile(stagedReportPath, finalReportPath, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        QFile::remove(stagedReportPath);
        // Roll back the DB promotion so the next invocation is forced to rebuild.
        if (!QFile::rename(finalOutputPath, stagedOutputPath)) {
            qCritical() << "[build-compendium] Could not roll back DB promotion —"
                        << "delete" << finalOutputPath << "and rerun to recover.";
        }
        return 1;
    }

    return conflictsCount > 0 ? 2 : 0;
}
