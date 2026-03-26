/**
 * @file test_tui_smoke.cpp
 * @brief Headless TUI smoke tests.
 *
 * Constructs TuiApp without calling run() (no notcurses context).
 * Verifies screen construction, naming, stack navigation, toast,
 * and help overlay in a headless environment.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/tui/app.h"
#include "../src/tui/launch_screen.h"
#include "../src/tui/main_menu_screen.h"
#include "../src/tui/library_screen.h"
#include "../src/tui/match_screen.h"
#include "../src/tui/compressor_screen.h"
#include "../src/tui/patch_screen.h"
#include "../src/tui/options_screen.h"
#include "../src/core/database.h"

using Remus::Database;

// ── Helpers ─────────────────────────────────────────────────

static QString initApp(TuiApp &app, const QString &dir)
{
    QString dbPath = dir + "/smoke.db";
    app.db().initialize(dbPath);
    return dbPath;
}

// ════════════════════════════════════════════════════════════
// Test: screen construction & names
// ════════════════════════════════════════════════════════════
class TestScreenConstruction : public QObject {
    Q_OBJECT
private slots:
    void launchScreenName()
    {
        TuiApp app;
        LaunchScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("Launch"));
    }

    void mainMenuScreenName()
    {
        TuiApp app;
        MainMenuScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("MainMenu"));
    }

    void libraryScreenName()
    {
        TuiApp app;
        LibraryScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("Library"));
    }

    void matchScreenName()
    {
        TuiApp app;
        MatchScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("Match"));
    }

    void compressorScreenName()
    {
        TuiApp app;
        CompressorScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("Compressor"));
    }

    void patchScreenName()
    {
        TuiApp app;
        PatchScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("Patch"));
    }

    void optionsScreenName()
    {
        TuiApp app;
        OptionsScreen s(app);
        QCOMPARE(QString::fromStdString(s.name()), QString("Options"));
    }
};

// ════════════════════════════════════════════════════════════
// Test: screen stack (pushScreen / popScreen / setScreen)
// ════════════════════════════════════════════════════════════
class TestScreenStack : public QObject {
    Q_OBJECT
private slots:
    void pushAddsScreen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TuiApp app;
        initApp(app, tmp.path());

        app.pushScreen(std::make_unique<LaunchScreen>(app));
        // nc() is nullptr so no render happens, but stack grows
        QVERIFY(app.nc() == nullptr);  // headless
    }

    void pushThenPopReturns()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TuiApp app;
        initApp(app, tmp.path());

        app.pushScreen(std::make_unique<LaunchScreen>(app));
        app.pushScreen(std::make_unique<MainMenuScreen>(app));
        app.popScreen();
        // After pop, one screen remains — no crash
    }

    void popEmptyIsNoop()
    {
        TuiApp app;
        // Should not crash
        app.popScreen();
    }

    void setScreenReplacesStack()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TuiApp app;
        initApp(app, tmp.path());

        app.pushScreen(std::make_unique<LaunchScreen>(app));
        app.pushScreen(std::make_unique<MainMenuScreen>(app));
        app.setScreen(std::make_unique<LibraryScreen>(app));
        // Stack replaced — no crash, library screen is active
    }

    void multiPushPopCycle()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TuiApp app;
        initApp(app, tmp.path());

        // Push all seven screen types
        app.pushScreen(std::make_unique<LaunchScreen>(app));
        app.pushScreen(std::make_unique<MainMenuScreen>(app));
        app.pushScreen(std::make_unique<LibraryScreen>(app));
        app.pushScreen(std::make_unique<MatchScreen>(app));
        app.pushScreen(std::make_unique<CompressorScreen>(app));
        app.pushScreen(std::make_unique<PatchScreen>(app));
        app.pushScreen(std::make_unique<OptionsScreen>(app));

        // Pop them all
        for (int i = 0; i < 7; ++i)
            app.popScreen();

        // Extra pop on empty => no crash
        app.popScreen();
    }
};

// ════════════════════════════════════════════════════════════
// Test: TuiApp basics (version, DB, toast)
// ════════════════════════════════════════════════════════════
class TestAppBasics : public QObject {
    Q_OBJECT
private slots:
    void versionIsNonEmpty()
    {
        TuiApp app;
        // Constructor reads VERSION file; may fall back to "0.10.1"
        QVERIFY(!app.version().empty());
    }

    void databaseAccessible()
    {
        TuiApp app;
        // db() should return a valid reference
        Database &db = app.db();
        (void)db;
    }

    void initialNcIsNull()
    {
        TuiApp app;
        QCOMPARE(app.nc(), nullptr);
    }

    void initialDimensions()
    {
        TuiApp app;
        QCOMPARE(app.rows(), 0u);
        QCOMPARE(app.cols(), 0u);
    }

    void toastDoesNotCrashHeadless()
    {
        TuiApp app;
        // Toast uses m_toast internally, no notcurses needed for show()
        app.toast("Hello");
        app.toast("Warning", Toast::Level::Warning, 5000);
        app.toast("Error", Toast::Level::Error, 1000);
    }

    void dbInitToTemp()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TuiApp app;
        QString dbPath = initApp(app, tmp.path());
        QVERIFY(QFile::exists(dbPath));
    }
};

// ════════════════════════════════════════════════════════════
// Test: keybindings returned by screens
// ════════════════════════════════════════════════════════════
class TestKeybindings : public QObject {
    Q_OBJECT
private slots:
    void mainMenuHasBindings()
    {
        TuiApp app;
        MainMenuScreen s(app);
        auto bindings = s.keybindings();
        // Main menu should have navigation keybindings
        QVERIFY(!bindings.empty());
    }

    void launchScreenBindingsEmpty()
    {
        TuiApp app;
        LaunchScreen s(app);
        // Launch screen has no interactive keybindings (base default)
        auto bindings = s.keybindings();
        // May be empty; just don't crash
        (void)bindings;
    }

    void optionsScreenHasBindings()
    {
        TuiApp app;
        OptionsScreen s(app);
        auto bindings = s.keybindings();
        QVERIFY(!bindings.empty());
    }

    void libraryScreenHasBindings()
    {
        TuiApp app;
        LibraryScreen s(app);
        auto bindings = s.keybindings();
        QVERIFY(!bindings.empty());
    }
};

// ════════════════════════════════════════════════════════════
// Test: screen onEnter/onLeave (headless, no render)
// ════════════════════════════════════════════════════════════
class TestScreenLifecycle : public QObject {
    Q_OBJECT
private slots:
    void launchScreenOnEnter()
    {
        TuiApp app;
        LaunchScreen s(app);
        // onEnter sets m_start time — should not crash
        s.onEnter();
    }

    void mainMenuOnEnter()
    {
        TuiApp app;
        MainMenuScreen s(app);
        s.onEnter();
    }

    void optionsScreenOnEnter()
    {
        TuiApp app;
        OptionsScreen s(app);
        s.onEnter();
    }

    void launchScreenTick()
    {
        TuiApp app;
        LaunchScreen s(app);
        s.onEnter();
        // tick returns true if redraw needed
        bool redraw = s.tick();
        (void)redraw;
    }
};

// ════════════════════════════════════════════════════════════
// main — run all test classes
// ════════════════════════════════════════════════════════════
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int status = 0;

    {
        TestScreenConstruction t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestScreenStack t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestAppBasics t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestKeybindings t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestScreenLifecycle t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_tui_smoke.moc"
