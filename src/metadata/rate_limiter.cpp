#include "rate_limiter.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
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
        locker.unlock();  // Unlock during sleep
        if (QThread::currentThread() == qApp->thread()) {
            // Main/GUI thread: pump the event loop so the UI stays responsive.
            QEventLoop loop;
            QTimer::singleShot(sleepTime, &loop, &QEventLoop::quit);
            loop.exec();
        } else {
            QThread::msleep(sleepTime);
        }
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
