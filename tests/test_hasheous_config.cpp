#include <QtTest/QtTest>
#include <QSettings>

#include "../src/core/constants/constants.h"
#include "../src/core/hasheous_config.h"

using namespace Remus;
using namespace Remus::Constants;

class HasheousConfigTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testNormalizeBaseUrl();
    void testResolveOverrideBeatsEnv();
    void testResolveEnvBeatsSettings();
    void testResolveDefault();

private:
    QByteArray m_savedEnv;
};

void HasheousConfigTest::init() {
    m_savedEnv = qgetenv("REMUS_HASHEOUS_BASE_URL");
    qunsetenv("REMUS_HASHEOUS_BASE_URL");
}

void HasheousConfigTest::cleanup() {
    if (m_savedEnv.isEmpty())
        qunsetenv("REMUS_HASHEOUS_BASE_URL");
    else
        qputenv("REMUS_HASHEOUS_BASE_URL", m_savedEnv);

    QSettings settings(QString::fromLatin1(SETTINGS_ORGANIZATION), QString::fromLatin1(SETTINGS_APPLICATION));
    settings.remove(QString::fromLatin1(Settings::Providers::HASHEOUS_BASE_URL));
}

void HasheousConfigTest::testNormalizeBaseUrl() {
    QCOMPARE(normalizeHasheousBaseUrl(QStringLiteral("https://example.com///")), QStringLiteral("https://example.com"));
}

void HasheousConfigTest::testResolveOverrideBeatsEnv() {
    qputenv("REMUS_HASHEOUS_BASE_URL", "https://env.example");
    QCOMPARE(resolveHasheousBaseUrl(QStringLiteral("https://cli.example/")), QStringLiteral("https://cli.example"));
}

void HasheousConfigTest::testResolveEnvBeatsSettings() {
    qputenv("REMUS_HASHEOUS_BASE_URL", "https://env.example");

    QSettings settings(QString::fromLatin1(SETTINGS_ORGANIZATION), QString::fromLatin1(SETTINGS_APPLICATION));
    settings.setValue(
        QString::fromLatin1(Settings::Providers::HASHEOUS_BASE_URL), QStringLiteral("https://settings.example"));

    QCOMPARE(resolveHasheousBaseUrl(), QStringLiteral("https://env.example"));
}

void HasheousConfigTest::testResolveDefault() {
    QCOMPARE(resolveHasheousBaseUrl(), QString::fromLatin1(API::HASHEOUS_BASE_URL));
}

QTEST_MAIN(HasheousConfigTest)
#include "test_hasheous_config.moc"
