#include "compendium_manifest_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

namespace Remus {

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

bool requireString(const QJsonObject &object,
                   const QString &fieldName,
                   QString &value,
                   QString &error,
                   bool allowEmpty)
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

QString resolveManifestRelativePath(const QString &manifestPath, const QString &sourcePath)
{
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isAbsolute()) {
        return sourceInfo.absoluteFilePath();
    }

    const QFileInfo manifestInfo(manifestPath);
    return manifestInfo.dir().absoluteFilePath(sourcePath);
}

bool parseSourceDescriptor(const QJsonObject &object,
                           const QString &manifestPath,
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
    descriptor.path = resolveManifestRelativePath(manifestPath, descriptor.path);

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
        if (!parseSourceDescriptor(value.toObject(), manifestPath, descriptor, error)) {
            return false;
        }
        sources.append(descriptor);
    }

    return true;
}

} // namespace Remus
