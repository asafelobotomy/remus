#include "mod_workflow_service.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "../core/constants/constants.h"

namespace {
/// Reject loopback, private, link-local, and CGNAT hosts to prevent SSRF
/// via a compromised remote mod catalog directing patch downloads to internal
/// addresses.
static bool isPatchHostAllowed(const QString &host)
{
    const QString h = host.trimmed().toLower();
    if (h.isEmpty() || h == QLatin1String("localhost")
            || h.endsWith(QLatin1String(".localhost")))
        return false;

    QHostAddress addr;
    if (!addr.setAddress(h))
        return true; // hostname — rely on OS resolver; DNS rebinding is out of scope here

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
            && !((ip.c[0] & 0xFE) == 0xFC)           // fc00::/7 unique-local
            && !((ip.c[0] == 0xFE && (ip.c[1] & 0xC0) == 0x80)); // fe80::/10 link-local
    }
    return true;
}
} // namespace

namespace Remus {

QString ModWorkflowService::resolvePatchPath(const QString &patchUrl,
                                            QString &error,
                                            ProgressCallback cb)
{
    if (patchUrl.isEmpty()) {
        error = "Empty patch URL";
        return {};
    }

    const QUrl url(patchUrl);
    if (url.scheme() == QStringLiteral("file")) {
        if (m_catalogIsRemote) {
            error = "Local file patch sources are not permitted when catalog is remote";
            return {};
        }
        const QString localPath = url.toLocalFile();
        if (!QFile::exists(localPath)) {
            error = "Local patch file not found: " + localPath;
            return {};
        }
        return localPath;
    }

    if (url.scheme().isEmpty() || url.isRelative()) {
        if (m_catalogIsRemote) {
            error = "Relative patch sources are not permitted when catalog is remote";
            return {};
        }
        if (!QFile::exists(patchUrl)) {
            error = "Patch file not found: " + patchUrl;
            return {};
        }
        return patchUrl;
    }

    if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
        if (url.scheme() == QStringLiteral("http")) {
            error = "Insecure HTTP patch sources are not permitted; use HTTPS";
            return {};
        }
        // Reject private/loopback/link-local targets to prevent SSRF via
        // a compromised remote catalog directing downloads to internal hosts.
        if (!isPatchHostAllowed(url.host())) {
            error = "Patch URL targets a disallowed host or scheme";
            return {};
        }
        return downloadPatch(url, error, cb);
    }

    error = "Unsupported URL scheme: " + url.scheme();
    return {};
}

QString ModWorkflowService::downloadPatch(const QUrl &url,
                                         QString &error,
                                         ProgressCallback cb)
{
    if (cb) cb("downloading", 2);

    if (!m_downloadDir) {
        m_downloadDir = std::make_unique<QTemporaryDir>();
        if (!m_downloadDir->isValid()) {
            error = "Failed to create temp directory for patch download";
            return {};
        }
    }

    QString filename = QFileInfo(url.path()).fileName();
    if (filename.isEmpty()) {
        filename = QStringLiteral("patch.bin");
    }
    const QString destPath = m_downloadDir->path() + QStringLiteral("/") + filename;

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = manager.get(request);
    if (cb) {
        QObject::connect(reply, &QNetworkReply::downloadProgress,
            [&cb](qint64 received, qint64 total) {
                if (total > 0) {
                    const int pct = 2 + static_cast<int>(received * 10 / total);
                    cb("downloading", pct);
                }
            });
    }

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
        error = "Patch download timed out: " + url.toString();
        return {};
    }
    timeout.stop();

    if (reply->error() != QNetworkReply::NoError) {
        error = "Patch download failed: " + reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    if (data.isEmpty()) {
        error = "Downloaded patch file is empty";
        return {};
    }

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "Failed to write downloaded patch: " + destPath;
        return {};
    }
    file.write(data);
    file.close();

    if (cb) cb("downloaded", 12);
    return destPath;
}

bool ModWorkflowService::verifySha1(const QString &filePath, const QString &expectedSha1)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    const QString actual = QString::fromLatin1(hash.result().toHex());
    return actual.compare(expectedSha1, Qt::CaseInsensitive) == 0;
}

} // namespace Remus