#include "artwork_downloader.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QImageReader>
#include "../core/constants/constants.h"

namespace Remus {

namespace {

constexpr int MAX_REMOTE_REDIRECTS = 5;

bool isDisallowedRemoteAddress(const QHostAddress &address)
{
    if (address.isLoopback() || address.isNull()) {
        return true;
    }

    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ipv4 = address.toIPv4Address();
        const quint8 firstOctet = static_cast<quint8>((ipv4 >> 24) & 0xFF);
        const quint8 secondOctet = static_cast<quint8>((ipv4 >> 16) & 0xFF);

        return firstOctet == 0
            || firstOctet == 10
            || firstOctet == 127
            || (firstOctet == 100 && secondOctet >= 64 && secondOctet <= 127)
            || (firstOctet == 169 && secondOctet == 254)
            || (firstOctet == 172 && secondOctet >= 16 && secondOctet <= 31)
            || (firstOctet == 192 && secondOctet == 168);
    }

    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const Q_IPV6ADDR ipv6 = address.toIPv6Address();
        bool unspecified = true;
        for (quint8 byte : ipv6.c) {
            if (byte != 0) {
                unspecified = false;
                break;
            }
        }

        return unspecified
            || ((ipv6.c[0] & 0xFE) == 0xFC) // fc00::/7 unique local
            || (ipv6.c[0] == 0xFE && (ipv6.c[1] & 0xC0) == 0x80); // fe80::/10 link-local
    }

    return false;
}

bool isDisallowedRemoteHost(const QString &host)
{
    const QString normalizedHost = host.trimmed().toLower();
    if (normalizedHost.isEmpty()) {
        return true;
    }

    if (normalizedHost == QStringLiteral("localhost")
        || normalizedHost.endsWith(QStringLiteral(".localhost"))) {
        return true;
    }

    QHostAddress address;
    if (!address.setAddress(normalizedHost)) {
        return false;
    }

    return isDisallowedRemoteAddress(address);
}

} // namespace

ArtworkDownloader::ArtworkDownloader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

ArtworkDownloader::ArtworkDownloader(QNetworkAccessManager *mgr, QObject *parent)
    : QObject(parent)
    , m_networkManager(mgr)
{
}

bool ArtworkDownloader::isSupportedUrl(const QUrl &url)
{
    if (!url.isValid()) {
        return false;
    }

    if (url.isLocalFile()) {
        return true;
    }

    return url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 &&
        !url.host().trimmed().isEmpty();
}

bool ArtworkDownloader::isSupportedRemoteUrl(const QUrl &url)
{
    // Only HTTPS is permitted for remote/provider-sourced artwork URLs.
    // file://, loopback, and private-network targets are intentionally rejected
    // to prevent local-file or local-network access via a compromised provider
    // response.
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !isDisallowedRemoteHost(url.host());
}

bool ArtworkDownloader::areResolvedRemoteAddressesAllowed(const QList<QHostAddress> &addresses)
{
    if (addresses.isEmpty()) {
        return false;
    }

    for (const QHostAddress &address : addresses) {
        if (isDisallowedRemoteAddress(address)) {
            return false;
        }
    }

    return true;
}

bool ArtworkDownloader::download(const QUrl &url, const QString &destPath, QString *savedPath)
{
    if (!isSupportedUrl(url)) {
        emit downloadFailed(url, "Unsupported artwork URL");
        return false;
    }

    // Ensure destination directory exists
    QFileInfo fileInfo(destPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QByteArray data = downloadToMemory(url);
    if (data.isEmpty()) {
        return false;
    }

    // Save to file
    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit downloadFailed(url, QString("Failed to open file: %1").arg(destPath));
        return false;
    }

    file.write(data);
    file.close();

    // Detect actual image format and fix extension if mismatched
    QString finalPath = destPath;
    QByteArray detectedFormat = QImageReader::imageFormat(destPath);
    if (!detectedFormat.isEmpty()) {
        QString correctExt = QLatin1Char('.') + QString::fromLatin1(detectedFormat);
        QString currentExt = QFileInfo(destPath).suffix().toLower();
        // Normalize jpeg → jpg for comparison
        if (correctExt == QLatin1String(".jpeg")) correctExt = QStringLiteral(".jpg");
        if (currentExt == QLatin1String("jpeg")) currentExt = QStringLiteral("jpg");

        if (!currentExt.isEmpty() && correctExt != (QLatin1Char('.') + currentExt)) {
            finalPath = destPath.left(destPath.lastIndexOf(QLatin1Char('.'))) + correctExt;
            if (QFile::rename(destPath, finalPath)) {
                qDebug() << "Artwork format corrected:" << destPath << "->" << finalPath;
            } else {
                finalPath = destPath; // rename failed, keep original
            }
        }
    }

    if (savedPath) {
        *savedPath = finalPath;
    }

    emit downloadCompleted(url, finalPath);
    return true;
}

QByteArray ArtworkDownloader::downloadToMemory(const QUrl &url)
{
    if (!isSupportedUrl(url)) {
        emit downloadFailed(url, "Unsupported artwork URL");
        return {};
    }

    QUrl currentUrl = url;
    for (int redirectCount = 0; redirectCount <= MAX_REMOTE_REDIRECTS; ++redirectCount) {
        if (!currentUrl.isLocalFile()) {
            if (!isSupportedRemoteUrl(currentUrl)) {
                emit downloadFailed(currentUrl, "Unsupported artwork URL");
                return {};
            }

            const QHostInfo hostInfo = QHostInfo::fromName(currentUrl.host());
            if (hostInfo.error() != QHostInfo::NoError
                || !areResolvedRemoteAddressesAllowed(hostInfo.addresses())) {
                emit downloadFailed(currentUrl, "Resolved artwork host is not allowed");
                return {};
            }
        }

        QNetworkRequest request(currentUrl);
        request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::ManualRedirectPolicy);

        QNetworkReply *reply = m_networkManager->get(request);

        connect(reply, &QNetworkReply::downloadProgress, this,
                [this, currentUrl](qint64 bytesReceived, qint64 bytesTotal) {
                    emit downloadProgress(currentUrl, bytesReceived, bytesTotal);
                });

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(Constants::Network::ARTWORK_TIMEOUT_MS);

        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        timeout.start();
        loop.exec();

        if (!timeout.isActive()) {
            reply->abort();
            reply->deleteLater();
            emit downloadFailed(currentUrl, "Download timeout");
            return {};
        }

        timeout.stop();

        const QVariant redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirectTarget.isValid()) {
            const QUrl redirectedUrl = currentUrl.resolved(redirectTarget.toUrl());
            reply->deleteLater();

            if (!isSupportedRemoteUrl(redirectedUrl)) {
                emit downloadFailed(redirectedUrl, "Redirected artwork URL is not allowed");
                return {};
            }

            currentUrl = redirectedUrl;
            continue;
        }

        if (reply->error() == QNetworkReply::NoError) {
            if (!reply->isOpen()) {
                qWarning() << "remus.artwork: device not open after download from"
                           << currentUrl.toString();
                reply->deleteLater();
                return {};
            }
            const QByteArray data = reply->readAll();
            reply->deleteLater();
            return data;
        }

        const QString errorString = reply->errorString();
        reply->deleteLater();
        emit downloadFailed(currentUrl, errorString);
        return {};
    }

    emit downloadFailed(currentUrl, "Too many artwork redirects");
    return {};
}

} // namespace Remus
