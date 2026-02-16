#ifndef REMUS_RATE_LIMITER_H
#define REMUS_RATE_LIMITER_H

#include <QObject>
#include <QDateTime>
#include <QMutex>

namespace Remus {

/**
 * @brief Rate limiter for API requests
 * 
 * Ensures minimum interval between requests to respect API limits.
 */
class RateLimiter : public QObject {
    Q_OBJECT

public:
    explicit RateLimiter(QObject *parent = nullptr);

    /**
     * @brief Set minimum interval between requests (milliseconds)
     */
    void setInterval(int milliseconds);

    /**
     * @brief Wait if necessary to respect rate limit
     * Blocks until it's safe to make next request
     */
    void waitIfNeeded();

    /**
     * @brief Reset rate limiter
     */
    void reset();

    /**
     * @brief Get time since last request (milliseconds)
     */
    qint64 timeSinceLastRequest() const;

private:
    QDateTime m_lastRequest;
    int m_intervalMs = 1000;  // Default: 1 second
    mutable QMutex m_mutex;
};

} // namespace Remus

#endif // REMUS_RATE_LIMITER_H
