#include "cli_commands.h"
#include "cli_compendium_build_phases.h"
#include "cli_helpers.h"
#include "compendium_consolidate_thumbnails.h"
#include "compendium_enrichment.h"
#include "compendium_progress.h"
#include "compendium_sql_utilities.h"

#include "../core/compendium_manifest_parser.h"
#include "../metadata/compendium_compiler_service.h"
#include "../core/constants/constants.h"

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
#include <QProcess>
#include <QCoreApplication>
#include <algorithm>

using namespace CompendiumSqlUtilities;
using namespace Remus;

namespace {

void releaseDatabase(QSqlDatabase &database, const QString &connectionName);

QString normalizeManifestJson(const QString &manifestJson) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        return manifestJson;
    }
    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

QString makeStagedSiblingPath(const QString &finalPath) {
    return finalPath + QStringLiteral(".staged-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void removeStagedArtifactFiles(const QString &stagedDbPath, const QString &stagedReportPath = QString()) {
    if (stagedDbPath.isEmpty())
        return;
    QFile::remove(stagedDbPath);
    QFile::remove(stagedDbPath + QStringLiteral("-shm"));
    QFile::remove(stagedDbPath + QStringLiteral("-wal"));
    if (!stagedReportPath.isEmpty())
        QFile::remove(stagedReportPath);
}

bool stagedDbHasPopulatedContent(const QString &stagedDbPath) {
    if (!QFileInfo::exists(stagedDbPath)) {
        return false;
    }

    const QString connectionName
        = QStringLiteral("compendium-staged-purge-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(stagedDbPath);
    if (!database.open()) {
        qWarning().noquote() << QStringLiteral(
            "[build-compendium] Keeping staged DB (could not open for purge check): %1")
                                    .arg(stagedDbPath);
        return true;
    }

    QSqlQuery checkpointQ(database);
    checkpointQ.exec(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)"));

    QSqlQuery countQ(database);
    int gameCount = 0;
    if (countQ.exec(QStringLiteral("SELECT COUNT(*) FROM games")) && countQ.next()) {
        gameCount = countQ.value(0).toInt();
    } else {
        qWarning().noquote()
            << QStringLiteral("[build-compendium] Keeping staged DB (games table unreadable): %1").arg(stagedDbPath);
        releaseDatabase(database, connectionName);
        return true;
    }

    releaseDatabase(database, connectionName);
    return gameCount > 0;
}

void purgeStaleStagedSiblings(const QString &finalOutputPath) {
    const QFileInfo finalInfo(finalOutputPath);
    const QDir dir = finalInfo.dir();
    const QString prefix = finalInfo.fileName() + QStringLiteral(".staged-");
    const QFileInfoList entries = dir.entryInfoList(QDir::Files);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        if (!name.startsWith(prefix))
            continue;
        if (name.endsWith(QStringLiteral("-shm")) || name.endsWith(QStringLiteral("-wal"))) {
            continue;
        }
        const QString stagedDbPath = entry.absoluteFilePath();
        if (stagedDbHasPopulatedContent(stagedDbPath)) {
            qInfo().noquote()
                << QStringLiteral("[build-compendium] Preserving recoverable staged DB: %1").arg(stagedDbPath);
            continue;
        }
        removeStagedArtifactFiles(stagedDbPath, reportPathForDatabase(stagedDbPath));
    }
}

void writeTerminalProgress(
    const QString &progressPath, const QString &status, qint64 elapsedMs, const QString &buildPhase = QString()) {
    QJsonObject obj {
        { QStringLiteral("status"), status },
        { QStringLiteral("elapsed_ms"), elapsedMs },
        { QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
    };
    if (!buildPhase.isEmpty()) {
        obj.insert(QStringLiteral("build_phase"), buildPhase);
    }
    QFile progressFile(progressPath);
    if (progressFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        progressFile.write(QJsonDocument(obj).toJson());
}

class StagedBuildGuard {
public:
    StagedBuildGuard(const QString &stagedDbPath, const QString &stagedReportPath,
        const QString &progressPath = QString(), QElapsedTimer *timer = nullptr)
        : m_stagedDbPath(stagedDbPath)
        , m_stagedReportPath(stagedReportPath)
        , m_progressPath(progressPath)
        , m_timer(timer) { }

    ~StagedBuildGuard() {
        if (!m_active)
            return;
        if (m_ingestCommitted) {
            if (!m_progressPath.isEmpty() && m_timer != nullptr) {
                writeTerminalProgress(
                    m_progressPath, QStringLiteral("partial"), m_timer->elapsed(), QStringLiteral("ingest_complete"));
            }
            qWarning().noquote() << QStringLiteral("[build-compendium] Staged database preserved at %1 "
                                                   "(ingest committed; rerun build to resume)")
                                        .arg(m_stagedDbPath);
            return;
        }
        if (!m_progressPath.isEmpty() && m_timer != nullptr)
            writeTerminalProgress(m_progressPath, QStringLiteral("failed"), m_timer->elapsed());
        removeStagedArtifactFiles(m_stagedDbPath, m_stagedReportPath);
    }

    void release() {
        m_active = false;
    }

    void markIngestCommitted() {
        m_ingestCommitted = true;
    }

private:
    QString m_stagedDbPath;
    QString m_stagedReportPath;
    QString m_progressPath;
    QElapsedTimer *m_timer = nullptr;
    bool m_active = true;
    bool m_ingestCommitted = false;
};

bool copyDatabaseArtifact(const QString &srcDbPath, const QString &destDbPath, QString &error) {
    if (QFileInfo::exists(destDbPath) && !QFile::remove(destDbPath)) {
        error = QStringLiteral("Failed to replace %1").arg(destDbPath);
        return false;
    }
    if (!QFile::copy(srcDbPath, destDbPath)) {
        error = QStringLiteral("Failed to copy %1 to %2").arg(srcDbPath, destDbPath);
        return false;
    }
    for (const QString &suffix : { QStringLiteral("-wal"), QStringLiteral("-shm") }) {
        const QString srcSide = srcDbPath + suffix;
        if (!QFileInfo::exists(srcSide)) {
            continue;
        }
        const QString destSide = destDbPath + suffix;
        QFile::remove(destSide);
        if (!QFile::copy(srcSide, destSide)) {
            error = QStringLiteral("Failed to copy %1").arg(srcSide);
            return false;
        }
    }
    return true;
}

bool promoteStagedFile(const QString &stagedPath, const QString &finalPath, QString &error) {
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
        const QString timestampedBackup = finalPath + QStringLiteral(".pre-rebuild-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".bak");
        if (!QFile::rename(backupPath, timestampedBackup)) {
            qWarning().noquote() << QStringLiteral("[build-compendium] Kept rollback backup at %1").arg(backupPath);
        }
    }
    return true;
}

bool checkpointAndPromoteIngestDatabase(QSqlDatabase &database, const QString &connectionName,
    const QString &stagedOutputPath, const QString &finalOutputPath, const QString &buildId,
    StagedBuildGuard &stagedGuard, bool &ingestPromotedEarly, QString &error) {
    QSqlQuery checkpointQ(database);
    if (!checkpointQ.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"))) {
        error = checkpointQ.lastError().text();
        return false;
    }

    const QFileInfo finalInfo(finalOutputPath);
    const QString backupDir = finalInfo.dir().filePath(QStringLiteral("backups"));
    if (!QDir().mkpath(backupDir)) {
        error = QStringLiteral("Failed to create backup directory: %1").arg(backupDir);
        return false;
    }

    QString safeBuildId = buildId;
    for (const QChar ch : QStringLiteral("/\\:?*\"<>|")) {
        safeBuildId.replace(ch, QLatin1Char('_'));
    }
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmssZ"));
    const QString backupPath = QDir(backupDir).filePath(finalInfo.baseName() + QStringLiteral(".post-ingest.")
        + safeBuildId + QLatin1Char('.') + stamp + QStringLiteral(".db"));

    releaseDatabase(database, connectionName);

    if (!copyDatabaseArtifact(stagedOutputPath, backupPath, error)) {
        return false;
    }
    if (!promoteStagedFile(stagedOutputPath, finalOutputPath, error)) {
        return false;
    }

    stagedGuard.release();
    ingestPromotedEarly = true;

    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(finalOutputPath);
    if (!database.open()) {
        error = database.lastError().text();
        return false;
    }
    applyCompendiumBuildPragmas(database);

    persistBuildPhaseNotes(
        database, buildId, QStringLiteral("ingest_complete"), QStringLiteral("Ingest promoted; enrichment pending"));

    qInfo().noquote()
        << QStringLiteral("[build-compendium] Ingest promoted to %1 (backup: %2)").arg(finalOutputPath, backupPath);
    return true;
}

void releaseDatabase(QSqlDatabase &database, const QString &connectionName) {
    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void logEnrichmentPassErrors(const EnrichmentStats &stats) {
    for (const EnrichmentStats::PassError &pe : stats.passErrors) {
        qCritical().noquote() << QStringLiteral("  - [%1] %2: %3").arg(pe.sourceKey, pe.passName, pe.message);
    }
}

bool handleInterruptedEnrichmentAfterPromote(QSqlDatabase &database, const QString &buildId,
    const QString &progressPath, qint64 elapsedMs, const EnrichmentStats &stats,
    const QString &phase = QStringLiteral("ingest_complete")) {
    persistBuildPhaseNotes(database, buildId, phase, QStringLiteral("Enrichment interrupted after ingest promote"));
    writeTerminalProgress(progressPath, QStringLiteral("failed"), elapsedMs, phase);
    qCritical().noquote() << QStringLiteral("✗ %1 enrichment pass(es) failed after ingest promote; build interrupted")
                                 .arg(stats.passesFailedWithError);
    logEnrichmentPassErrors(stats);
    return true;
}

bool handlePostPromoteFailure(QSqlDatabase &database, const QString &buildId, const QString &progressPath,
    qint64 elapsedMs, bool ingestPromotedEarly, const QString &errorMessage) {
    if (!ingestPromotedEarly) {
        return false;
    }
    persistBuildPhaseNotes(
        database, buildId, QStringLiteral("ingest_complete"), QStringLiteral("Build interrupted after ingest promote"));
    writeTerminalProgress(progressPath, QStringLiteral("failed"), elapsedMs, QStringLiteral("ingest_complete"));
    qCritical().noquote() << QStringLiteral("✗ %1").arg(errorMessage);
    return true;
}

} // namespace

int handleBuildCompendiumCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("build-compendium"))
        return 0;

    const QString manifestPath = ctx.parser.value("compendium-manifest").trimmed();

    // --enrich-source filters which enrichment pass(es) to run.
    // When given without an explicit --compendium-output, auto-derive the output name from the key(s).
    const QString sourceFilterArg = ctx.parser.value("enrich-source").trimmed();
    const QStringList sourceFilter
        = sourceFilterArg.isEmpty() ? QStringList { } : sourceFilterArg.split(QLatin1Char(','), Qt::SkipEmptyParts);
    {
        const QStringList validKeys = knownEnrichmentSourceKeys();
        for (const QString &key : sourceFilter) {
            if (!validKeys.contains(key)) {
                qWarning().noquote() << QStringLiteral("[enrich] Unknown --enrich-source key '%1' — will be ignored. "
                                                       "Valid keys: %2")
                                            .arg(key, validKeys.join(", "));
            }
        }
    }
    const QString outputPath = [&]() -> QString {
        if (!sourceFilter.isEmpty() && !ctx.parser.isSet("compendium-output")) {
            const QString suffix
                = sourceFilter.join(QLatin1Char('_')).toUpper().replace(QLatin1Char('-'), QLatin1Char('_'));
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
    const QString progressPath = finalOutputPath + QStringLiteral(".progress.json");
    CompendiumProgressWriter progressWriter(progressPath);
    const QString stagedReportPath = reportPathForDatabase(stagedOutputPath);

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
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0005_game_external_ids.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0006_game_achievement_count.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0007_disc_sets.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0008_game_facts_lookup_index.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0009_game_signatures_source_entry_key.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0010_game_extended_metadata.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0011_materialized_coverage.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0012_game_assets.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0013_disc_tracks_per_set_unique.sql")),
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
    if (!verifyAndNormalizeSourceChecksums(sources, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        return 1;
    }
    const QString normalizedManifestJson = normalizeManifestJson(manifestJson);

    const QString metadataDir = findMetadataDir();
    const QString gametdbDir = findGameTDBDir();
    const QString openvgdbPath = findOpenVGDBPath();
    const QString mameCatverPath = findMameCatverPath();
    const QString mameListXmlPath = findMameListXmlPath();
    const QString launchboxMetadataPath = findLaunchBoxMetadataPath();
    const QString credPath = outputInfo.dir().filePath(QStringLiteral("enrichment-credentials.json"));
    const EnrichmentCliOptions enrichOpts = resolveEnrichmentCliOptions(ctx.parser, sourceFilter);
    const bool offlineOnlyEnrichment = enrichOpts.offlineOnlyEnrichment;
    const bool onlineEnrichmentAll = enrichOpts.onlineEnrichmentAll;
    const bool strictOfflineEnrichment = enrichOpts.strictOfflineEnrichment;
    const QStringList effectiveSourceFilter = enrichOpts.sourceFilter;
    const QString enrichmentFingerprint = computeEnrichmentInputsFingerprint(metadataDir, gametdbDir, openvgdbPath,
        mameCatverPath, mameListXmlPath, launchboxMetadataPath, credPath, effectiveSourceFilter, offlineOnlyEnrichment,
        onlineEnrichmentAll, findRemusThumbnailsDir(), findLibretroAcquisitionDir());

    const bool forceFullRebuild = ctx.parser.isSet(QStringLiteral("force-full-rebuild"));
    const bool forceEnrichment = ctx.parser.isSet(QStringLiteral("force-enrichment"));
    CompendiumBuildPlan buildPlan;
    if (!planCompendiumBuild(finalOutputPath, schemaVersion, sources, enrichmentFingerprint, forceFullRebuild,
            forceEnrichment, QFileInfo::exists(finalReportPath), progressPath, buildPlan, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        return 1;
    }

    if (buildPlan.mode == CompendiumBuildMode::Skip) {
        bool manifestDrift = false;
        {
            const QString connectionName
                = QStringLiteral("compendium-manifest-drift-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(finalOutputPath);
            if (database.open()) {
                QSqlQuery query(database);
                if (query.exec(QStringLiteral("SELECT source_manifest_json FROM compendium_builds "
                                              "ORDER BY built_at DESC LIMIT 1"))
                    && query.next()) {
                    manifestDrift = normalizeManifestJson(query.value(0).toString()) != normalizedManifestJson;
                }
                releaseDatabase(database, connectionName);
            } else {
                QSqlDatabase::removeDatabase(connectionName);
            }
        }

        if (manifestDrift) {
            qInfo() << "[build-compendium] Source checksums unchanged — syncing manifest metadata.";
            const QString connectionName
                = QStringLiteral("compendium-manifest-sync-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(finalOutputPath);
            if (!database.open()) {
                qCritical() << "✗ Failed to open database for manifest sync:" << database.lastError().text();
                return 1;
            }
            applyCompendiumBuildPragmas(database);
            if (!database.transaction()) {
                qCritical() << "✗ Failed to start manifest sync transaction:" << database.lastError().text();
                releaseDatabase(database, connectionName);
                return 1;
            }
            if (!syncManifestSourcesToDatabase(
                    database, sources, { }, buildId, schemaVersion, normalizedManifestJson, error)) {
                database.rollback();
                qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
                releaseDatabase(database, connectionName);
                return 1;
            }
            if (!database.commit()) {
                qCritical() << "✗ Failed to commit manifest sync transaction:" << database.lastError().text();
                releaseDatabase(database, connectionName);
                return 1;
            }
            releaseDatabase(database, connectionName);
            return 0;
        }

        qInfo() << "[build-compendium] Existing DB already matches manifest source checksums and enrichment "
                   "inputs — skipping rebuild.";
        qInfo() << "[build-compendium] Use --force-full-rebuild or delete the DB to force a full rebuild.";
        return 0;
    }

    const QDateTime startedAt = QDateTime::currentDateTimeUtc();
    progressWriter.setStartedAt(startedAt);
    QElapsedTimer timer;
    timer.start();
    purgeStaleStagedSiblings(finalOutputPath);
    StagedBuildGuard stagedGuard(stagedOutputPath, stagedReportPath, progressPath, &timer);

    if (buildPlan.mode == CompendiumBuildMode::EnrichmentOnly) {
        qInfo() << "[build-compendium] Source checksums unchanged — running enrichment-only refresh.";
        if (!QFile::copy(finalOutputPath, stagedOutputPath)) {
            qCritical() << "✗ Failed to stage existing database for enrichment refresh:" << finalOutputPath;
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }

        const QString connectionName
            = QStringLiteral("compendium-enrich-refresh-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(stagedOutputPath);
        if (!database.open()) {
            qCritical() << "✗ Failed to open staged database:" << database.lastError().text();
            return 1;
        }
        applyCompendiumBuildPragmas(database);

        if (!database.transaction()) {
            qCritical() << "✗ Failed to start enrichment metadata transaction:" << database.lastError().text();
            return 1;
        }
        if (!syncManifestSourcesToDatabase(
                database, sources, { }, buildId, schemaVersion, normalizedManifestJson, error)) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            QFile::remove(stagedOutputPath);
            return 1;
        }
        if (!database.commit()) {
            qCritical() << "✗ Failed to commit enrichment metadata transaction:" << database.lastError().text();
            releaseDatabase(database, connectionName);
            QFile::remove(stagedOutputPath);
            return 1;
        }

        QJsonObject existingReport;
        QFile existingReportFile(finalReportPath);
        if (existingReportFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonDocument doc = QJsonDocument::fromJson(existingReportFile.readAll());
            if (doc.isObject())
                existingReport = doc.object();
        }

        EnrichmentProgressCallback onEnrichProgress = [&](int passIdx, int totalPasses, const QString &passName) {
            const int pct = 10 + (passIdx - 1) * 85 / (totalPasses > 0 ? totalPasses : 1);
            const QJsonObject obj {
                { QStringLiteral("status"), QStringLiteral("enriching") },
                { QStringLiteral("enrichment_pass_current"), passIdx },
                { QStringLiteral("enrichment_pass_total"), totalPasses },
                { QStringLiteral("enrichment_pass_name"), passName },
                { QStringLiteral("overall_pct"), pct },
                { QStringLiteral("elapsed_ms"), static_cast<qint64>(timer.elapsed()) },
            };
            QFile pf(progressPath);
            if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
                pf.write(QJsonDocument(obj).toJson());
        };

        QJsonObject report;
        if (runCompendiumEnrichmentOnlyRefresh(database, buildId, finalReportPath, existingReport,
                enrichmentFingerprint, metadataDir, gametdbDir, openvgdbPath, credPath, mameCatverPath, mameListXmlPath,
                launchboxMetadataPath, effectiveSourceFilter, onEnrichProgress, report, error, offlineOnlyEnrichment,
                onlineEnrichmentAll, &progressWriter)
            != 0) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }

        report.insert(QStringLiteral("build_id"), buildId);
        report.insert(QStringLiteral("schema_version"), schemaVersion);
        report.insert(QStringLiteral("duration_ms"), static_cast<qint64>(timer.elapsed()));

        int conflictsCount = scalarCount(database,
            QStringLiteral("SELECT COUNT(*) FROM merge_conflicts WHERE resolution_status = 'unresolved'"), error);
        if (conflictsCount < 0) {
            qCritical().noquote() << QStringLiteral("✗ Failed to count unresolved conflicts: %1").arg(error);
            releaseDatabase(database, connectionName);
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }
        report.insert(QStringLiteral("unresolved_conflicts"), conflictsCount);

        if (!writeReport(stagedReportPath, report, error)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }

        releaseDatabase(database, connectionName);
        if (!promoteStagedFile(stagedOutputPath, finalOutputPath, error)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }
        if (!promoteStagedFile(stagedReportPath, finalReportPath, error)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }
        stagedGuard.release();
        writeTerminalProgress(progressPath, QStringLiteral("complete"), timer.elapsed());
        return conflictsCount > 0 ? 2 : 0;
    }

    const bool incrementalIngest = buildPlan.mode == CompendiumBuildMode::IncrementalIngest;
    if (incrementalIngest) {
        qInfo().noquote() << QStringLiteral("[build-compendium] Incremental ingest for %1 changed source(s).")
                                 .arg(buildPlan.sourcesToIngest.size());
        if (!QFile::copy(finalOutputPath, stagedOutputPath)) {
            qCritical() << "✗ Failed to copy existing database for incremental ingest:" << finalOutputPath;
            return 1;
        }
    } else {
        qInfo() << "[build-compendium] Running full compendium rebuild.";
        removeStagedArtifactFiles(stagedOutputPath, stagedReportPath);
    }

    const QString connectionName = QStringLiteral("compendium-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ingestPromotedEarly = false;

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(stagedOutputPath);
    if (!database.open()) {
        qCritical() << "✗ Failed to open output database:" << database.lastError().text();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    { applyCompendiumBuildPragmas(database); }

    if (!incrementalIngest) {
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
            sourceQuery.prepare(QStringLiteral("INSERT INTO sources (source_id, display_name, source_type, license_id, "
                                               "license_url, attribution_required, priority, enabled) "
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
            snapshotQuery.prepare(
                QStringLiteral("INSERT INTO source_snapshots (snapshot_id, source_id, snapshot_label, "
                               "snapshot_ref, fetched_at, checksum_sha256) "
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

        if (!purgeDisabledSourcesIngestData(database, sources, error)) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            return 1;
        }

        if (!database.commit()) {
            qCritical() << "✗ Failed to commit compendium metadata:" << database.lastError().text();
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    } else {
        if (!database.transaction()) {
            qCritical() << "✗ Failed to start incremental metadata transaction:" << database.lastError().text();
            releaseDatabase(database, connectionName);
            return 1;
        }
        if (!syncManifestSourcesToDatabase(
                database, sources, buildPlan.sourcesToIngest, buildId, schemaVersion, normalizedManifestJson, error)) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            return 1;
        }
        if (!purgeDisabledSourcesIngestData(database, sources, error)) {
            database.rollback();
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            releaseDatabase(database, connectionName);
            return 1;
        }
        if (!database.commit()) {
            qCritical() << "✗ Failed to commit incremental metadata:" << database.lastError().text();
            releaseDatabase(database, connectionName);
            return 1;
        }
    }

    // ── Run compiler service (extraction → linking → persistence → merge) ──────
    Remus::Compendium::CompendiumBuildConfig buildConfig;
    buildConfig.buildId = buildId;
    buildConfig.schemaVersion = schemaVersion;
    buildConfig.manifestJson = manifestJson;
    for (const CompendiumSourceDescriptor &src : sources) {
        Remus::Compendium::CompendiumSourceConfig cfg;
        cfg.sourceId = src.sourceId;
        cfg.displayName = src.displayName;
        cfg.sourceType = src.sourceType;
        cfg.snapshotId = src.snapshotId;
        cfg.filePath = src.path;
        cfg.priority = src.priority;
        cfg.enabled = src.enabled;
        cfg.licenseId = src.licenseId;
        cfg.licenseUrl = src.licenseUrl;
        cfg.attributionRequired = src.attributionRequired;
        buildConfig.sources.append(cfg);
    }

    std::sort(buildConfig.sources.begin(), buildConfig.sources.end(),
        [](const Remus::Compendium::CompendiumSourceConfig &a, const Remus::Compendium::CompendiumSourceConfig &b) {
            if (a.priority != b.priority)
                return a.priority > b.priority;
            return a.sourceId < b.sourceId;
        });

    if (!database.transaction()) {
        qCritical() << "✗ Failed to start ingestion transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    Remus::Compendium::CompendiumCompilerService service;

    // ── Progress tracking: <output>.progress.json — query with cat or jq ─────
    int totalEnabled = 0;
    for (const auto &s : std::as_const(buildConfig.sources)) {
        if (s.enabled) {
            if (incrementalIngest && !buildPlan.sourcesToIngest.contains(s.sourceId)) {
                continue;
            }
            ++totalEnabled;
        }
    }

    auto writeProgress
        = [&](const QString &status, int current, const QString &srcId, const Remus::Compendium::CompilerStats &s,
              int overallPct = -1, const QString &buildPhase = QString()) {
              // Default: scale ingest progress over 0-10% of the full pipeline.
              const int pct = overallPct >= 0 ? overallPct : (totalEnabled > 0 ? current * 10 / totalEnabled : 0);
              QJsonObject obj {
                  { QStringLiteral("status"), status },
                  { QStringLiteral("current"), current },
                  { QStringLiteral("total"), totalEnabled },
                  { QStringLiteral("current_source"), srcId },
                  { QStringLiteral("overall_pct"), pct },
                  { QStringLiteral("records_ingested"), s.recordsIngested },
                  { QStringLiteral("games_created"), s.gamesCreated },
                  { QStringLiteral("elapsed_ms"), static_cast<qint64>(timer.elapsed()) },
                  { QStringLiteral("started_at"), startedAt.toString(Qt::ISODate) },
                  { QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
              };
              if (!buildPhase.isEmpty()) {
                  obj.insert(QStringLiteral("build_phase"), buildPhase);
              }
              QFile pf(progressPath);
              if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
                  pf.write(QJsonDocument(obj).toJson());
          };
    Remus::Compendium::CompilerRunOptions runOptions;
    if (incrementalIngest) {
        runOptions.ingestSourceIds = buildPlan.sourcesToIngest;
        runOptions.purgeChangedSources = true;
        runOptions.preloadIdentityLinker = true;
    }
    Remus::Compendium::ProgressCallback onProgress
        = [&](int current, int /*total*/, const QString &srcId, const Remus::Compendium::CompilerStats &s) {
              qInfo().noquote() << QStringLiteral("[%1/%2] \u2714 %3").arg(current, 3).arg(totalEnabled, 3).arg(srcId);
              const int pct = totalEnabled > 0 ? 5 + (current * 5 / totalEnabled) : 5;
              writeProgress(QStringLiteral("in_progress"), current, srcId, s, pct);
          };
    runOptions.onExtractProgress = [&](int current, int total, const QString &srcId) {
        qInfo().noquote() << QStringLiteral("[extract %1/%2] %3").arg(current, 3).arg(total, 3).arg(srcId);
        const int pct = total > 0 ? current * 5 / total : 0;
        writeProgress(QStringLiteral("extracting"), current, srcId, { }, pct);
    };
    writeProgress(QStringLiteral("extracting"), 0, { }, { }, 0);

    const Remus::Compendium::CompilerStats stats = service.run(buildConfig, database, error, onProgress, runOptions);
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
    stagedGuard.markIngestCommitted();
    if (!checkpointAndPromoteIngestDatabase(database, connectionName, stagedOutputPath, finalOutputPath, buildId,
            stagedGuard, ingestPromotedEarly, error)) {
        qCritical().noquote() << QStringLiteral("✗ Post-ingest checkpoint failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }
    writeProgress(
        QStringLiteral("enriching"), totalEnabled, { }, stats, /*overallPct=*/10, QStringLiteral("ingest_complete"));

    // ── Consolidate libretro acquisition → remus-thumbnails blobs ─────────────
    if (!ctx.parser.isSet(QStringLiteral("skip-consolidate-thumbnails"))) {
        const QString acquisitionDir = findLibretroAcquisitionDir();
        const QString thumbnailDir = findRemusThumbnailsDir();
        if (strictOfflineEnrichment && !CompendiumThumbnails::remusThumbnailsManifestExists(thumbnailDir)
            && (acquisitionDir.isEmpty() || !QDir(acquisitionDir).exists())) {
            qCritical() << "✗ --strict-offline: remus-thumbnails manifest missing and no acquisition tree";
            if (handlePostPromoteFailure(database, buildId, progressPath, timer.elapsed(), ingestPromotedEarly,
                    QStringLiteral("--strict-offline: remus-thumbnails manifest missing and no acquisition tree"))) {
                database.close();
                QSqlDatabase::removeDatabase(connectionName);
                return 1;
            }
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        if (!acquisitionDir.isEmpty() && QDir(acquisitionDir).exists()) {
            ConsolidateThumbnailsOptions cOpts;
            cOpts.acquisitionDir = acquisitionDir;
            cOpts.outputDir = thumbnailDir;
            cOpts.pruneAcquisitionSources = ctx.parser.isSet(QStringLiteral("prune-acquisition-sources"));
            const QString sysArg = ctx.parser.value(QStringLiteral("thumbnail-system")).trimmed();
            if (!sysArg.isEmpty()) {
                cOpts.systems = sysArg.split(QLatin1Char(','), Qt::SkipEmptyParts);
            }
            cOpts.snapQuality = ctx.parser.value(QStringLiteral("thumbnail-snap-quality")).toInt();
            if (cOpts.snapQuality <= 0) {
                cOpts.snapQuality = 85;
            }
            cOpts.snapLossless = ctx.parser.isSet(QStringLiteral("thumbnail-snap-lossless"));
            cOpts.onProgress = [&](int done, int total, const QString &detail) {
                progressWriter.setElapsedMs(timer.elapsed());
                progressWriter.writeTaskProgress(QStringLiteral("consolidate_thumbnails"), QStringLiteral("scanning"),
                    done, total, detail, 8 + (total > 0 ? done * 2 / total : 0));
            };
            ConsolidateThumbnailsStats cStats;
            QString cError;
            qInfo() << "[consolidate-thumbnails] Starting consolidate pass";
            if (!CompendiumThumbnails::consolidateThumbnails(database, cOpts, cStats, cError)) {
                qCritical().noquote() << QStringLiteral("✗ consolidate-thumbnails failed: %1").arg(cError);
                if (handlePostPromoteFailure(
                        database, buildId, progressPath, timer.elapsed(), ingestPromotedEarly, cError)) {
                    database.close();
                    QSqlDatabase::removeDatabase(connectionName);
                    return 1;
                }
                database.close();
                QSqlDatabase::removeDatabase(connectionName);
                return 1;
            }
            qInfo().noquote() << QStringLiteral("[consolidate-thumbnails] games=%1 written=%2 dedup=%3 misses=%4")
                                     .arg(cStats.gamesScanned)
                                     .arg(cStats.assetsWritten)
                                     .arg(cStats.assetsDeduplicated)
                                     .arg(cStats.misses);
        }
        if (strictOfflineEnrichment && !CompendiumThumbnails::remusThumbnailsManifestExists(thumbnailDir)) {
            qCritical() << "✗ --strict-offline: remus-thumbnails manifest missing after consolidate";
            if (handlePostPromoteFailure(database, buildId, progressPath, timer.elapsed(), ingestPromotedEarly,
                    QStringLiteral("--strict-offline: remus-thumbnails manifest missing after consolidate"))) {
                database.close();
                QSqlDatabase::removeDatabase(connectionName);
                return 1;
            }
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    // ── Enrichment passes (Libretro, GameTDB, OpenVGDB, IGDB) + merge resolve ──
    // Fires before each pass to keep progress.json live during the long enrichment phase.
    // overall_pct model: DAT ingest = 0-10%, enrichment passes = 10-95%, FTS = 95-99%, done = 100%.
    EnrichmentProgressCallback onEnrichProgress = [&](int passIdx, int totalPasses, const QString &passName) {
        const int pct = 10 + (passIdx - 1) * 85 / (totalPasses > 0 ? totalPasses : 1);
        const QJsonObject obj {
            { QStringLiteral("status"), QStringLiteral("enriching") },
            { QStringLiteral("current"), totalEnabled },
            { QStringLiteral("total"), totalEnabled },
            { QStringLiteral("current_source"), QString() },
            { QStringLiteral("enrichment_pass_current"), passIdx },
            { QStringLiteral("enrichment_pass_total"), totalPasses },
            { QStringLiteral("enrichment_pass_name"), passName },
            { QStringLiteral("overall_pct"), pct },
            { QStringLiteral("build_phase"), QStringLiteral("enriching") },
            { QStringLiteral("records_ingested"), stats.recordsIngested },
            { QStringLiteral("games_created"), stats.gamesCreated },
            { QStringLiteral("elapsed_ms"), static_cast<qint64>(timer.elapsed()) },
            { QStringLiteral("started_at"), startedAt.toString(Qt::ISODate) },
            { QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        };
        QFile pf(progressPath);
        if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            pf.write(QJsonDocument(obj).toJson());
    };

    // ── Enrichment passes (Libretro, GameTDB, OpenVGDB, IGDB) + merge resolve ──
    EnrichmentStats enrichStats;
    {
        if (!runCompendiumEnrichmentPasses(database, metadataDir, gametdbDir, openvgdbPath, credPath, mameCatverPath,
                mameListXmlPath, launchboxMetadataPath, enrichStats, error, onEnrichProgress, effectiveSourceFilter,
                offlineOnlyEnrichment, onlineEnrichmentAll, &progressWriter)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            if (ingestPromotedEarly) {
                handleInterruptedEnrichmentAfterPromote(database, buildId, progressPath, timer.elapsed(), enrichStats);
            }
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        if (enrichStats.passesFailedWithError > 0) {
            if (ingestPromotedEarly) {
                handleInterruptedEnrichmentAfterPromote(database, buildId, progressPath, timer.elapsed(), enrichStats);
                database.close();
                QSqlDatabase::removeDatabase(connectionName);
                return 1;
            }
            if (ctx.parser.isSet(QStringLiteral("fail-on-enrichment-errors"))) {
                qCritical().noquote() << QStringLiteral(
                    "✗ %1 enrichment pass(es) failed with errors (--fail-on-enrichment-errors)")
                                             .arg(enrichStats.passesFailedWithError);
                logEnrichmentPassErrors(enrichStats);
                database.close();
                QSqlDatabase::removeDatabase(connectionName);
                return 1;
            }
        }

        persistBuildPhaseNotes(database, buildId, QStringLiteral("enrich_complete"),
            QStringLiteral("Enrichment passes complete; FTS pending"));
    }

    if (ctx.parser.isSet(QStringLiteral("ingest-remote-artwork")) && !offlineOnlyEnrichment) {
        int remoteArtGames = 0;
        QString remoteArtError;
        const QStringList remoteSources {
            QStringLiteral("igdb"),
            QStringLiteral("screenscraper"),
            QStringLiteral("launchbox"),
        };
        for (const QString &sourceId : remoteSources) {
            if (!CompendiumEnrichment::ingestRemoteCoverArtIntoBlobStore(
                    database, findRepoRoot(), findRemusThumbnailsDir(), sourceId, remoteArtGames, remoteArtError)) {
                qWarning().noquote() << QStringLiteral("[ingest-remote-artwork] %1: %2").arg(sourceId, remoteArtError);
            }
        }
        qInfo().noquote() << QStringLiteral("[ingest-remote-artwork] games_updated=%1").arg(remoteArtGames);
    }

    if (shouldRebuildCompendiumFts(enrichStats, ctx.parser.isSet(QStringLiteral("skip-fts")))) {
        QString ftsError;
        if (!populateCompendiumFtsIndex(database, enrichStats.ftsRowsIndexed, ftsError, &progressWriter)) {
            qCritical().noquote() << QStringLiteral("✗ FTS rebuild failed: %1").arg(ftsError);
            if (handlePostPromoteFailure(
                    database, buildId, progressPath, timer.elapsed(), ingestPromotedEarly, ftsError)) {
                database.close();
                QSqlDatabase::removeDatabase(connectionName);
                return 1;
            }
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    } else if (enrichStats.passesExecuted > 0) {
        qInfo() << "[FTS] Skipped — enrichment did not change searchable title fields";
    }

    int systemsCount = scalarCount(database, QStringLiteral("SELECT COUNT(*) FROM systems"), error);
    if (systemsCount < 0) {
        qCritical().noquote() << QStringLiteral("✗ Failed to count systems: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    int conflictsCount = scalarCount(
        database, QStringLiteral("SELECT COUNT(*) FROM merge_conflicts WHERE resolution_status = 'unresolved'"), error);
    if (conflictsCount < 0) {
        qCritical().noquote() << QStringLiteral("✗ Failed to count unresolved conflicts: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    if (!integrityCheckOk(database, error)) {
        qCritical().noquote() << QStringLiteral("✗ Integrity check failed: %1").arg(error);
        if (handlePostPromoteFailure(database, buildId, progressPath, timer.elapsed(), ingestPromotedEarly, error)) {
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    finalizeCompendiumBuildArtifacts(database);

    const QString reportPath = stagedReportPath;
    QJsonObject report;
    report.insert(QStringLiteral("build_id"), buildId);
    report.insert(QStringLiteral("schema_version"), schemaVersion);
    report.insert(
        QStringLiteral("compendium_migration_version"), Constants::DatabaseSchema::Compendium::MIGRATION_VERSION);
    report.insert(QStringLiteral("input_sources"), sourceObjects);
    report.insert(QStringLiteral("records_ingested"), stats.recordsIngested);
    report.insert(QStringLiteral("games_created"), stats.gamesCreated);
    report.insert(QStringLiteral("games_deduplicated"), stats.deduplicatedGames);
    report.insert(QStringLiteral("signatures_created"), stats.signaturesCreated);
    report.insert(QStringLiteral("serials_created"), stats.serialsCreated);
    report.insert(QStringLiteral("disc_sets_created"), stats.discSetsCreated);
    report.insert(QStringLiteral("tracks_created"), stats.tracksCreated);
    report.insert(QStringLiteral("facts_created"), stats.factsCreated);
    report.insert(QStringLiteral("resolved_fields"), stats.resolvedFields);
    report.insert(QStringLiteral("unresolved_conflicts"), conflictsCount);
    insertEnrichmentStatsReportFields(report, enrichStats, QStringLiteral("post_enrich_resolved_fields"));
    report.insert(QStringLiteral("enrichment_inputs_fingerprint"), enrichmentFingerprint);
    report.insert(QStringLiteral("duration_ms"), static_cast<qint64>(timer.elapsed()));
    report.insert(QStringLiteral("build_mode"),
        incrementalIngest ? QStringLiteral("incremental_ingest") : QStringLiteral("full_rebuild"));

    if (!writeReport(reportPath, report, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        if (handlePostPromoteFailure(database, buildId, progressPath, timer.elapsed(), ingestPromotedEarly, error)) {
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
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

    writeProgress(QStringLiteral("complete"), totalEnabled, { }, stats, /*overallPct=*/100, QStringLiteral("complete"));

    {
        const QJsonObject notesObj {
            { QStringLiteral("description"), QStringLiteral("Phase 1 bootstrap compiler run") },
            { QStringLiteral("enrichment_inputs_fingerprint"), enrichmentFingerprint },
            { QStringLiteral("build_phase"), QStringLiteral("complete") },
        };
        QSqlQuery notesQuery(database);
        notesQuery.prepare(QStringLiteral("UPDATE compendium_builds SET notes = ? WHERE build_id = ?"));
        notesQuery.addBindValue(QString::fromUtf8(QJsonDocument(notesObj).toJson(QJsonDocument::Compact)));
        notesQuery.addBindValue(buildId);
        if (!notesQuery.exec()) {
            qWarning() << "[build-compendium] Failed to persist enrichment fingerprint in build notes:"
                       << notesQuery.lastError().text();
        }
    }

    releaseDatabase(database, connectionName);

    if (!ingestPromotedEarly) {
        if (!promoteStagedFile(stagedOutputPath, finalOutputPath, error)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
            writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
            return 1;
        }
    }
    if (!promoteStagedFile(stagedReportPath, finalReportPath, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        writeTerminalProgress(progressPath, QStringLiteral("failed"), timer.elapsed());
        if (!ingestPromotedEarly) {
            // Roll back the DB promotion so the next invocation is forced to rebuild.
            if (!QFile::rename(finalOutputPath, stagedOutputPath)) {
                qCritical() << "[build-compendium] Could not roll back DB promotion —"
                            << "delete" << finalOutputPath << "and rerun to recover.";
            }
        } else {
            qCritical().noquote()
                << QStringLiteral("[build-compendium] Ingest DB preserved at %1").arg(finalOutputPath);
        }
        return 1;
    }

    stagedGuard.release();
    writeTerminalProgress(progressPath, QStringLiteral("complete"), timer.elapsed());
    return conflictsCount > 0 ? 2 : 0;
}

int handleInitCompendiumCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("init-compendium"))) {
        return 0;
    }

    ctx.actionExecuted = true;
    const QString scriptPath = QDir(QCoreApplication::applicationDirPath())
                                   .absoluteFilePath(QStringLiteral("../../scripts/init_compendium.sh"));
    QStringList candidates = {
        QDir::currentPath() + QStringLiteral("/scripts/init_compendium.sh"),
        scriptPath,
    };
    QString initScript;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            initScript = QDir::cleanPath(candidate);
            break;
        }
    }
    if (initScript.isEmpty()) {
        qCritical() << "init_compendium.sh not found — run from repository root";
        return 1;
    }

    QProcess process;
    process.setProgram(QStringLiteral("bash"));
    process.setArguments({ initScript });
    process.setWorkingDirectory(QFileInfo(initScript).absolutePath() + QStringLiteral("/.."));
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start();
    if (!process.waitForStarted()) {
        qCritical() << "Failed to start init_compendium.sh";
        return 1;
    }
    process.waitForFinished(-1);
    return process.exitCode() == 0 ? 0 : 1;
}
