#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "../src/ui/controllers/processing_controller.h"
#include "../src/core/database.h"
#include "../src/metadata/provider_orchestrator.h"

using namespace Remus;

class ProcessingControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitialStateIsIdle();
    void testOptionSettersAndGetters();
    void testArtworkBasePath();
    void testStartProcessingEmitsStartedSignal();
    void testCancelProcessingTransitionsToIdle();
    void testPauseAndResume();
    void testGetProcessingStatsInitial();
    void testGetPendingFilesEmpty();
    void testStartProcessingWithRealFile();

private:
    // Build a minimal controller backed by an in-memory DB.
    struct Fixture {
        Database             db;
        ProviderOrchestrator orchestrator;
        ProcessingController controller;

        Fixture() : controller(&db, &orchestrator) {
            db.initialize(":memory:");
        }
    };

    // Write a small NES ROM file and insert it into the DB; return file ID.
    static int makeRomFile(const QTemporaryDir &dir, Database &db,
                            const QString &name = "game.nes")
    {
        const QString path = dir.path() + "/" + name;
        { QFile f(path); f.open(QIODevice::WriteOnly); f.write(QByteArray(16, 0xAA)); }

        int libId = db.insertLibrary(dir.path(), "Lib");
        int sysId = db.getSystemId("NES");

        FileRecord fr;
        fr.libraryId    = libId;
        fr.filename     = name;
        fr.originalPath = path;
        fr.currentPath  = path;
        fr.extension    = ".nes";
        fr.systemId     = sysId;
        fr.fileSize     = 16;
        return db.insertFile(fr);
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────

void ProcessingControllerTest::testInitialStateIsIdle()
{
    Fixture f;
    QVERIFY(!f.controller.isProcessing());
    QVERIFY(!f.controller.isPaused());
    QCOMPARE(f.controller.currentFileIndex(), 0);
    QCOMPARE(f.controller.totalFiles(), 0);
}

void ProcessingControllerTest::testOptionSettersAndGetters()
{
    Fixture f;

    f.controller.setConvertToChd(true);
    QVERIFY(f.controller.convertToChd());
    f.controller.setConvertToChd(false);
    QVERIFY(!f.controller.convertToChd());

    f.controller.setDownloadArtwork(false);
    QVERIFY(!f.controller.downloadArtwork());
    f.controller.setDownloadArtwork(true);
    QVERIFY(f.controller.downloadArtwork());

    f.controller.setFetchMetadata(false);
    QVERIFY(!f.controller.fetchMetadata());
    f.controller.setFetchMetadata(true);
    QVERIFY(f.controller.fetchMetadata());
}

void ProcessingControllerTest::testArtworkBasePath()
{
    Fixture f;
    QVERIFY(f.controller.artworkBasePath().isEmpty());

    f.controller.setArtworkBasePath("/home/user/.cache/remus/artwork");
    QCOMPARE(f.controller.artworkBasePath(), QStringLiteral("/home/user/.cache/remus/artwork"));
}

void ProcessingControllerTest::testStartProcessingEmitsStartedSignal()
{
    Fixture f;
    QSignalSpy spy(&f.controller, &ProcessingController::processingStarted);

    f.controller.startProcessing({1, 2, 3});

    // processingStarted is emitted synchronously or on the first timer tick
    if (spy.isEmpty()) {
        QVERIFY(spy.wait(1000));
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toInt(), 3);  // fileCount

    f.controller.cancelProcessing();
}

void ProcessingControllerTest::testCancelProcessingTransitionsToIdle()
{
    Fixture f;
    QSignalSpy cancelledSpy(&f.controller, &ProcessingController::processingCancelled);

    f.controller.startProcessing({1});
    f.controller.cancelProcessing();

    if (cancelledSpy.isEmpty()) {
        QVERIFY(cancelledSpy.wait(1000));
    }
    QVERIFY(!f.controller.isProcessing());
}

void ProcessingControllerTest::testPauseAndResume()
{
    Fixture f;
    QSignalSpy pausedSpy(&f.controller, &ProcessingController::pausedChanged);

    f.controller.startProcessing({1, 2});

    f.controller.pauseProcessing();
    if (pausedSpy.isEmpty()) {
        QVERIFY(pausedSpy.wait(500));
    }
    QVERIFY(f.controller.isPaused());

    f.controller.resumeProcessing();
    if (pausedSpy.count() < 2) {
        QVERIFY(pausedSpy.wait(500));
    }
    QVERIFY(!f.controller.isPaused());

    f.controller.cancelProcessing();
}

void ProcessingControllerTest::testGetProcessingStatsInitial()
{
    Fixture f;
    QVariantMap stats = f.controller.getProcessingStats();

    // Keys must be present with zero initial values
    QVERIFY(stats.contains("total"));
    QVERIFY(stats.contains("success"));
    QVERIFY(stats.contains("failed"));
    QCOMPARE(stats["total"].toInt(), 0);
}

void ProcessingControllerTest::testGetPendingFilesEmpty()
{
    Fixture f;
    QVariantList pending = f.controller.getPendingFiles();
    QVERIFY(pending.isEmpty());
}

void ProcessingControllerTest::testStartProcessingWithRealFile()
{
    // Integration-level: start pipeline on a real hashed file and wait for
    // at least one step signal. This exercises the pipeline dispatch loop.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Fixture f;
    int fileId = makeRomFile(dir, f.db);

    QSignalSpy fileStarted(&f.controller, &ProcessingController::fileStarted);
    QSignalSpy completed(&f.controller, &ProcessingController::processingCompleted);

    // Disable network-dependent steps so the test is hermetic
    f.controller.setFetchMetadata(false);
    f.controller.setDownloadArtwork(false);
    f.controller.setConvertToChd(false);

    f.controller.startProcessing({fileId});

    if (completed.isEmpty()) {
        QVERIFY(completed.wait(5000));
    }

    QCOMPARE(fileStarted.count(), 1);
    QCOMPARE(fileStarted.first().first().toInt(), fileId);
    QVERIFY(!f.controller.isProcessing());
}

QTEST_MAIN(ProcessingControllerTest)
#include "test_processing_controller.moc"
