#include "cli_commands.h"
#include "cli_helpers.h"

#include "../metadata/compendium_compiler_service.h"
#include "../metadata/compendium_merge_resolver.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

int handleDedupCompendiumCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("dedup-compendium"))
        return 0;

    const QString outputPath = ctx.parser.value("compendium-output").trimmed();
    if (outputPath.isEmpty()) {
        qCritical() << "✗ Missing required option: --compendium-output <path>";
        return 1;
    }

    const QFileInfo outputInfo(outputPath);
    if (!outputInfo.exists()) {
        qCritical() << "✗ Database not found:" << outputPath;
        return 1;
    }

    const QString connectionName
        = QStringLiteral("compendium-dedup-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(outputInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open database:" << database.lastError().text();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    {
        QSqlQuery pragmaQuery(database);
        pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
        pragmaQuery.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
        if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
            qCritical() << "✗ Failed to enable foreign keys:" << pragmaQuery.lastError().text();
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

    QString dedupError;
    const int merged = Remus::Compendium::deduplicateGames(database, dedupError);
    if (merged < 0) {
        database.rollback();
        qCritical().noquote() << QStringLiteral("✗ Dedup failed: %1").arg(dedupError);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    Remus::Compendium::CompilerStats stats;
    const Remus::Compendium::MergeResolver resolver;
    QString resolveError;
    if (!resolver.resolve(database, stats, resolveError)) {
        database.rollback();
        qCritical().noquote() << QStringLiteral("✗ Merge resolution failed: %1").arg(resolveError);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    if (!database.commit()) {
        qCritical() << "✗ Failed to commit dedup transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo() << "=== Dedup Compendium ===";
    qInfo() << "Database:" << outputInfo.absoluteFilePath();
    qInfo() << "Merged game rows:" << merged;
    qInfo().nospace() << "Duration: " << timer.elapsed() << " ms";

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
