#include "artwork_downloader.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include "../core/constants/constants.h"

namespace Remus {

ArtworkDownloader::ArtworkDownloader(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

bool ArtworkDownloader::download(const QUrl &url, const QString &destPath)
{
    if (!url.isValid()) {
        emit downloadFailed(url, "Invalid URL");
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

    emit downloadCompleted(url, destPath);
    return true;
}

QByteArray ArtworkDownloader::downloadToMemory(const QUrl &url)
{
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
        emit downloadFailed(url, "Download timeout");
    }

    reply->deleteLater();
    return data;
}

} // namespace Remus
