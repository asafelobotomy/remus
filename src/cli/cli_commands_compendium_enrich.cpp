#include "cli_commands.h"
#include "cli_helpers.h"
#include "compendium_enrichment.h"
#include "compendium_sql_utilities.h"

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

    // Check whether GameTDB enrichment already ran
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
    if (existingGameTDB > 0) {
        qInfo() << "GameTDB enrichment already applied — nothing to do.";
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 0;
    }

    qInfo() << "Running GameTDB enrichment on" << outputInfo.absoluteFilePath();

    if (!database.transaction()) {
        qCritical() << "✗ Failed to start GameTDB enrichment transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    QString error;
    int gametdbGamesEnriched = 0;
    int gametdbFactsInserted = 0;
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
    report.insert(QStringLiteral("enrich_compendium_duration_ms"), static_cast<qint64>(timer.elapsed()));

    if (!CompendiumSqlUtilities::writeReport(reportPath, report, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Enrich Compendium ===";
    qInfo() << "Database:" << outputInfo.absoluteFilePath();
    qInfo() << "GameTDB games enriched:" << gametdbGamesEnriched;
    qInfo() << "GameTDB facts inserted:" << gametdbFactsInserted;
    qInfo().nospace() << "Duration: " << timer.elapsed() << " ms";

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
