#include <QtTest>
#include <QSettings>
#include "ui/theme_constants.h"

using namespace Remus;

class ThemeConstantsTest : public QObject {
    Q_OBJECT

private slots:
    void togglesAndPersists();
    void exposesConfidenceHelpers();
};

void ThemeConstantsTest::togglesAndPersists()
{
    QSettings settings;
    settings.clear();

    ThemeConstants theme;
    bool initial = theme.isDarkMode();

    QSignalSpy spy(&theme, &ThemeConstants::themeModeChanged);
    theme.toggleTheme();
    QCOMPARE(spy.count(), 1);
    QVERIFY(theme.isDarkMode() != initial);

    // Colors should flip between dark and light variants
    QString prevPrimary = theme.primary();
    theme.toggleTheme();
    QVERIFY(prevPrimary != theme.primary());
}

void ThemeConstantsTest::exposesConfidenceHelpers()
{
    ThemeConstants theme;

    QCOMPARE(theme.confidenceHigh(), 90);
    QCOMPARE(theme.confidenceMedium(), 60);
    QCOMPARE(theme.confidenceColor(95), theme.success());
    QCOMPARE(theme.confidenceColor(70), theme.warning());
    QCOMPARE(theme.confidenceColor(10), theme.danger());
}

QTEST_MAIN(ThemeConstantsTest)
#include "test_theme_constants.moc"
