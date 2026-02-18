/**
 * @file test_compressor_screen.cpp
 * @brief Unit tests for CompressorScreen utility methods and initial state.
 *
 * Tests the static helpers (detectFileType, fileTypeString, formatSize)
 * and verifies the screen's initial state via public query API.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/tui/app.h"
#include "../src/tui/compressor_screen.h"
#include "../src/core/database.h"

class TestCompressorScreen : public QObject
{
    Q_OBJECT

private slots:

    // ── detectFileType ────────────────────────────────────

    void testDetectFileTypeKnownExtensions()
    {
        using FT = CompressorScreen::FileType;

        QCOMPARE(CompressorScreen::detectFileType("game.cue"), FT::CUE);
        QCOMPARE(CompressorScreen::detectFileType("disc.iso"), FT::ISO);
        QCOMPARE(CompressorScreen::detectFileType("game.gdi"), FT::GDI);
        QCOMPARE(CompressorScreen::detectFileType("disc.chd"), FT::CHD);
        QCOMPARE(CompressorScreen::detectFileType("roms.zip"), FT::ZIP);
        QCOMPARE(CompressorScreen::detectFileType("roms.7z"),  FT::SevenZ);
        QCOMPARE(CompressorScreen::detectFileType("roms.rar"), FT::RAR);
    }

    void testDetectFileTypeCaseInsensitive()
    {
        using FT = CompressorScreen::FileType;

        QCOMPARE(CompressorScreen::detectFileType("GAME.CUE"), FT::CUE);
        QCOMPARE(CompressorScreen::detectFileType("Game.ISO"), FT::ISO);
        QCOMPARE(CompressorScreen::detectFileType("Archive.ZIP"), FT::ZIP);
    }

    void testDetectFileTypeUnknown()
    {
        using FT = CompressorScreen::FileType;

        QCOMPARE(CompressorScreen::detectFileType("readme.txt"), FT::Unknown);
        QCOMPARE(CompressorScreen::detectFileType("game.nes"),   FT::Unknown);
        QCOMPARE(CompressorScreen::detectFileType(""),           FT::Unknown);
    }

    // ── fileTypeString ────────────────────────────────────

    void testFileTypeStringAllTypes()
    {
        using FT = CompressorScreen::FileType;

        QCOMPARE(CompressorScreen::fileTypeString(FT::CUE),     std::string("BIN/CUE"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::ISO),     std::string("ISO"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::GDI),     std::string("GDI"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::CHD),     std::string("CHD"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::ZIP),     std::string("ZIP"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::SevenZ),  std::string("7z"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::RAR),     std::string("RAR"));
        QCOMPARE(CompressorScreen::fileTypeString(FT::Unknown), std::string("Unknown"));
    }

    // ── formatSize ────────────────────────────────────────

    void testFormatSizeBytes()
    {
        QCOMPARE(CompressorScreen::formatSize(0),   std::string("0 B"));
        QCOMPARE(CompressorScreen::formatSize(512), std::string("512 B"));
    }

    void testFormatSizeKilobytes()
    {
        std::string result = CompressorScreen::formatSize(2048);
        QVERIFY(result.find("KB") != std::string::npos);
        QVERIFY(result.find("2.0") != std::string::npos);
    }

    void testFormatSizeMegabytes()
    {
        std::string result = CompressorScreen::formatSize(10 * 1024 * 1024);
        QVERIFY(result.find("MB") != std::string::npos);
        QVERIFY(result.find("10.0") != std::string::npos);
    }

    void testFormatSizeGigabytes()
    {
        std::string result = CompressorScreen::formatSize(2LL * 1024 * 1024 * 1024);
        QVERIFY(result.find("GB") != std::string::npos);
    }

    void testFormatSizeNegative()
    {
        QCOMPARE(CompressorScreen::formatSize(-1), std::string("?"));
    }

    // ── Initial state ─────────────────────────────────────

    void testInitialStateEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/comp.db"));

        CompressorScreen screen(app);

        QCOMPARE(screen.fileCount(), 0);
        QVERIFY(!screen.isRunning());
        QCOMPARE(screen.mode(), CompressorScreen::OpMode::Compress);
        QVERIFY(!screen.deleteOriginals());
    }

    // ── detectFileType → fileTypeString round-trip ────────

    void testDetectAndStringRoundTrip()
    {
        auto ft = CompressorScreen::detectFileType("game.iso");
        QCOMPARE(CompressorScreen::fileTypeString(ft), std::string("ISO"));
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestCompressorScreen t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_compressor_screen.moc"
