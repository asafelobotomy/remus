#include "mod_catalog_provider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTimer>
#include "../core/constants/constants.h"

namespace Remus {

bool ModCatalogProvider::loadFromFile(const QString &path)
{
    m_mods.clear();
    m_lastError.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("Cannot open catalog file: ") + path;
        return false;
    }

    return loadFromJson(file.readAll());
}

bool ModCatalogProvider::loadFromUrl(const QUrl &url, bool forceRefresh)
{
    m_mods.clear();
    m_lastError.clear();

    // Reject plain HTTP before any cache read — a previously cached insecure
    // response must not be served on subsequent calls to avoid bypassing the guard.
    if (url.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0) {
        m_lastError = QStringLiteral("Insecure HTTP catalog URL is not permitted; use HTTPS");
        return false;
    }

    const QString cachePath = cacheFileForUrl(url);
    QFileInfo cacheInfo(cachePath);

    // Use cache if fresh enough and not forcing refresh
    if (!forceRefresh && cacheInfo.exists()) {
        const qint64 ageSeconds = cacheInfo.lastModified().secsTo(QDateTime::currentDateTime());
        if (ageSeconds < kCacheTtlSeconds) {
            QFile cacheFile(cachePath);
            if (cacheFile.open(QIODevice::ReadOnly)) {
                if (loadFromJson(cacheFile.readAll())) {
                    return true;
                }
                // Cache corrupted — fall through to network fetch
            }
        }
    }

    // Download from network
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    // Send ETag / If-Modified-Since if we have a cached copy
    if (cacheInfo.exists()) {
        request.setRawHeader("If-Modified-Since",
                             cacheInfo.lastModified().toUTC()
                                 .toString(Qt::RFC2822Date).toUtf8());
    }

    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::ARTWORK_TIMEOUT_MS);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    if (!timeout.isActive()) {
        // Abort the in-flight request on timeout before falling back to cache
        reply->abort();
        reply->deleteLater();
        // Timeout — fall back to stale cache if available
        if (cacheInfo.exists()) {
            QFile cacheFile(cachePath);
            if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll())) {
                return true;
            }
        }
        m_lastError = QStringLiteral("Catalog download timed out: ") + url.toString();
        return false;
    }
    timeout.stop();

    // 304 Not Modified — cache is still valid
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 304) {
        reply->deleteLater();
        QFile cacheFile(cachePath);
        if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll())) {
            return true;
        }
        m_lastError = QStringLiteral("304 received but cache file unreadable");
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString netErr = reply->errorString();
        reply->deleteLater();
        // Network error — fall back to stale cache
        if (cacheInfo.exists()) {
            QFile cacheFile(cachePath);
            if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll())) {
                return true;
            }
        }
        m_lastError = QStringLiteral("Catalog download failed: ") + netErr;
        return false;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (!loadFromJson(data)) {
        return false;
    }

    // Write cache
    QDir().mkpath(cacheDir());
    QFile cacheFile(cachePath);
    if (cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cacheFile.write(data);
    }

    return true;
}

QList<ModEntry> ModCatalogProvider::findModsForRom(const QString &crc32,
                                                    const QString &md5,
                                                    const QString &sha1) const
{
    QList<ModEntry> results;
    for (const ModEntry &mod : m_mods) {
        // Match by SHA1 first (strongest), then MD5, then CRC32
        if (!sha1.isEmpty() && !mod.baseSha1.isEmpty() &&
            sha1.compare(mod.baseSha1, Qt::CaseInsensitive) == 0) {
            results.append(mod);
            continue;
        }
        if (!md5.isEmpty() && !mod.baseMd5.isEmpty() &&
            md5.compare(mod.baseMd5, Qt::CaseInsensitive) == 0) {
            results.append(mod);
            continue;
        }
        if (!crc32.isEmpty() && !mod.baseCrc32.isEmpty() &&
            crc32.compare(mod.baseCrc32, Qt::CaseInsensitive) == 0) {
            results.append(mod);
            continue;
        }
    }
    return results;
}

QList<ModEntry> ModCatalogProvider::findModsBySystem(const QString &system) const
{
    QList<ModEntry> results;
    for (const ModEntry &mod : m_mods) {
        if (mod.system.compare(system, Qt::CaseInsensitive) == 0) {
            results.append(mod);
        }
    }
    return results;
}

std::optional<ModEntry> ModCatalogProvider::getModById(const QString &id) const
{
    for (const ModEntry &mod : m_mods) {
        if (mod.id == id) {
            return mod;
        }
    }
    return std::nullopt;
}

const QList<ModEntry> &ModCatalogProvider::allMods() const
{
    return m_mods;
}

QString ModCatalogProvider::lastError() const
{
    return m_lastError;
}

bool ModCatalogProvider::loadFromJson(const QByteArray &data)
{
    m_mods.clear();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = QStringLiteral("JSON parse error: ") + parseError.errorString();
        return false;
    }

    const QJsonObject root = doc.object();

    const int catalogVersion = root.value(QStringLiteral("catalog_version")).toInt(0);
    if (catalogVersion < 1) {
        m_lastError = QStringLiteral("Unsupported or missing catalog_version");
        return false;
    }

    const QJsonArray modsArray = root.value(QStringLiteral("mods")).toArray();
    m_mods.reserve(modsArray.size());

    for (const QJsonValue &val : modsArray) {
        const QJsonObject obj = val.toObject();

        ModEntry entry;
        entry.id          = obj.value(QStringLiteral("id")).toString();
        entry.title       = obj.value(QStringLiteral("title")).toString();
        entry.author      = obj.value(QStringLiteral("author")).toString();
        entry.version     = obj.value(QStringLiteral("version")).toString();
        entry.description = obj.value(QStringLiteral("description")).toString();
        entry.type        = obj.value(QStringLiteral("type")).toString(Constants::FileTypes::HACK);
        entry.system      = obj.value(QStringLiteral("system")).toString();
        entry.format      = obj.value(QStringLiteral("format")).toString();
        entry.patchUrl    = obj.value(QStringLiteral("patch_url")).toString();
        entry.patchSha1   = obj.value(QStringLiteral("patch_sha1")).toString();
        entry.patchSize   = obj.value(QStringLiteral("patch_size")).toInteger(0);
        entry.sourceUrl   = obj.value(QStringLiteral("source_url")).toString();
        entry.rating      = obj.value(QStringLiteral("rating")).toDouble(0.0);
        entry.downloads   = obj.value(QStringLiteral("downloads")).toInt(0);

        const QJsonObject hashes = obj.value(QStringLiteral("base_rom_hashes")).toObject();
        entry.baseCrc32 = hashes.value(QStringLiteral("crc32")).toString();
        entry.baseMd5   = hashes.value(QStringLiteral("md5")).toString();
        entry.baseSha1  = hashes.value(QStringLiteral("sha1")).toString();

        if (entry.id.isEmpty() || entry.title.isEmpty()) {
            continue;
        }

        m_mods.append(std::move(entry));
    }

    return true;
}

QString ModCatalogProvider::cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/mod_catalog_cache");
}

QString ModCatalogProvider::cacheFileForUrl(const QUrl &url)
{
    const QByteArray hash = QCryptographicHash::hash(
        url.toString().toUtf8(), QCryptographicHash::Sha1);
    return cacheDir() + QStringLiteral("/") + hash.toHex() + QStringLiteral(".json");
}

} // namespace Remus
