#include "cli_commands.h"
#include "cli_compendium_build_phases.h"
#include "cli_helpers.h"
#include "compendium_sql_utilities.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>

// ── --enrich-compendium ────────────────────────────────────────────────────────
// Opens an existing compendium DB and runs all enrichment passes that have data
// available, filling only fields that are still missing (COALESCE semantics).
// Passes are idempotent: already-filled fields are never overwritten.
int handleEnrichCompendiumCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("enrich-compendium")) return 0;

    const QString outputPath = ctx.parser.value("compendium-output").trimmed();
    if (outputPath.isEmpty()) {
        qCritical() << "✗ Missing required option: --compendium-output <path>";
        return 1;
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists()) {
        qCritical() << "✗ Database not found (run --build-compendium first):" << outputPath;
        return 1;
    }

    // Locate data directories — absent directories are skipped, not fatal.
    const QString metadataDir  = findMetadataDir();
    const QString gametdbDir   = findGameTDBDir();
    const QString openvgdbPath = findOpenVGDBPath();
    const QString mameCatverPath = findMameCatverPath();
    const QString mameListXmlPath = findMameListXmlPath();
    const QString credPath     = outputInfo.dir().filePath(QStringLiteral("enrichment-credentials.json"));

    if (metadataDir.isEmpty())
        qInfo() << "[enrich] data/metadata/ not found — Libretro metadata pass skipped";
    if (gametdbDir.isEmpty())
        qInfo() << "[enrich] data/gametdb/ not found — GameTDB pass skipped";
    if (openvgdbPath.isEmpty())
        qInfo() << "[enrich] data/openvgdb/openvgdb.sqlite not found — OpenVGDB pass skipped";
    if (!QFile::exists(credPath))
        qInfo() << "[enrich] enrichment-credentials.json not found — IGDB and RA passes skipped";
    if (mameCatverPath.isEmpty())
        qInfo() << "[enrich] data/mame/catver.ini not found — MAME catver pass skipped";
    if (mameListXmlPath.isEmpty())
        qInfo() << "[enrich] data/mame/listxml.xml not found — MAME listxml pass skipped";

    const QString connectionName = QStringLiteral("compendium-enrich-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(outputInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open database:" << database.lastError().text();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    {
        QSqlQuery pragmaQuery(database);
        // WAL mode allows concurrent readers + one writer; prevents SQLITE_LOCKED
        // when the nested QEventLoop in waitForReply() re-enters the event loop.
        pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
        // Retry for up to 5 s on transient lock contention instead of failing immediately.
        pragmaQuery.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
        if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
            qCritical() << "✗ Failed to enable foreign keys:" << pragmaQuery.lastError().text();
            database.close();
            database = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    qInfo() << "Running enrichment on" << outputInfo.absoluteFilePath();

    // ── Run all enrichment passes and merge resolution ───────────────────────
    const QString sourceFilterArg = ctx.parser.value("enrich-source").trimmed();
    const QStringList sourceFilter = sourceFilterArg.isEmpty()
        ? QStringList{}
        : sourceFilterArg.split(QLatin1Char(','), Qt::SkipEmptyParts);
    EnrichmentStats stats;
    {
        QString enrichError;
        if (!runCompendiumEnrichmentPasses(database,
                                          metadataDir,
                                          gametdbDir,
                                          openvgdbPath,
                                          credPath,
                                          mameCatverPath,
                                          mameListXmlPath,
                                          stats,
                                          enrichError,
                                          nullptr,
                                          sourceFilter)) {
            qCritical().noquote() << QStringLiteral("✗ %1").arg(enrichError);
            database.close();
            database = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    // Rebuild FTS index to pick up new descriptions.
    {
        QString ftsError;
        if (!populateCompendiumFtsIndex(database, stats.ftsRowsIndexed, ftsError)) {
            qCritical().noquote() << QStringLiteral("✗ FTS rebuild failed: %1").arg(ftsError);
            database.close();
            database = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    // Update (or create) the report JSON alongside the DB.
    const QString reportPath = CompendiumSqlUtilities::reportPathForDatabase(outputInfo.absoluteFilePath());
    QJsonObject report;

    // Preserve existing report fields if the file exists.
    QFile existingReport(reportPath);
    if (existingReport.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray existing = existingReport.readAll();
        existingReport.close();
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(existing, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject())
            report = doc.object();
    }

    insertEnrichmentStatsReportFields(report, stats, QStringLiteral("resolved_fields"));
    report.insert(QStringLiteral("enrich_compendium_duration_ms"), static_cast<qint64>(timer.elapsed()));

    QString reportError;
    if (!CompendiumSqlUtilities::writeReport(reportPath, report, reportError)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(reportError);
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Enrich Compendium ===";
    qInfo() << "Database:" << outputInfo.absoluteFilePath();
    if (stats.metadataGamesEnriched > 0 || stats.metadataFactsInserted > 0) {
        qInfo() << "Libretro metadata games enriched:" << stats.metadataGamesEnriched;
        qInfo() << "Libretro metadata facts inserted:" << stats.metadataFactsInserted;
    }
    if (stats.gametdbGamesEnriched > 0 || stats.gametdbFactsInserted > 0) {
        qInfo() << "GameTDB games enriched:" << stats.gametdbGamesEnriched;
        qInfo() << "GameTDB facts inserted:" << stats.gametdbFactsInserted;
    }
    if (stats.openvgdbGamesEnriched > 0 || stats.openvgdbFactsInserted > 0) {
        qInfo() << "OpenVGDB games enriched:" << stats.openvgdbGamesEnriched;
        qInfo() << "OpenVGDB facts inserted:" << stats.openvgdbFactsInserted;
    }
    if (stats.igdbGamesEnriched > 0 || stats.igdbFactsInserted > 0) {
        qInfo() << "IGDB games enriched:" << stats.igdbGamesEnriched;
        qInfo() << "IGDB facts inserted:" << stats.igdbFactsInserted;
    }
    if (stats.raGamesEnriched > 0 || stats.raFactsInserted > 0) {
        qInfo() << "RA games enriched:" << stats.raGamesEnriched;
        qInfo() << "RA facts inserted:" << stats.raFactsInserted;
    }
    if (stats.mameGamesEnriched > 0 || stats.mameFactsInserted > 0) {
        qInfo() << "MAME games enriched:" << stats.mameGamesEnriched;
        qInfo() << "MAME facts inserted:" << stats.mameFactsInserted;
    }
    if (stats.zxinfoGamesEnriched > 0 || stats.zxinfoFactsInserted > 0) {
        qInfo() << "ZXInfo games enriched:" << stats.zxinfoGamesEnriched;
        qInfo() << "ZXInfo facts inserted:" << stats.zxinfoFactsInserted;
    }
    if (stats.resolvedFields > 0) {
        qInfo() << "Resolved fields:" << stats.resolvedFields;
    }
    qInfo().nospace() << "Duration: " << timer.elapsed() << " ms";

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
