/**
 * @file test_patch_screen.cpp
 * @brief Unit tests for PatchScreen utility methods and initial state.
 *
 * Tests the static formatSize() helper and verifies the screen's
 * initial state via public query API.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/tui/app.h"
#include "../src/tui/patch_screen.h"
#include "../src/core/database.h"

class TestPatchScreen : public QObject
{
    Q_OBJECT

private slots:

    // ── formatSize ────────────────────────────────────────

    void testFormatSizeBytes()
    {
        QCOMPARE(PatchScreen::formatSize(0),   std::string("0 B"));
        QCOMPARE(PatchScreen::formatSize(999), std::string("999 B"));
    }

    void testFormatSizeKilobytes()
    {
        std::string result = PatchScreen::formatSize(4096);
        QVERIFY(result.find("KB") != std::string::npos);
        QVERIFY(result.find("4.0") != std::string::npos);
    }

    void testFormatSizeMegabytes()
    {
        std::string result = PatchScreen::formatSize(5 * 1024 * 1024);
        QVERIFY(result.find("MB") != std::string::npos);
    }

    void testFormatSizeLargeValue()
    {
        // PatchScreen::formatSize caps at MB range
        std::string result = PatchScreen::formatSize(3LL * 1024 * 1024 * 1024);
        QVERIFY(result.find("MB") != std::string::npos);
    }

    void testFormatSizeNegative()
    {
        QCOMPARE(PatchScreen::formatSize(-1), std::string("?"));
    }

    // ── Initial state ─────────────────────────────────────

    void testInitialStateEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/patch.db"));

        PatchScreen screen(app);

        QCOMPARE(screen.patchCount(), 0);
        QVERIFY(!screen.isRunning());
        QVERIFY(screen.createBackup());  // default is true
    }

    // ── PatchEntry struct defaults ────────────────────────

    void testPatchEntryDefaults()
    {
        PatchScreen::PatchEntry pe;
        QVERIFY(pe.path.empty());
        QVERIFY(pe.filename.empty());
        QVERIFY(pe.formatName.empty());
        QCOMPARE(pe.sizeBytes, static_cast<int64_t>(0));
        QVERIFY(pe.checked);    // default true
        QVERIFY(pe.valid);      // default true
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestPatchScreen t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_patch_screen.moc"
