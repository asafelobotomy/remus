#include "cli_commands.h"
#include "cli_helpers.h"
#include "compendium_consolidate_thumbnails.h"
#include "compendium_enrichment.h"

#include <QTextStream>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUuid>

namespace {

ConsolidateThumbnailsOptions optionsFromParser(const QCommandLineParser &parser) {
    ConsolidateThumbnailsOptions opts;
    opts.acquisitionDir = parser.value(QStringLiteral("acquisition-dir")).trimmed();
    if (opts.acquisitionDir.isEmpty()) {
        opts.acquisitionDir = findLibretroAcquisitionDir();
    }
    opts.outputDir = parser.value(QStringLiteral("thumbnail-output-dir")).trimmed();
    if (opts.outputDir.isEmpty()) {
        opts.outputDir = findRemusThumbnailsDir();
    }
    const QString systemsArg = parser.value(QStringLiteral("thumbnail-system")).trimmed();
    if (!systemsArg.isEmpty()) {
        opts.systems = systemsArg.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }
    opts.format = parser.value(QStringLiteral("thumbnail-format")).trimmed();
    if (opts.format.isEmpty()) {
        opts.format = QStringLiteral("webp");
    }
    opts.snapQuality = parser.value(QStringLiteral("thumbnail-snap-quality")).toInt();
    if (opts.snapQuality <= 0) {
        opts.snapQuality = 85;
    }
    opts.snapLossless = parser.isSet(QStringLiteral("thumbnail-snap-lossless"));
    opts.dryRun = parser.isSet(QStringLiteral("thumbnail-dry-run"));
    opts.pruneAcquisitionSources = parser.isSet(QStringLiteral("prune-acquisition-sources"));
    return opts;
}

QString resolveDatabasePath(const QCommandLineParser &parser) {
    QString path = parser.value(QStringLiteral("compendium-output")).trimmed();
    if (path.isEmpty()) {
        const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
        if (!compendiumDir.isEmpty()) {
            path = compendiumDir + QStringLiteral("/remus_compendium.db");
        }
    }
    return path;
}

int openCompendiumDatabase(const QString &dbPath, QSqlDatabase &database, QString &connectionName) {
    if (!QFileInfo::exists(dbPath)) {
        qCritical() << "✗ Database not found:" << dbPath;
        return 1;
    }
    connectionName = QStringLiteral("compendium-thumbnails-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(dbPath);
    if (!database.open()) {
        qCritical() << "✗ Failed to open database:" << database.lastError().text();
        return 1;
    }
    return 0;
}

} // namespace

int handleConsolidateThumbnailsCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("consolidate-thumbnails"))) {
        return 0;
    }

    const QString dbPath = resolveDatabasePath(ctx.parser);
    QString connectionName;
    QSqlDatabase database;
    if (openCompendiumDatabase(dbPath, database, connectionName) != 0) {
        return 1;
    }

    ConsolidateThumbnailsOptions opts = optionsFromParser(ctx.parser);
    ConsolidateThumbnailsStats stats;
    QString error;
    if (!CompendiumThumbnails::consolidateThumbnails(database, opts, stats, error)) {
        QTextStream(stderr) << "consolidate-thumbnails failed: " << error << '\n';
        qCritical().noquote() << QStringLiteral("✗ consolidate-thumbnails failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("[consolidate-thumbnails] games=%1 written=%2 dedup=%3 skipped=%4 misses=%5 "
                                        "bytes=%6")
                             .arg(stats.gamesScanned)
                             .arg(stats.assetsWritten)
                             .arg(stats.assetsDeduplicated)
                             .arg(stats.assetsSkipped)
                             .arg(stats.misses)
                             .arg(stats.bytesTotal);

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}

int handleGcThumbnailsCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("gc-thumbnails"))) {
        return 0;
    }

    const QString dbPath = resolveDatabasePath(ctx.parser);
    QString connectionName;
    QSqlDatabase database;
    if (openCompendiumDatabase(dbPath, database, connectionName) != 0) {
        return 1;
    }

    const QString thumbnailRoot = findRemusThumbnailsDir();
    const QString repoRoot = findRepoRoot();
    const bool dryRun = ctx.parser.isSet(QStringLiteral("thumbnail-dry-run"));
    int orphans = 0;
    QString error;
    if (!CompendiumThumbnails::gcThumbnailBlobs(database, thumbnailRoot, repoRoot, dryRun, orphans, error)) {
        qCritical().noquote() << QStringLiteral("✗ gc-thumbnails failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("[gc-thumbnails] orphans=%1 dry_run=%2").arg(orphans).arg(dryRun);
    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}

int handleExportRetroArchArtworkCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("export-retroarch-artwork"))) {
        return 0;
    }

    const QString dbPath = resolveDatabasePath(ctx.parser);
    QString connectionName;
    QSqlDatabase database;
    if (openCompendiumDatabase(dbPath, database, connectionName) != 0) {
        return 1;
    }

    QString exportDir = ctx.parser.value(QStringLiteral("retroarch-artwork-dir")).trimmed();
    if (exportDir.isEmpty()) {
        exportDir = findDataSubdir(QStringLiteral("export")) + QStringLiteral("/retroarch-thumbnails");
    }
    QDir().mkpath(exportDir);

    QStringList systems;
    const QString systemsArg = ctx.parser.value(QStringLiteral("thumbnail-system")).trimmed();
    if (!systemsArg.isEmpty()) {
        systems = systemsArg.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }

    int filesExported = 0;
    QString error;
    if (!CompendiumThumbnails::exportRetroArchArtwork(
            database, findRemusThumbnailsDir(), findRepoRoot(), exportDir, systems, filesExported, error)) {
        qCritical().noquote() << QStringLiteral("✗ export-retroarch-artwork failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("[export-retroarch-artwork] files=%1 dir=%2").arg(filesExported).arg(exportDir);
    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}

int handleIngestRemoteArtworkCommand(CliContext &ctx) {
    if (!ctx.parser.isSet(QStringLiteral("ingest-remote-artwork"))) {
        return 0;
    }

    const QString dbPath = resolveDatabasePath(ctx.parser);
    QString connectionName;
    QSqlDatabase database;
    if (openCompendiumDatabase(dbPath, database, connectionName) != 0) {
        return 1;
    }

    QString sourceId = ctx.parser.value(QStringLiteral("artwork-source-id")).trimmed();
    if (sourceId.isEmpty()) {
        sourceId = QStringLiteral("igdb");
    }

    int gamesEnriched = 0;
    QString error;
    if (!CompendiumEnrichment::ingestRemoteCoverArtIntoBlobStore(
            database, findRepoRoot(), findRemusThumbnailsDir(), sourceId, gamesEnriched, error)) {
        qCritical().noquote() << QStringLiteral("✗ ingest-remote-artwork failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("[ingest-remote-artwork] source=%1 games=%2").arg(sourceId).arg(gamesEnriched);
    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
