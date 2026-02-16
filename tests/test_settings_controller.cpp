#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include "../src/ui/controllers/settings_controller.h"
#include "../src/core/constants/constants.h"

using namespace Remus;

class SettingsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void testSettingsReadWrite();
    void testFirstRun();
    void testKeysAndDefaults();
};

void SettingsControllerTest::testSettingsReadWrite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());

    SettingsController controller;

    controller.setSetting("test/key", "value");
    QCOMPARE(controller.getSetting("test/key"), QStringLiteral("value"));

    controller.setValue("test/variant", 42);
    QCOMPARE(controller.getValue("test/variant").toInt(), 42);

    QVariantMap all = controller.getAllSettings();
    QVERIFY(all.contains("test/key"));
    QVERIFY(all.contains("test/variant"));
}

void SettingsControllerTest::testFirstRun()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());

    QSettings settings(Constants::SETTINGS_ORGANIZATION, Constants::SETTINGS_APPLICATION);
    settings.clear();
    settings.sync();

    SettingsController controller;

    QVERIFY(controller.isFirstRun());
    controller.markFirstRunComplete();
    QVERIFY(!controller.isFirstRun());
}

void SettingsControllerTest::testKeysAndDefaults()
{
    SettingsController controller;

    QVariantMap keys = controller.keys();
    QVERIFY(keys.contains("screenscraperUsername"));
    QVERIFY(keys.contains("igdbClientId"));
    QVERIFY(keys.contains("organizeNamingTemplate"));

    QVariantMap defaults = controller.defaults();
    QVERIFY(defaults.contains("providerPriority"));
    QVERIFY(defaults.contains("namingTemplate"));
    QVERIFY(defaults.contains("templateVariableHint"));

    QCOMPARE(defaults["namingTemplate"].toString(), Constants::Settings::Defaults::NAMING_TEMPLATE);
}

QTEST_MAIN(SettingsControllerTest)
#include "test_settings_controller.moc"
