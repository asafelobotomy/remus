#include "cli_commands.h"
#include "compendium_sql_utilities.h"

#include "../metadata/compendium_disc_set_backfill.h"

#include <QDebug>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

using namespace CompendiumSqlUtilities;
using namespace Remus;

int handleBackfillDiscSetsCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("backfill-disc-sets")))
        return 0;

    const QString outputPath = ctx.parser.value(QStringLiteral("compendium-output")).trimmed();
    if (outputPath.isEmpty()) {
        qCritical() << "✗ Missing required option: --compendium-output <path>";
        return 1;
    }

    const QFileInfo dbInfo(outputPath);
    if (!dbInfo.exists()) {
        qCritical() << "✗ Compendium database not found:" << outputPath;
        return 1;
    }

    const bool force = ctx.parser.isSet(QStringLiteral("force-disc-set-backfill"));

    const QString connectionName = QStringLiteral("compendium_backfill_disc_sets");
    if (QSqlDatabase::contains(connectionName))
        QSqlDatabase::removeDatabase(connectionName);

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(dbInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open compendium database:" << database.lastError().text();
        return 1;
    }

    if (!force) {
        QSqlQuery existing(database);
        if (existing.exec(QStringLiteral("SELECT COUNT(*) FROM game_disc_sets")) && existing.next()
            && existing.value(0).toLongLong() > 0) {
            qInfo() << "[backfill-disc-sets] Disc topology already present — use --force-disc-set-backfill to rebuild.";
            database.close();
            QSqlDatabase::removeDatabase(connectionName);
            return 0;
        }
    }

    qInfo() << "[backfill-disc-sets] Rebuilding disc topology from source_items...";
    Compendium::CompilerStats stats;
    QString error;
    if (!Compendium::DiscSetBackfill::backfillDiscSets(database, force, stats, error)) {
        qCritical() << "✗ Disc set backfill failed:" << error;
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("[backfill-disc-sets] ✔ disc_sets=%1 tracks=%2")
                             .arg(stats.discSetsCreated)
                             .arg(stats.tracksCreated);

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
