#include <QtTest>
#include <QElapsedTimer>
#include "metadata/rate_limiter.h"

using namespace Remus;

class RateLimiterTest : public QObject {
    Q_OBJECT

private slots:
    void respectsInterval();
    void resetClearsLastRequest();
};

void RateLimiterTest::respectsInterval()
{
    RateLimiter limiter;
    limiter.setInterval(20);

    QElapsedTimer timer;
    timer.start();

    limiter.waitIfNeeded();           // first call sets timestamp
    limiter.waitIfNeeded();           // second call should sleep ~20ms

    const qint64 elapsed = timer.elapsed();
    QVERIFY2(elapsed >= 15, "Rate limiter should delay subsequent calls");
}

void RateLimiterTest::resetClearsLastRequest()
{
    RateLimiter limiter;
    limiter.setInterval(10);

    limiter.waitIfNeeded();
    limiter.reset();

    // After reset, next call should not wait because timestamp cleared
    QElapsedTimer timer;
    timer.start();
    limiter.waitIfNeeded();

    QVERIFY(timer.elapsed() < 5);
}

QTEST_MAIN(RateLimiterTest)
#include "test_rate_limiter.moc"
