#include <QtTest/QtTest>
#include <QSettings>

#include "../src/metadata/metadata_rate_limits.h"
#include "../src/core/constants/constants.h"
#include "../src/core/constants/network.h"

using namespace Remus;

class MetadataRateLimitsTest : public QObject {
    Q_OBJECT

private slots:
    void returnsDefaultWhenUnset();
    void perProviderOverrideTakesPrecedence();
};

void MetadataRateLimitsTest::returnsDefaultWhenUnset() {
    const int value = configuredRateLimitMs(QStringLiteral("hasheous"), Constants::Network::HASHEOUS_RATE_LIMIT_MS);
    QCOMPARE(value, Constants::Network::HASHEOUS_RATE_LIMIT_MS);
}

void MetadataRateLimitsTest::perProviderOverrideTakesPrecedence() {
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
        QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    settings.setValue(QStringLiteral("metadata/rate_limit/hasheous"), 1234);
    settings.sync();

    const int value = configuredRateLimitMs(QStringLiteral("hasheous"), Constants::Network::HASHEOUS_RATE_LIMIT_MS);
    QCOMPARE(value, 1234);

    settings.remove(QStringLiteral("metadata/rate_limit/hasheous"));
    settings.sync();
}

QTEST_MAIN(MetadataRateLimitsTest)
#include "test_metadata_rate_limits.moc"
