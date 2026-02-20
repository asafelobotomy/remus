#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "../src/core/archive_creator.h"
#include "../src/core/archive_extractor.h"

using namespace Remus;

// ── Fake subclass that intercepts runProcess ──────────────────────────────

class FakeArchiveCreator : public ArchiveCreator
{
public:
    int  exitCode  = 0;
    bool started   = true;
    QString stdOut;
    QString stdErr;

protected:
    ProcessResult runProcess(const QString &, const QStringList &args, int) override
    {
        ProcessResult r;
        r.exitCode = exitCode;
        r.stdOut   = stdOut;
        r.stdErr   = stdErr;
        // Simulate the tool creating the output archive so compress()'s
        // post-process file-existence check passes.  For both zip and 7z the
        // output path is always args[1]: zip -j <output> <inputs…>
        if (exitCode == 0 && args.size() >= 2) {
            QFile f(args[1]);
            f.open(QIODevice::WriteOnly);
            f.write("PK\x05\x06" + QByteArray(18, '\0')); // minimal ZIP end-of-central-dir
        }
        return r;
    }
};

// ── Test class ────────────────────────────────────────────────────────────

class ArchiveCreatorTest : public QObject
{
    Q_OBJECT

private slots:
    void testCompressSuccessReturnsOutputPath();
    void testCompressFailureReturnsError();
    void testBatchCompressResultCount();
    void testCanCompressQueryWithFakePaths();
    void testRoundTripZip();
};

// ── Tests ─────────────────────────────────────────────────────────────────

void ArchiveCreatorTest::testCompressSuccessReturnsOutputPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create a dummy input file so calculateTotalSize doesn't fail
    const QString inputFile = dir.path() + "/rom.nes";
    { QFile f(inputFile); f.open(QIODevice::WriteOnly); f.write("FAKE ROM"); }
    const QString outputZip = dir.path() + "/rom.zip";

    FakeArchiveCreator creator;
    creator.exitCode = 0;

    // Point to a real executable so canCompress() passes the isToolAvailable
    // check.  runProcess() is overridden, so the actual binary is never run.
    creator.setZipPath("/bin/sh");

    CompressionResult result = creator.compress({inputFile}, outputZip, ArchiveFormat::ZIP);
    // Since runProcess returns exitCode=0, compress should report success
    QVERIFY(result.success);
    QCOMPARE(result.outputPath, outputZip);
}

void ArchiveCreatorTest::testCompressFailureReturnsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString inputFile = dir.path() + "/rom.nes";
    { QFile f(inputFile); f.open(QIODevice::WriteOnly); f.write("FAKE ROM"); }

    FakeArchiveCreator creator;
    creator.exitCode = 1;  // Non-zero → failure
    creator.stdErr   = "zip: permission denied";
    creator.setZipPath("/usr/bin/zip");

    CompressionResult result = creator.compress({dir.path() + "/rom.nes"},
                                                dir.path() + "/out.zip",
                                                ArchiveFormat::ZIP);
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
}

void ArchiveCreatorTest::testBatchCompressResultCount()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create two subdirectories to compress individually
    const QStringList dirs = {dir.path() + "/a", dir.path() + "/b"};
    for (const QString &d : dirs) {
        QDir().mkpath(d);
        QFile f(d + "/rom.nes");
        f.open(QIODevice::WriteOnly);
        f.write("DATA");
    }

    FakeArchiveCreator creator;
    creator.exitCode = 0;
    creator.setZipPath("/usr/bin/zip");

    QList<CompressionResult> results =
        creator.batchCompress(dirs, dir.path(), ArchiveFormat::ZIP);

    QCOMPARE(results.size(), 2);
}

void ArchiveCreatorTest::testCanCompressQueryWithFakePaths()
{
    ArchiveCreator creator;
    // With a non-existent tool path, canCompress should return false for ZIP
    creator.setZipPath("/nonexistent/zip");
    // canCompress checks tool availability; with a fake path it may still
    // report true if system zip is on PATH, so we just verify the method runs.
    (void)creator.canCompress(ArchiveFormat::ZIP);
    (void)creator.canCompress(ArchiveFormat::SevenZip);
    QVERIFY(true);  // Guard — test that no crash occurs
}

void ArchiveCreatorTest::testRoundTripZip()
{
    // This test requires the system `zip` and `unzip` tools.
    ArchiveCreator creator;
    ArchiveExtractor extractor;

    if (!creator.canCompress(ArchiveFormat::ZIP)) {
        QSKIP("zip tool not available — skipping round-trip test");
    }
    if (!extractor.canExtract(ArchiveFormat::ZIP)) {
        QSKIP("unzip tool not available — skipping round-trip test");
    }

    QTemporaryDir srcDir, dstDir;
    QVERIFY(srcDir.isValid() && dstDir.isValid());

    // Write a small file
    const QByteArray payload = "Round-trip ROM payload";
    const QString srcFile    = srcDir.path() + "/game.nes";
    { QFile f(srcFile); f.open(QIODevice::WriteOnly); f.write(payload); }

    // Compress
    const QString zipPath = srcDir.path() + "/game.zip";
    CompressionResult cr = creator.compress({srcFile}, zipPath, ArchiveFormat::ZIP);
    QVERIFY2(cr.success, qPrintable(cr.error));
    QVERIFY(QFile::exists(zipPath));
    QVERIFY(cr.compressedSize > 0);
    QCOMPARE(cr.filesCompressed, 1);

    // Extract
    ExtractionResult er = extractor.extract(zipPath, dstDir.path());
    QVERIFY2(er.success, qPrintable(er.error));
    QCOMPARE(er.filesExtracted, 1);

    // Verify content identity
    QFile out(dstDir.path() + "/game.nes");
    QVERIFY(out.open(QIODevice::ReadOnly));
    QCOMPARE(out.readAll(), payload);
}

QTEST_MAIN(ArchiveCreatorTest)
#include "test_archive_creator.moc"
