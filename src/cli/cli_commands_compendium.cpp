#include "cli_commands.h"
#include "cli_helpers.h"

#include "../metadata/compendium_compiler_service.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {

struct CompendiumSourceDescriptor {
    QString sourceId;
    QString displayName;
    QString sourceType;
    QString snapshotId;
    QString snapshotLabel;
    QString snapshotRef;
    QString path;
    QString checksumSha256;
    QString licenseId;
    QString licenseUrl;
    QString fetchedAt;
    int priority = 0;
    bool enabled = false;
    bool attributionRequired = false;
};

QString reportPathForDatabase(const QString &databasePath)
{
    QFileInfo info(databasePath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".report.json"));
}

bool readTextFile(const QString &path, QString &content, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Failed to open %1: %2").arg(path, file.errorString());
        return false;
    }

    content = QString::fromUtf8(file.readAll());
    return true;
}

bool executeSqlScript(QSqlDatabase &database, const QString &path, QString &error)
{
    QString content;
    if (!readTextFile(path, content, error)) {
        return false;
    }

    const QStringList statements = content.split(';');
    for (const QString &rawStatement : statements) {
        const QString statement = rawStatement.trimmed();
        if (statement.isEmpty()) {
            continue;
        }

        QSqlQuery query(database);
        if (!query.exec(statement)) {
            error = QStringLiteral("Failed to execute %1: %2")
                .arg(path, query.lastError().text());
            return false;
        }
    }

    return true;
}

bool requireString(const QJsonObject &object,
                   const QString &fieldName,
                   QString &value,
                   QString &error,
                   bool allowEmpty = false)
{
    if (!object.contains(fieldName) || !object.value(fieldName).isString()) {
        error = QStringLiteral("Manifest field '%1' must be a string").arg(fieldName);
        return false;
    }

    value = object.value(fieldName).toString().trimmed();
    if (!allowEmpty && value.isEmpty()) {
        error = QStringLiteral("Manifest field '%1' must not be empty").arg(fieldName);
        return false;
    }

    return true;
}

bool parseSourceDescriptor(const QJsonObject &object,
                           CompendiumSourceDescriptor &descriptor,
                           QString &error)
{
    if (!requireString(object, QStringLiteral("source_id"), descriptor.sourceId, error)) {
        return false;
    }
    if (!requireString(object, QStringLiteral("source_type"), descriptor.sourceType, error)) {
        return false;
    }
    if (!requireString(object, QStringLiteral("snapshot_id"), descriptor.snapshotId, error)) {
        return false;
    }
    if (!requireString(object, QStringLiteral("snapshot_label"), descriptor.snapshotLabel, error)) {
        return false;
    }
    if (!requireString(object, QStringLiteral("path"), descriptor.path, error)) {
        return false;
    }

    descriptor.displayName = object.value(QStringLiteral("display_name")).toString().trimmed();
    if (descriptor.displayName.isEmpty()) {
        descriptor.displayName = descriptor.sourceId;
    }

    descriptor.snapshotRef = object.value(QStringLiteral("snapshot_ref")).toString().trimmed();
    descriptor.checksumSha256 = object.value(QStringLiteral("checksum_sha256")).toString().trimmed();
    descriptor.licenseId = object.value(QStringLiteral("license_id")).toString().trimmed();
    descriptor.licenseUrl = object.value(QStringLiteral("license_url")).toString().trimmed();
    descriptor.fetchedAt = object.value(QStringLiteral("fetched_at")).toString().trimmed();

    if (!object.contains(QStringLiteral("enabled")) || !object.value(QStringLiteral("enabled")).isBool()) {
        error = QStringLiteral("Source '%1' field 'enabled' must be a boolean").arg(descriptor.sourceId);
        return false;
    }
    descriptor.enabled = object.value(QStringLiteral("enabled")).toBool();

    if (!object.contains(QStringLiteral("priority")) || !object.value(QStringLiteral("priority")).isDouble()) {
        error = QStringLiteral("Source '%1' field 'priority' must be an integer").arg(descriptor.sourceId);
        return false;
    }
    descriptor.priority = object.value(QStringLiteral("priority")).toInt();

    if (object.contains(QStringLiteral("attribution_required"))) {
        if (!object.value(QStringLiteral("attribution_required")).isBool()) {
            error = QStringLiteral("Source '%1' field 'attribution_required' must be a boolean")
                .arg(descriptor.sourceId);
            return false;
        }
        descriptor.attributionRequired = object.value(QStringLiteral("attribution_required")).toBool();
    }

    const QFileInfo inputInfo(descriptor.path);
    if (descriptor.enabled && (!inputInfo.exists() || !inputInfo.isFile())) {
        error = QStringLiteral("Source '%1' path does not exist: %2")
            .arg(descriptor.sourceId, descriptor.path);
        return false;
    }

    return true;
}

