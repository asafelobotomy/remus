#pragma once

#include "metadata_provider.h"
#include "rate_limiter.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Remus {

/**
 * @brief Intermediate base for providers that talk over HTTP.
 *
 * Owns a QNetworkAccessManager and a RateLimiter, and provides the
 * shared request/reply/timeout boilerplate so concrete providers only
 * need to build the QNetworkRequest and call waitForReply().
 *
 * Thread-safety audit (P2): m_networkManager is a child QObject bound to the
 * thread that creates the HttpMetadataProvider instance.  Qt requires QNAM to
 * be used only from its owning thread.  Concurrent match/enrich pipelines must
 * therefore keep all HTTP provider calls on the provider-owning (main) thread;
 * only CPU/IO-bound steps (disc serial detection, DB-only lookups) may be
 * dispatched to worker threads.
 */
class HttpMetadataProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit HttpMetadataProvider(int rateLimitMs, QObject *parent = nullptr);

protected:
    struct ApiResponse {
        bool success = false;
        QString error;
        QByteArray data;
        int httpStatusCode = 0;
    };

    /** Block until the rate-limit interval has elapsed. */
    void throttle();

    /**
     * @brief Wait for a QNetworkReply using a local event loop + timeout.
     *
     * Handles the QEventLoop / QTimer pattern that was previously duplicated
     * in every HTTP provider's makeRequest().
     *
     * @param reply  Active reply (caller creates it via m_networkManager)
     * @param timeoutMs  Per-request timeout in milliseconds
     * @return Filled ApiResponse; reply is deleteLater()'d automatically.
     */
    ApiResponse waitForReply(QNetworkReply *reply, int timeoutMs);

    QNetworkAccessManager *m_networkManager;
    RateLimiter *m_rateLimiter;
};

} // namespace Remus
