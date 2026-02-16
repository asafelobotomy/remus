#include "rate_limiter.h"
#include <QThread>
#include <QDebug>

namespace Remus {

RateLimiter::RateLimiter(QObject *parent)
    : QObject(parent)
{
}

void RateLimiter::setInterval(int milliseconds)
{
    QMutexLocker locker(&m_mutex);
    m_intervalMs = milliseconds;
}

void RateLimiter::waitIfNeeded()
{
    QMutexLocker locker(&m_mutex);

    if (!m_lastRequest.isValid()) {
        m_lastRequest = QDateTime::currentDateTime();
        return;
    }

    qint64 elapsed = m_lastRequest.msecsTo(QDateTime::currentDateTime());
    
    if (elapsed < m_intervalMs) {
        int sleepTime = m_intervalMs - elapsed;
        qDebug() << "Rate limiter: waiting" << sleepTime << "ms";
        locker.unlock();  // Unlock during sleep
        QThread::msleep(sleepTime);
        locker.relock();
    }

    m_lastRequest = QDateTime::currentDateTime();
}

void RateLimiter::reset()
{
    QMutexLocker locker(&m_mutex);
    m_lastRequest = QDateTime();
}

qint64 RateLimiter::timeSinceLastRequest() const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_lastRequest.isValid()) {
        return m_intervalMs;  // Return interval if no request yet
    }
    
    return m_lastRequest.msecsTo(QDateTime::currentDateTime());
}

} // namespace Remus
