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
    explicit HttpMetadataProvider(const QString &providerSettingsKey, int defaultRateLimitMs, QObject *parent = nullptr);

    /**
     * @brief Flush all pending deferred-delete events from Qt's network layer.
     *
     * Call this periodically when making many sequential HTTP requests on a shared
     * QNetworkAccessManager to drain deleteLater() events before they accumulate.
     * Also call it before a provider goes out of scope to ensure all pending reply
     * cleanups complete before the QNAM destructs.
     *
     * Centralises the QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete)
     * pattern so enrichers don't need to reference QCoreApplication directly.
     */
    static void processNetworkEvents();

    /**
     * @brief RAII guard that calls processNetworkEvents() on destruction.
     *
     * Declare one of these at the top of a scope (e.g. a per-system loop body)
     * to guarantee a flush on all exit paths, including early `continue` / `return`.
     *
     * @code
     *   for (const auto &sys : systems) {
     *       HttpMetadataProvider::DeferredDeleteFlushGuard guard;
     *       // ... network calls ...
     *   } // guard destructs → processNetworkEvents() called
     * @endcode
     */
    struct DeferredDeleteFlushGuard {
        ~DeferredDeleteFlushGuard() {
            HttpMetadataProvider::processNetworkEvents();
        }
    };

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