bool parseManifest(const QString &manifestPath,
                   QString &buildId,
                   int &schemaVersion,
                   QString &manifestJson,
                   QJsonArray &sourceObjects,
                   QList<CompendiumSourceDescriptor> &sources,
                   QString &error)
{
    if (!readTextFile(manifestPath, manifestJson, error)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Manifest JSON parse failed: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    if (!requireString(root, QStringLiteral("build_id"), buildId, error)) {
        return false;
    }

    if (!root.contains(QStringLiteral("schema_version")) || !root.value(QStringLiteral("schema_version")).isDouble()) {
        error = QStringLiteral("Manifest field 'schema_version' must be an integer");
        return false;
    }
    schemaVersion = root.value(QStringLiteral("schema_version")).toInt();
    if (schemaVersion != 1) {
        error = QStringLiteral("Unsupported schema_version %1 (expected 1)").arg(schemaVersion);
        return false;
    }

    if (!root.contains(QStringLiteral("sources")) || !root.value(QStringLiteral("sources")).isArray()) {
        error = QStringLiteral("Manifest field 'sources' must be an array");
        return false;
    }

    sourceObjects = root.value(QStringLiteral("sources")).toArray();
    if (sourceObjects.isEmpty()) {
        error = QStringLiteral("Manifest field 'sources' must not be empty");
        return false;
    }

    for (const QJsonValue &value : sourceObjects) {
        if (!value.isObject()) {
            error = QStringLiteral("Each manifest source must be an object");
            return false;
        }

        CompendiumSourceDescriptor descriptor;
        if (!parseSourceDescriptor(value.toObject(), descriptor, error)) {
            return false;
        }
        sources.append(descriptor);
    }

    return true;
}

bool execPrepared(QSqlQuery &query, QString &error, const QString &context)
{
    if (!query.exec()) {
        error = QStringLiteral("%1 failed: %2").arg(context, query.lastError().text());
        return false;
    }
    return true;
}

int scalarCount(QSqlDatabase &database, const QString &sql, QString &error)
{
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        error = query.lastError().text();
        return -1;
    }
    if (!query.next()) {
        error = QStringLiteral("No rows returned for count query");
        return -1;
    }
    return query.value(0).toInt();
}

bool integrityCheckOk(QSqlDatabase &database, QString &error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA integrity_check"))) {
        error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        error = QStringLiteral("PRAGMA integrity_check returned no rows");
        return false;
    }
    if (query.value(0).toString().trimmed().compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        error = query.value(0).toString().trimmed();
        return false;
    }
    return true;
}

bool writeReport(const QString &path, const QJsonObject &report, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        error = QStringLiteral("Failed to write report %1: %2").arg(path, file.errorString());
        return false;
    }

    file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

int handleBuildCompendiumCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("build-compendium")) return 0;

    const QString manifestPath = ctx.parser.value("compendium-manifest").trimmed();
    const QString outputPath = ctx.parser.value("compendium-output").trimmed();

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

    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (compendiumDir.isEmpty()) {
        qCritical() << "✗ Could not locate data/compendium directory";
        return 1;
    }

    const QStringList sqlScripts = {
        QDir(compendiumDir).filePath(QStringLiteral("migrations/0001_phase1_canonical_schema.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("seeds/0001_regions.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("seeds/0002_systems.sql")),
        QDir(compendiumDir).filePath(QStringLiteral("seeds/0003_merge_policy.sql"))
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

    QFile::remove(outputInfo.absoluteFilePath());

    const QString connectionName = QStringLiteral("compendium-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QElapsedTimer timer;
    timer.start();

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(outputInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open output database:" << database.lastError().text();
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

    QSqlQuery buildQuery(database);
    buildQuery.prepare(QStringLiteral(
        "INSERT INTO compendium_builds (build_id, schema_version, built_at, source_manifest_json, notes) "
        "VALUES (?, ?, ?, ?, ?)"));
    buildQuery.addBindValue(buildId);
    buildQuery.addBindValue(schemaVersion);
    buildQuery.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    buildQuery.addBindValue(manifestJson);
    buildQuery.addBindValue(QStringLiteral("Phase 1 bootstrap compiler run"));
    if (!execPrepared(buildQuery, error, QStringLiteral("Insert compendium build"))) {
        database.rollback();
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    for (const CompendiumSourceDescriptor &source : sources) {
        QSqlQuery sourceQuery(database);
        sourceQuery.prepare(QStringLiteral(
            "INSERT INTO sources (source_id, display_name, source_type, license_id, license_url, attribution_required, priority, enabled) "
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
        snapshotQuery.prepare(QStringLiteral(
            "INSERT INTO source_snapshots (snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
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

    if (!database.commit()) {
        qCritical() << "✗ Failed to commit compendium metadata:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    // ── Run compiler service (extraction → linking → persistence → merge) ──────
    Remus::Compendium::CompendiumBuildConfig buildConfig;
    buildConfig.buildId       = buildId;
    buildConfig.schemaVersion = schemaVersion;
    buildConfig.manifestJson  = manifestJson;
    for (const CompendiumSourceDescriptor &src : sources) {
        Remus::Compendium::CompendiumSourceConfig cfg;
        cfg.sourceId             = src.sourceId;
        cfg.displayName          = src.displayName;
        cfg.sourceType           = src.sourceType;
        cfg.snapshotId           = src.snapshotId;
        cfg.filePath             = src.path;
        cfg.priority             = src.priority;
        cfg.enabled              = src.enabled;
        cfg.licenseId            = src.licenseId;
        cfg.licenseUrl           = src.licenseUrl;
        cfg.attributionRequired  = src.attributionRequired;
        buildConfig.sources.append(cfg);
    }

    if (!database.transaction()) {
        qCritical() << "✗ Failed to start ingestion transaction:" << database.lastError().text();
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    Remus::Compendium::CompendiumCompilerService service;
    const Remus::Compendium::CompilerStats stats = service.run(buildConfig, database, error);
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

    int systemsCount = scalarCount(database, QStringLiteral("SELECT COUNT(*) FROM systems"), error);
    if (systemsCount < 0) {
        qCritical().noquote() << QStringLiteral("✗ Failed to count systems: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    int conflictsCount = scalarCount(database,
                                     QStringLiteral("SELECT COUNT(*) FROM merge_conflicts WHERE resolution_status = 'unresolved'"),
                                     error);
    if (conflictsCount < 0) {
        qCritical().noquote() << QStringLiteral("✗ Failed to count unresolved conflicts: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    if (!integrityCheckOk(database, error)) {
        qCritical().noquote() << QStringLiteral("✗ Integrity check failed: %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    const QString reportPath = reportPathForDatabase(outputInfo.absoluteFilePath());
    QJsonObject report;
    report.insert(QStringLiteral("build_id"), buildId);
    report.insert(QStringLiteral("schema_version"), schemaVersion);
    report.insert(QStringLiteral("input_sources"), sourceObjects);
    report.insert(QStringLiteral("records_ingested"), stats.recordsIngested);
    report.insert(QStringLiteral("games_created"), stats.gamesCreated);
    report.insert(QStringLiteral("signatures_created"), stats.signaturesCreated);
    report.insert(QStringLiteral("serials_created"), stats.serialsCreated);
    report.insert(QStringLiteral("facts_created"), stats.factsCreated);
    report.insert(QStringLiteral("resolved_fields"), stats.resolvedFields);
    report.insert(QStringLiteral("unresolved_conflicts"), conflictsCount);
    report.insert(QStringLiteral("duration_ms"), static_cast<qint64>(timer.elapsed()));

    if (!writeReport(reportPath, report, error)) {
        qCritical().noquote() << QStringLiteral("✗ %1").arg(error);
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Build Compendium ===";
    qInfo() << "Manifest:" << manifestInfo.absoluteFilePath();
    qInfo() << "Output:" << outputInfo.absoluteFilePath();
    qInfo() << "Report:" << reportPath;
    qInfo() << "Build ID:" << buildId;
    qInfo() << "Sources recorded:" << sources.size();
    qInfo() << "Seeded systems:" << systemsCount;
    qInfo() << "Unresolved conflicts:" << conflictsCount;

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return conflictsCount > 0 ? 2 : 0;
}
