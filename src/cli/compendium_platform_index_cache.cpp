#include "compendium_platform_index_cache.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace CompendiumPlatformIndexCache {

QString cacheRootDir() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty())
        return QDir(dataDir).filePath(QStringLiteral("compendium/cache"));
    return QStringLiteral("data/compendium/cache");
}

static QString cacheFilePath(const QString &providerKey, const QString &platformKey) {
    QString safePlatform = platformKey;
    safePlatform.replace(QLatin1Char('/'), QLatin1Char('_'));
    return QDir(cacheRootDir()).filePath(QStringLiteral("%1/%2.json").arg(providerKey, safePlatform));
}

static Remus::GameMetadata metadataFromJson(const QJsonObject &obj) {
    Remus::GameMetadata gm;
    gm.id = obj.value(QStringLiteral("id")).toString();
    gm.title = obj.value(QStringLiteral("title")).toString();
    gm.description = obj.value(QStringLiteral("description")).toString();
    gm.developer = obj.value(QStringLiteral("developer")).toString();
    gm.publisher = obj.value(QStringLiteral("publisher")).toString();
    gm.releaseDate = obj.value(QStringLiteral("releaseDate")).toString();
    gm.players = obj.value(QStringLiteral("players")).toInt();
    gm.rating = static_cast<float>(obj.value(QStringLiteral("rating")).toDouble());
    gm.boxArtUrl = obj.value(QStringLiteral("boxArtUrl")).toString();
    gm.series = obj.value(QStringLiteral("series")).toString();
    gm.ageRating = obj.value(QStringLiteral("ageRating")).toString();
    const QJsonArray genres = obj.value(QStringLiteral("genres")).toArray();
    for (const QJsonValue &g : genres)
        gm.genres.append(g.toString());
    const QJsonObject extIds = obj.value(QStringLiteral("externalIds")).toObject();
    for (auto it = extIds.constBegin(); it != extIds.constEnd(); ++it)
        gm.externalIds.insert(it.key(), it.value().toString());
    return gm;
}

static QJsonObject metadataToJson(const Remus::GameMetadata &gm) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), gm.id);
    obj.insert(QStringLiteral("title"), gm.title);
    obj.insert(QStringLiteral("description"), gm.description);
    obj.insert(QStringLiteral("developer"), gm.developer);
    obj.insert(QStringLiteral("publisher"), gm.publisher);
    obj.insert(QStringLiteral("releaseDate"), gm.releaseDate);
    obj.insert(QStringLiteral("players"), gm.players);
    obj.insert(QStringLiteral("rating"), gm.rating);
    obj.insert(QStringLiteral("boxArtUrl"), gm.boxArtUrl);
    obj.insert(QStringLiteral("series"), gm.series);
    obj.insert(QStringLiteral("ageRating"), gm.ageRating);
    QJsonArray genres;
    for (const QString &g : gm.genres)
        genres.append(g);
    obj.insert(QStringLiteral("genres"), genres);
    QJsonObject extIds;
    for (auto it = gm.externalIds.constBegin(); it != gm.externalIds.constEnd(); ++it)
        extIds.insert(it.key(), it.value());
    obj.insert(QStringLiteral("externalIds"), extIds);
    return obj;
}

bool loadPlatformIndex(
    const QString &providerKey, const QString &platformKey, QList<Remus::GameMetadata> &out, QDateTime *cachedAt) {
    out.clear();
    const QString path = cacheFilePath(providerKey, platformKey);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QDateTime written = QDateTime::fromString(root.value(QStringLiteral("cached_at")).toString(), Qt::ISODate);
    if (!written.isValid() || written.daysTo(QDateTime::currentDateTimeUtc()) > kDefaultTtlDays)
        return false;

    if (cachedAt)
        *cachedAt = written;

    const QJsonArray games = root.value(QStringLiteral("games")).toArray();
    out.reserve(games.size());
    for (const QJsonValue &v : games)
        out.append(metadataFromJson(v.toObject()));
    return !out.isEmpty();
}

bool storePlatformIndex(
    const QString &providerKey, const QString &platformKey, const QList<Remus::GameMetadata> &games) {
    const QString path = cacheFilePath(providerKey, platformKey);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const Remus::GameMetadata &gm : games)
        arr.append(metadataToJson(gm));

    QJsonObject root;
    root.insert(QStringLiteral("cached_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("games"), arr);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return true;
}

} // namespace CompendiumPlatformIndexCache
