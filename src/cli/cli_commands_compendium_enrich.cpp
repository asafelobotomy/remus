#include "cli_commands.h"
#include "cli_compendium_build_phases.h"
#include "cli_helpers.h"
#include "compendium_enrichment.h"
#include "compendium_sql_utilities.h"
#include "../metadata/compendium_merge_resolver.h"
#include "../metadata/compendium_types.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

// ── --enrich-compendium ────────────────────────────────────────────────────────
// Opens an existing compendium DB and runs any missing enrichment passes
// (currently GameTDB) without touching the previously ingested DAT data.
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

    const QString gametdbDir = findDataSubdir(QStringLiteral("gametdb"));
    if (gametdbDir.isEmpty()) {
        qCritical() << "✗ Could not locate data/gametdb directory";
        return 1;
    }

    const QString connectionName = QStringLiteral("compendium-enrich-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(outputInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open database:" << database.lastError().text();
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

    // Check which enrichments have already been applied
    QString checkError;
    const int existingGameTDB = CompendiumSqlUtilities::scalarCount(database,
        QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = 'gametdb'"),
        checkError);
    if (existingGameTDB < 0) {
        qCritical().noquote() << "✗ Could not query sources table:" << checkError;
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    const int existingIGDB = CompendiumSqlUtilities::scalarCount(database,
        QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = 'igdb'"),
        checkError);
    if (existingIGDB < 0) {
        qCritical().noquote() << "✗ Could not query sources table:" << checkError;
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    const QString credPath = outputInfo.dir().filePath(QStringLiteral("enrichment-credentials.json"));
    const bool needsGameTDB = (existingGameTDB == 0);
    const bool needsIGDB    = (existingIGDB == 0) && QFile::exists(credPath);

    if (!needsGameTDB && !needsIGDB) {
        qInfo() << "All enrichments already applied — nothing to do.";
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 0;
    }

    qInfo() << "Running enrichment on" << outputInfo.absoluteFilePath();

    // ── Enrichment pass: GameTDB ──────────────────────────────────────────────
    int gametdbGamesEnriched = 0, gametdbFactsInserted = 0;
    if (needsGameTDB) {
        if (!database.transaction()) {
            qCritical() << "✗ Failed to start GameTDB enrichment transaction:" << database.lastError().text();
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
        QString error;
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

    // ── Enrichment pass: IGDB (manages its own per-system transactions) ───────
    int igdbGamesEnriched = 0, igdbFactsInserted = 0;
    if (needsIGDB) {
        QString error;
        if (!CompendiumEnrichment::enrichFromIGDB(database, credPath,
                                                  igdbGamesEnriched, igdbFactsInserted, error)) {
            qCritical().noquote() << QStringLiteral("✗ IGDB enrichment failed: %1").arg(error);
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 1;
        }
    }

    // Re-run merge resolution to pick up facts written by enrichment passes.
    {
        Remus::Compendium::CompilerStats resolveStats;
        QString resolveError;
        const Remus::Compendium::MergeResolver resolver;
        if (!resolver.resolve(database, resolveStats, resolveError)) {
            qWarning() << "[enrich-compendium] Merge resolution failed (non-fatal):" << resolveError;
        } else if (resolveStats.resolvedFields > 0) {
            qInfo().noquote() << QStringLiteral("[enrich-compendium] Merge resolved %1 fields.")
                                     .arg(resolveStats.resolvedFields);
        }
    }

    // Rebuild FTS index to pick up new descriptions
    populateCompendiumFtsIndex(database);

    // Update (or create) the report JSON alongside the DB
    const QString reportPath = CompendiumSqlUtilities::reportPathForDatabase(outputInfo.absoluteFilePath());
    QJsonObject report;

    // Preserve existing report fields if the file exists
    QFile existingReport(reportPath);
    if (existingReport.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray existing = existingReport.readAll();
        existingReport.close();
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(existing, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject())
            report = doc.object();
    }

    report.insert(QStringLiteral("gametdb_games_enriched"), gametdbGamesEnriched);
    report.insert(QStringLiteral("gametdb_facts_inserted"), gametdbFactsInserted);
    report.insert(QStringLiteral("igdb_games_enriched"), igdbGamesEnriched);
    report.insert(QStringLiteral("igdb_facts_inserted"), igdbFactsInserted);
    report.insert(QStringLiteral("enrich_compendium_duration_ms"), static_cast<qint64>(timer.elapsed()));

    QString reportError;
    if (!CompendiumSqlUtilities::writeReport(reportPath, report, reportError)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(reportError);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Enrich Compendium ===";
    qInfo() << "Database:" << outputInfo.absoluteFilePath();
    qInfo() << "GameTDB games enriched:" << gametdbGamesEnriched;
    qInfo() << "GameTDB facts inserted:" << gametdbFactsInserted;
    qInfo() << "IGDB games enriched:" << igdbGamesEnriched;
    qInfo() << "IGDB facts inserted:" << igdbFactsInserted;
    qInfo().nospace() << "Duration: " << timer.elapsed() << " ms";

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
