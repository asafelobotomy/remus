#include "http_metadata_provider.h"
#include "../core/constants/constants.h"
#include <QEventLoop>
#include <QTimer>

namespace Remus {

HttpMetadataProvider::HttpMetadataProvider(int rateLimitMs, QObject *parent)
    : MetadataProvider(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this))
{
    m_rateLimiter->setInterval(rateLimitMs);
}

void HttpMetadataProvider::throttle()
{
    m_rateLimiter->waitIfNeeded();
}

HttpMetadataProvider::ApiResponse
HttpMetadataProvider::waitForReply(QNetworkReply *reply, int timeoutMs)
{
    ApiResponse response;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(timeoutMs);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();

        response.httpStatusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError) {
            response.success = true;
            response.data = reply->readAll();
        } else {
            response.success = false;
            response.error = reply->errorString();
        }
    } else {
        response.success = false;
        response.error = Constants::Errors::MetadataProvider::REQUEST_TIMEOUT;
    }

    reply->deleteLater();
    return response;
}

} // namespace Remus
