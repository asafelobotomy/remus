#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSettings>
#include "../src/cli/interactive_session.h"

using namespace Remus::Cli;

class SessionStateTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("Remus");
        QCoreApplication::setApplicationName("CLIStateTest");
        QSettings settings("Remus", "CLI");
        settings.clear();
    }

    void testRoundTrip() {
        SessionState state;
        state.lastScanPath = "/tmp/scan";
        state.lastDoHash = false;
        state.lastDoMatch = true;
        state.lastDoOrganize = true;
        state.lastOrganizeDest = "/tmp/out";
        state.lastDryRun = true;
        state.lastChdInput = "/tmp/game.cue";
        state.lastChdOutputDir = "/tmp/chd";
        state.lastChdCodec = "lzma";
        state.lastArchivePath = "/tmp/archive.zip";
        state.lastArchiveOut = "/tmp/extract";
        state.lastPatchBase = "base.bin";
        state.lastPatchFile = "patch.bps";
        state.lastPatchOutput = "out.bin";
        state.lastPatchOriginal = "orig.bin";
        state.lastPatchModified = "mod.bin";
        state.lastPatchFormat = "xdelta";
        state.lastExportFormat = "json";
        state.lastExportPath = "/tmp/export.json";
        state.lastExportSystems = "nes,snes";
        state.lastExportDryRun = false;
        state.lastTemplate = "{Title} - {Region}";

        InteractiveSession::saveStateSnapshot(state);
        SessionState loaded = InteractiveSession::loadStateSnapshot();

        QCOMPARE(loaded.lastScanPath, state.lastScanPath);
        QCOMPARE(loaded.lastDoHash, state.lastDoHash);
        QCOMPARE(loaded.lastDoMatch, state.lastDoMatch);
        QCOMPARE(loaded.lastDoOrganize, state.lastDoOrganize);
        QCOMPARE(loaded.lastOrganizeDest, state.lastOrganizeDest);
        QCOMPARE(loaded.lastDryRun, state.lastDryRun);
        QCOMPARE(loaded.lastChdInput, state.lastChdInput);
        QCOMPARE(loaded.lastChdOutputDir, state.lastChdOutputDir);
        QCOMPARE(loaded.lastChdCodec, state.lastChdCodec);
        QCOMPARE(loaded.lastArchivePath, state.lastArchivePath);
        QCOMPARE(loaded.lastArchiveOut, state.lastArchiveOut);
        QCOMPARE(loaded.lastPatchBase, state.lastPatchBase);
        QCOMPARE(loaded.lastPatchFile, state.lastPatchFile);
        QCOMPARE(loaded.lastPatchOutput, state.lastPatchOutput);
        QCOMPARE(loaded.lastPatchOriginal, state.lastPatchOriginal);
        QCOMPARE(loaded.lastPatchModified, state.lastPatchModified);
        QCOMPARE(loaded.lastPatchFormat, state.lastPatchFormat);
        QCOMPARE(loaded.lastExportFormat, state.lastExportFormat);
        QCOMPARE(loaded.lastExportPath, state.lastExportPath);
        QCOMPARE(loaded.lastExportSystems, state.lastExportSystems);
        QCOMPARE(loaded.lastExportDryRun, state.lastExportDryRun);
        QCOMPARE(loaded.lastTemplate, state.lastTemplate);
    }

    void cleanupTestCase() {
        QSettings("Remus", "CLI").clear();
    }
};

QTEST_MAIN(SessionStateTest)
#include "test_session_state.moc"
