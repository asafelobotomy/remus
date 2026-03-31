#include "artwork_downloader.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QImageReader>
#include "../core/constants/constants.h"

namespace Remus {

ArtworkDownloader::ArtworkDownloader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
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

bool ArtworkDownloader::download(const QUrl &url, const QString &destPath)
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

    emit downloadCompleted(url, finalPath);
    return true;
}

QByteArray ArtworkDownloader::downloadToMemory(const QUrl &url)
{
    if (!isSupportedUrl(url)) {
        emit downloadFailed(url, "Unsupported artwork URL");
        return {};
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, 
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_networkManager->get(request);
    
    // Connect progress signal
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, url](qint64 bytesReceived, qint64 bytesTotal) {
                emit downloadProgress(url, bytesReceived, bytesTotal);
            });

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::ARTWORK_TIMEOUT_MS);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();

    QByteArray data;

    if (timeout.isActive()) {
        timeout.stop();

        if (reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
        } else {
            emit downloadFailed(url, reply->errorString());
        }
    } else {
        // Ensure the in-flight request is aborted on timeout
        reply->abort();
        emit downloadFailed(url, "Download timeout");
    }

    reply->deleteLater();
    return data;
}

} // namespace Remus
