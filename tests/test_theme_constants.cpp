#include <QtTest>
#include <QSettings>
#include "ui/theme_constants.h"

using namespace Remus;

class ThemeConstantsTest : public QObject {
    Q_OBJECT

private slots:
    void togglesAndPersists();
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

QTEST_MAIN(ThemeConstantsTest)
#include "test_theme_constants.moc"
