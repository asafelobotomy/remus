#include "metadata_provider.h"
#include "../core/constants/constants.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>

namespace Remus {

MetadataProvider::MetadataProvider(QObject *parent)
    : QObject(parent)
{
}

void MetadataProvider::setCredentials(const QString &username, const QString &password)
{
    m_username = username;
    m_password = password;
    m_authenticated = !username.isEmpty();
}

QByteArray MetadataProvider::downloadImage(const QUrl &url)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);

    QNetworkReply *reply = manager.get(request);
    
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::METADATA_TIMEOUT_MS);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            reply->deleteLater();
            return data;
        }
    }

    reply->deleteLater();
    return QByteArray();
}

bool MetadataProvider::isAvailable()
{
    // Default implementation - assume available
    // Subclasses can override to ping API
    return true;
}

} // namespace Remus
