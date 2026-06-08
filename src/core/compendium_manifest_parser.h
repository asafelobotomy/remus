#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace Remus {

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

bool readTextFile(const QString &path, QString &content, QString &error);

bool requireString(
    const QJsonObject &object, const QString &fieldName, QString &value, QString &error, bool allowEmpty = false);

QString resolveManifestRelativePath(const QString &manifestPath, const QString &sourcePath);

bool parseSourceDescriptor(
    const QJsonObject &object, const QString &manifestPath, CompendiumSourceDescriptor &descriptor, QString &error);

bool parseManifest(const QString &manifestPath, QString &buildId, int &schemaVersion, QString &manifestJson,
    QJsonArray &sourceObjects, QList<CompendiumSourceDescriptor> &sources, QString &error);

} // namespace Remus
