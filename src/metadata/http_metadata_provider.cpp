#include "http_metadata_provider.h"
#include "metadata_rate_limits.h"
#include "../core/constants/constants.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

namespace Remus {

HttpMetadataProvider::HttpMetadataProvider(int rateLimitMs, QObject *parent)
    : MetadataProvider(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this)) {
    m_rateLimiter->setInterval(rateLimitMs);
}

HttpMetadataProvider::HttpMetadataProvider(const QString &providerSettingsKey, int defaultRateLimitMs, QObject *parent)
    : HttpMetadataProvider(configuredRateLimitMs(providerSettingsKey, defaultRateLimitMs), parent) { }

void HttpMetadataProvider::processNetworkEvents() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void HttpMetadataProvider::throttle() {
    m_rateLimiter->waitIfNeeded();
}

HttpMetadataProvider::ApiResponse HttpMetadataProvider::waitForReply(QNetworkReply *reply, int timeoutMs) {
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

        response.httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError) {
            response.success = true;
            response.data = reply->readAll();
        } else {
            response.success = false;
            response.error = reply->errorString();
        }
    } else {
        // Timed out: abort the request so it doesn't continue in the background
        reply->abort();
        response.success = false;
        response.error = Constants::Errors::MetadataProvider::REQUEST_TIMEOUT;
    }

    // deleteLater() is required — not delete reply — because Qt's QNAM uses thread-pool
    // workers for SSL operations. Even after the finished signal fires, background SSL
    // cleanup may still hold a pointer to the reply's underlying socket. Immediate
    // deletion causes a use-after-free. deleteLater() defers destruction until all
    // posted events (including SSL cleanup) have been processed by the event loop.
    // Callers responsible for many sequential requests must flush the deferred-delete
    // queue periodically using QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete).
    reply->deleteLater();
    return response;
}

} // namespace Remus
