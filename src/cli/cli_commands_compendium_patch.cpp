#include "cli_commands.h"
#include "cli_helpers.h"
#include "compendium_sql_utilities.h"

#include "../metadata/compendium_patch_catalog_importer.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

int handleImportPatchCatalogCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("import-patch-catalog"))
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

    QString patchDir = ctx.parser.value("patch-dir").trimmed();
    if (patchDir.isEmpty()) {
        patchDir = findDataSubdir(QStringLiteral("patches"));
        if (patchDir.isEmpty())
            patchDir = QDir(outputInfo.dir()).filePath(QStringLiteral("patches"));
    }

    const QString connectionName
        = QStringLiteral("compendium-patch-import-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(outputInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open database:" << database.lastError().text();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    CompendiumSqlUtilities::applyCompendiumWritePragmas(database);

    Remus::CompendiumPatchCatalog::ImportStats stats;
    QString importError;
    if (!Remus::CompendiumPatchCatalog::importDirectory(database, patchDir, stats, importError)) {
        qCritical().noquote() << QStringLiteral("✗ Patch catalog import failed: %1").arg(importError);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    database.close();
    QSqlDatabase::removeDatabase(connectionName);

    qInfo().noquote() << QStringLiteral(
        "✓ Patch catalog import complete in %1 ms — %2 source(s), %3 entries, %4 skipped")
                             .arg(timer.elapsed())
                             .arg(stats.sourcesImported)
                             .arg(stats.entriesImported)
                             .arg(stats.filesSkipped);
    return 0;
}
