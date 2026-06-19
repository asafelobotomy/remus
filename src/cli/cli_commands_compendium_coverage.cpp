#include "cli_commands.h"
#include "cli_helpers.h"

#include "../core/compendium_disc_bridge.h"

#include <QDebug>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QUuid>

namespace {

struct CompendiumCoverageDb {
    QString connectionName;
    QSqlDatabase database;

    bool open(const QString &outputPath, QString &error) {
        const QFileInfo dbInfo(outputPath);
        if (!dbInfo.exists()) {
            error = QStringLiteral("Compendium database not found: %1").arg(outputPath);
            return false;
        }

        connectionName = QStringLiteral("coverage-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(dbInfo.absoluteFilePath());
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!database.open()) {
            error = database.lastError().text();
            database = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
            connectionName.clear();
            return false;
        }
        return true;
    }

    void close() {
        if (connectionName.isEmpty())
            return;
        if (database.isOpen())
            database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        connectionName.clear();
    }

    qint64 scalar(QSqlQuery &q, const QString &sql) const {
        if (!q.exec(sql) || !q.next())
            return -1;
        return q.value(0).toLongLong();
    }
};

struct DiscSetCoverageStats {
    qint64 discBasedGames = 0;
    qint64 gamesWithDiscSets = 0;
    double coveragePct = 0.0;
};

bool queryDiscSetCoverageStats(QSqlDatabase &database, DiscSetCoverageStats &stats, QString &error) {
    stats = { };
    if (!compendiumDiscSetsAvailable(database))
        return true;

    QSqlQuery q(database);
    stats.discBasedGames = q.exec(QStringLiteral("SELECT COUNT(*) FROM games g "
                                                 "JOIN systems s ON s.system_id = g.system_id "
                                                 "WHERE s.is_disc_based = 1"))
            && q.next()
        ? q.value(0).toLongLong()
        : -1;
    if (stats.discBasedGames < 0) {
        error = q.lastError().text();
        return false;
    }

    stats.gamesWithDiscSets = q.exec(QStringLiteral("SELECT COUNT(DISTINCT gds.game_id) "
                                                    "FROM game_disc_sets gds "
                                                    "JOIN games g ON g.game_id = gds.game_id "
                                                    "JOIN systems s ON s.system_id = g.system_id "
                                                    "WHERE s.is_disc_based = 1"))
            && q.next()
        ? q.value(0).toLongLong()
        : -1;
    if (stats.gamesWithDiscSets < 0) {
        error = q.lastError().text();
        return false;
    }

    if (stats.discBasedGames > 0)
        stats.coveragePct
            = 100.0 * static_cast<double>(stats.gamesWithDiscSets) / static_cast<double>(stats.discBasedGames);
    return true;
}

} // namespace

int handleDiscSetCoverageCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("disc-set-coverage")))
        return 0;

    const QString outputPath = ctx.parser.value(QStringLiteral("compendium-output")).trimmed();
    if (outputPath.isEmpty()) {
        qCritical() << "✗ --disc-set-coverage requires --compendium-output <db>";
        return 1;
    }

    CompendiumCoverageDb db;
    QString error;
    if (!db.open(outputPath, error)) {
        qCritical() << "✗" << error;
        return 1;
    }

    DiscSetCoverageStats stats;
    if (!queryDiscSetCoverageStats(db.database, stats, error)) {
        qCritical() << "✗ Failed to query disc set coverage:" << error;
        db.close();
        return 1;
    }

    QTextStream out(stdout);
    out << QStringLiteral("# disc_based_games=%1 games_with_disc_sets=%2 disc_set_coverage_pct=%3\n")
               .arg(stats.discBasedGames)
               .arg(stats.gamesWithDiscSets)
               .arg(QString::number(stats.coveragePct, 'f', 1));
    out << QStringLiteral("system_id\tsystem_name\tdisc_based_games\tgames_with_disc_sets\tcoverage_pct\n");

    QSqlQuery q(db.database);
    if (q.exec(QStringLiteral("SELECT s.system_id, s.display_name, "
                              "       COUNT(DISTINCT g.game_id) AS disc_based_games, "
                              "       COUNT(DISTINCT CASE WHEN gds.game_id IS NOT NULL THEN g.game_id END) "
                              "           AS games_with_disc_sets "
                              "FROM systems s "
                              "LEFT JOIN games g ON g.system_id = s.system_id "
                              "LEFT JOIN game_disc_sets gds ON gds.game_id = g.game_id "
                              "WHERE s.is_disc_based = 1 "
                              "GROUP BY s.system_id, s.display_name "
                              "ORDER BY s.display_name"))) {
        while (q.next()) {
            const qint64 discGames = q.value(2).toLongLong();
            const qint64 withSets = q.value(3).toLongLong();
            const double pct
                = discGames > 0 ? 100.0 * static_cast<double>(withSets) / static_cast<double>(discGames) : 0.0;
            out << q.value(0).toInt() << '\t' << q.value(1).toString() << '\t' << discGames << '\t' << withSets << '\t'
                << QString::number(pct, 'f', 1) << '\n';
        }
    }
    out.flush();

    db.close();
    return 0;
}

int handleCoverageReportCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("coverage-report")))
        return 0;

    const QString outputPath = ctx.parser.value(QStringLiteral("compendium-output")).trimmed();
    if (outputPath.isEmpty()) {
        qCritical() << "✗ --coverage-report requires --compendium-output <db>";
        return 1;
    }

    CompendiumCoverageDb db;
    QString error;
    if (!db.open(outputPath, error)) {
        qCritical() << "✗" << error;
        return 1;
    }

    {
        QSqlQuery q(db.database);
        const auto scalar = [&](const QString &sql) -> qint64 { return db.scalar(q, sql); };

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

        DiscSetCoverageStats discSetStats;
        if (!queryDiscSetCoverageStats(db.database, discSetStats, error)) {
            qCritical() << "✗ Failed to query disc set coverage:" << error;
            db.close();
            return 1;
        }

        if (totalGames < 0) {
            qCritical() << "✗ Failed to query database:" << q.lastError().text();
            db.close();
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
            db.close();
            return 1;
        }

        QTextStream out(stdout);
        out << QStringLiteral("# games=%1 signatures=%2 systems=%3 active_sources=%4 shadowed_sources=%5 "
                              "disc_based_games=%6 games_with_disc_sets=%7 disc_set_coverage_pct=%8\n")
                   .arg(totalGames)
                   .arg(totalSignatures)
                   .arg(totalSystems)
                   .arg(totalSources)
                   .arg(shadowedSources)
                   .arg(discSetStats.discBasedGames)
                   .arg(discSetStats.gamesWithDiscSets)
                   .arg(QString::number(discSetStats.coveragePct, 'f', 1));
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

    db.close();
    return 0;
}
