#include "cli_commands.h"
#include "cli_helpers.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QUuid>

// ── --coverage-report ──────────────────────────────────────────────────────────
// Queries an existing compendium database and emits a per-source signature-yield
// report as TSV to stdout.  A machine-readable summary row at the top lists
// total game, signature, and system counts.
int handleCoverageReportCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("coverage-report")))
        return 0;

    const QString outputPath = ctx.parser.value(QStringLiteral("compendium-output")).trimmed();
    if (outputPath.isEmpty()) {
        qCritical() << "✗ --coverage-report requires --compendium-output <db>";
        return 1;
    }

    const QFileInfo dbInfo(outputPath);
    if (!dbInfo.exists()) {
        qCritical() << "✗ Compendium database not found:" << outputPath;
        return 1;
    }

    const QString connectionName = QStringLiteral("coverage-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(dbInfo.absoluteFilePath());
    database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    if (!database.open()) {
        qCritical() << "✗ Failed to open database:" << database.lastError().text();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    const auto cleanup = [&]() {
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    };

    {
        QSqlQuery q(database);
        const auto scalar = [&](const QString &sql) -> qint64 {
            if (!q.exec(sql) || !q.next())
                return -1;
            return q.value(0).toLongLong();
        };

        const qint64 totalGames = scalar(QStringLiteral("SELECT COUNT(*) FROM games"));
        const qint64 totalSignatures = scalar(QStringLiteral("SELECT COUNT(*) FROM game_signatures"));
        const qint64 totalSystems = scalar(QStringLiteral("SELECT COUNT(*) FROM systems"));
        const qint64 totalSources = scalar(QStringLiteral("SELECT COUNT(*) FROM sources WHERE enabled = 1"));
        const qint64 shadowedSources = scalar(QStringLiteral(
            "SELECT COUNT(*) FROM ("
            "  SELECT si.source_id FROM source_items si "
            "  JOIN sources s ON s.source_id = si.source_id AND s.enabled = 1 "
            "  GROUP BY si.source_id "
            "  HAVING COUNT(*) > 100 "
            "    AND COALESCE((SELECT COUNT(*) FROM game_signatures gs WHERE gs.source_id = si.source_id), 0) = 0"
            ")"));

        if (totalGames < 0) {
            qCritical() << "✗ Failed to query database:" << q.lastError().text();
            cleanup();
            return 1;
        }

        const bool ok = q.exec(QStringLiteral(
            "WITH "
            "si AS ( "
            "  SELECT source_id, COUNT(*) AS source_items "
            "  FROM source_items GROUP BY source_id "
            "), "
            "gs_owned AS ( "
            "  SELECT source_id, COUNT(*) AS sigs_owned "
            "  FROM game_signatures GROUP BY source_id "
            "), "
            "games_with_sig AS ( "
            "  SELECT DISTINCT game_id FROM game_signatures "
            "), "
            "gf_covered AS ( "
            "  SELECT gf.source_id, COUNT(DISTINCT gf.game_id) AS games_covered "
            "  FROM game_facts gf "
            "  INNER JOIN games_with_sig gws ON gws.game_id = gf.game_id "
            "  GROUP BY gf.source_id "
            ") "
            "SELECT si.source_id, "
            "       COALESCE(s.enabled, 1) AS enabled, "
            "       COALESCE(s.priority, 0) AS priority, "
            "       si.source_items, "
            "       COALESCE(gs_owned.sigs_owned, 0) AS sigs_owned, "
            "       COALESCE(gf_covered.games_covered, 0) AS games_covered, "
            "       ROUND(COALESCE(gf_covered.games_covered, 0) * 100.0 / si.source_items, 1) AS coverage_pct, "
            "       ROUND(COALESCE(gs_owned.sigs_owned, 0) * 100.0 / si.source_items, 1) AS sig_yield_pct, "
            "       CASE WHEN si.source_items > 100 AND COALESCE(gs_owned.sigs_owned, 0) = 0 THEN 1 ELSE 0 END "
            "           AS shadowed "
            "FROM si "
            "LEFT JOIN sources s ON s.source_id = si.source_id "
            "LEFT JOIN gs_owned   ON gs_owned.source_id   = si.source_id "
            "LEFT JOIN gf_covered ON gf_covered.source_id = si.source_id "
            "WHERE COALESCE(s.enabled, 1) = 1 "
            "ORDER BY shadowed DESC, sig_yield_pct ASC, coverage_pct ASC, si.source_items DESC"));
        if (!ok) {
            qCritical() << "✗ Failed to query source coverage:" << q.lastError().text();
            cleanup();
            return 1;
        }

        QTextStream out(stdout);
        out << QStringLiteral("# games=%1 signatures=%2 systems=%3 active_sources=%4 shadowed_sources=%5\n")
                   .arg(totalGames)
                   .arg(totalSignatures)
                   .arg(totalSystems)
                   .arg(totalSources)
                   .arg(shadowedSources);
        out << QStringLiteral(
            "source_id\tenabled\tpriority\tsource_items\tsigs_owned\tgames_covered\tcoverage_pct\tsig_yield_pct\t"
            "shadowed\n");
        while (q.next()) {
            out << q.value(0).toString() << '\t' << q.value(1).toInt() << '\t' << q.value(2).toInt() << '\t'
                << q.value(3).toLongLong() << '\t' << q.value(4).toLongLong() << '\t' << q.value(5).toLongLong() << '\t'
                << q.value(6).toString() << '\t' << q.value(7).toString() << '\t' << q.value(8).toInt() << '\n';
        }
        out.flush();
    }

    cleanup();
    return 0;
}
