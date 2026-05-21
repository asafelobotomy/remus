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
#include <QHostAddress>
#include <QHostInfo>
#include <QStandardPaths>
#include <QTimer>
#include "../core/constants/constants.h"

namespace {

// Returns false if addr is loopback, private, link-local, or CGNAT.
static bool isCatalogAddressAllowed(const QHostAddress &addr)
{
    if (addr.isLoopback() || addr.isNull())
        return false;
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = addr.toIPv4Address();
        const quint8 a = static_cast<quint8>((ip >> 24) & 0xFF);
        const quint8 b = static_cast<quint8>((ip >> 16) & 0xFF);
        return !(a == 0 || a == 10 || a == 127
                || (a == 100 && b >= 64 && b <= 127)
                || (a == 169 && b == 254)
                || (a == 172 && b >= 16 && b <= 31)
                || (a == 192 && b == 168));
    }
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR ip = addr.toIPv6Address();
        bool unspec = true;
        for (quint8 byte : ip.c) { if (byte) { unspec = false; break; } }
        return !unspec
            && !((ip.c[0] & 0xFE) == 0xFC)
            && !((ip.c[0] == 0xFE && (ip.c[1] & 0xC0) == 0x80));
    }
    return true;
}

// Reject loopback, private, link-local, and CGNAT catalog hosts.
// For hostnames every resolved address is checked (DNS rebinding defence).
static bool isCatalogHostAllowed(const QString &host)
{
    const QString h = host.trimmed().toLower();
    if (h.isEmpty() || h == QLatin1String("localhost")
            || h.endsWith(QLatin1String(".localhost")))
        return false;

    QHostAddress addr;
    if (!addr.setAddress(h)) {
        const QHostInfo info = QHostInfo::fromName(h);
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty())
            return false;
        for (const QHostAddress &a : info.addresses()) {
            if (!isCatalogAddressAllowed(a)) return false;
        }
        return true;
    }
    return isCatalogAddressAllowed(addr);
}

constexpr int MAX_CATALOG_REDIRECTS = 5;

} // namespace

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

    // Download from network with manual redirect handling so each hop's resolved
    // address is revalidated against private/loopback rules (DNS rebinding defence).
    QNetworkAccessManager manager;
    QUrl currentUrl = url;

    for (int redirectCount = 0; redirectCount <= MAX_CATALOG_REDIRECTS; ++redirectCount) {
        if (!isCatalogHostAllowed(currentUrl.host())) {
            if (cacheInfo.exists()) {
                QFile cacheFile(cachePath);
                if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll()))
                    return true;
            }
            m_lastError = QStringLiteral("Catalog URL targets a disallowed host: ")
                          + currentUrl.host();
            return false;
        }

        QNetworkRequest request(currentUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);

        // Send ETag / If-Modified-Since only on the first hop (original URL)
        if (redirectCount == 0 && cacheInfo.exists()) {
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
            reply->abort();
            reply->deleteLater();
            if (cacheInfo.exists()) {
                QFile cacheFile(cachePath);
                if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll()))
                    return true;
            }
            m_lastError = QStringLiteral("Catalog download timed out: ") + url.toString();
            return false;
        }
        timeout.stop();

        const QVariant redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirectTarget.isValid()) {
            const QUrl redirectedUrl = currentUrl.resolved(redirectTarget.toUrl());
            reply->deleteLater();
            if (redirectedUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
                m_lastError = QStringLiteral("Catalog redirect to non-HTTPS URL is not permitted");
                return false;
            }
            currentUrl = redirectedUrl;
            continue;
        }

        // 304 Not Modified — cache is still valid (meaningful only on first hop)
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 304) {
            reply->deleteLater();
            QFile cacheFile(cachePath);
            if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll()))
                return true;
            m_lastError = QStringLiteral("304 received but cache file unreadable");
            return false;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString netErr = reply->errorString();
            reply->deleteLater();
            if (cacheInfo.exists()) {
                QFile cacheFile(cachePath);
                if (cacheFile.open(QIODevice::ReadOnly) && loadFromJson(cacheFile.readAll()))
                    return true;
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

    m_lastError = QStringLiteral("Too many redirects fetching catalog: ") + url.toString();
    return false;
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
