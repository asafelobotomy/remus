#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/archive_extractor.h"

using namespace Remus;

class FakeArchiveExtractor : public ArchiveExtractor
{
public:
    using ProcessResult = ExternalToolRunner::ProcessResult;

    QList<ProcessResult> queuedResults;
    QStringList fakeFiles;
    QString lastProgram;
    QStringList lastArgs;

    void enqueueResult(const ProcessResult &result)
    {
        queuedResults.append(result);
    }

protected:
    ProcessResult runProcess(const QString &program, const QStringList &args, int) override
    {
        lastProgram = program;
        lastArgs = args;
        if (queuedResults.isEmpty()) {
            return {};
        }

        const ProcessResult result = queuedResults.takeFirst();
        return result;
    }

    ProcessResult runProcessTracked(const QString &program, const QStringList &args, int) override
    {
        lastProgram = program;
        lastArgs = args;
        if (queuedResults.isEmpty()) {
            return {};
        }

        const ProcessResult result = queuedResults.takeFirst();
        return result;
    }

    QStringList listFiles(const QString &dirPath) const override
    {
        QStringList result;
        for (const QString &f : fakeFiles)
            result.append(QDir(dirPath).absoluteFilePath(f));
        return result;
    }
};

using FakeProcessResult = FakeArchiveExtractor::ProcessResult;

class ArchiveExtractorTest : public QObject
{
    Q_OBJECT

private slots:
    void testDetectFormat();
    void testNormalizeArchiveMemberPath();
    void testToolAvailabilityReflectsConfiguredPaths();
    void testGetArchiveInfoZip();
    void testGetArchiveInfoZipViaSevenZipFallback();
    void testGetArchiveInfo7z();
    void testGetArchiveInfo7zWrappedFilename();
    void testGetArchiveInfo7zNoDateEntries();
    void testGetArchiveInfoRar();
    void testExtractZip();
    void testExtract7zCreatesSubfolderAndTracksFiles();
    void testExtractRarFallsBackToSevenZip();
    void testExtractFileZipReturnsBasenameInOutputDir();
    void testExtractRejectsUnsafeArchiveEntries();
    void testBatchExtractCanBeCancelledAfterFirstItem();
    void testExtractUnsupported();
};

void ArchiveExtractorTest::testDetectFormat()
{
    QCOMPARE(ArchiveExtractor::detectFormat("file.zip"), ArchiveFormat::ZIP);
    QCOMPARE(ArchiveExtractor::detectFormat("file.7z"), ArchiveFormat::SevenZip);
    QCOMPARE(ArchiveExtractor::detectFormat("file.rar"), ArchiveFormat::RAR);
    QCOMPARE(ArchiveExtractor::detectFormat("file.tgz"), ArchiveFormat::TarGz);
    QCOMPARE(ArchiveExtractor::detectFormat("file.tar.gz"), ArchiveFormat::GZip);
    QCOMPARE(ArchiveExtractor::detectFormat("file.unknown"), ArchiveFormat::Unknown);
}

void ArchiveExtractorTest::testNormalizeArchiveMemberPath()
{
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath(QStringLiteral("roms/game.nes")),
             QStringLiteral("roms/game.nes"));
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath(QStringLiteral("nested\\game.nes")),
             QStringLiteral("nested/game.nes"));
    QVERIFY(ArchiveExtractor::normalizeArchiveMemberPath(QStringLiteral("../game.nes")).isEmpty());
    QVERIFY(ArchiveExtractor::normalizeArchiveMemberPath(QStringLiteral("/etc/passwd")).isEmpty());
}

void ArchiveExtractorTest::testToolAvailabilityReflectsConfiguredPaths()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    for (int i = 0; i < 24; ++i) {
        extractor.enqueueResult(versionResult);
    }

    extractor.setUnzipPath("/bin/sh");
    extractor.setSevenZipPath("/bin/sh");
    extractor.setUnrarPath("/bin/sh");

    const QMap<ArchiveFormat, bool> tools = extractor.getAvailableTools();
    QVERIFY(tools.value(ArchiveFormat::ZIP));
    QVERIFY(tools.value(ArchiveFormat::SevenZip));
    QVERIFY(tools.value(ArchiveFormat::RAR));
    QVERIFY(tools.value(ArchiveFormat::GZip));

    QVERIFY(extractor.canExtract(ArchiveFormat::ZIP));
    QVERIFY(extractor.canExtract(QStringLiteral("game.zip")));
    QVERIFY(extractor.canExtract(QStringLiteral("game.7z")));
    QVERIFY(extractor.canExtract(QStringLiteral("game.rar")));
    QVERIFY(!extractor.canExtract(QStringLiteral("game.unknown")));
}

void ArchiveExtractorTest::testGetArchiveInfoZip()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult result;
    result.started = true;
    result.exitCode = 0;
    result.stdOutput =
        "Archive: test.zip\n"
        "  Length      Date    Time    Name\n"
        "---------  ---------- -----   ----\n"
        "   10  2020-01-01 00:00   file1.bin\n"
        "---------                     -------\n"
        "   10                     1 file\n";
    extractor.enqueueResult(result);
    extractor.setUnzipPath("/bin/sh");
    extractor.setSevenZipPath(QString());

    ArchiveInfo info = extractor.getArchiveInfo("test.zip");
    QCOMPARE(info.format, ArchiveFormat::ZIP);
    QCOMPARE(info.fileCount, 1);
    QCOMPARE(info.contents.first(), QStringLiteral("file1.bin"));
    QCOMPARE(info.entrySizes.value(QStringLiteral("file1.bin")), static_cast<qint64>(10));
    QCOMPARE(info.uncompressedSize, static_cast<qint64>(10));
}

void ArchiveExtractorTest::testGetArchiveInfoZipViaSevenZipFallback()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult result;
    result.started = true;
    result.exitCode = 0;
    result.stdOutput =
        "7-Zip 26.00 (x64)\n"
        " 64-bit locale=en_US.UTF-8 Threads:4 OPEN_MAX:524288, ASM\n"
        "\n"
        "Scanning the drive for archives:\n"
        "1 file, 812128 bytes (794 KiB)\n"
        "\n"
        "Listing archive: test.zip\n"
        "\n"
        "--\n"
        "Path = test.zip\n"
        "Type = zip\n"
        "Physical Size = 812128\n"
        "\n"
        "   Date      Time    Attr         Size   Compressed  Name\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "2026-02-05 18:40:00 .....       812000       400000  nested/file.nes\n"
        "2026-02-05 18:40:00 .....          128           64  .remus.md\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "2026-02-05 18:40:00              812128       400064  2 files\n";
    extractor.enqueueResult(result);

    extractor.setUnzipPath(QString());
    extractor.setSevenZipPath("/bin/sh");

    ArchiveInfo info = extractor.getArchiveInfo("test.zip");
    QCOMPARE(info.format, ArchiveFormat::ZIP);
    QCOMPARE(info.fileCount, 2);
    QCOMPARE(info.contents.at(0), QStringLiteral("nested/file.nes"));
    QCOMPARE(info.contents.at(1), QStringLiteral(".remus.md"));
    QCOMPARE(info.entrySizes.value(QStringLiteral("nested/file.nes")), static_cast<qint64>(812000));
    QCOMPARE(info.entrySizes.value(QStringLiteral(".remus.md")), static_cast<qint64>(128));
    QCOMPARE(info.uncompressedSize, static_cast<qint64>(812128));
    QCOMPARE(extractor.lastProgram, QStringLiteral("/bin/sh"));
    QCOMPARE(extractor.lastArgs, QStringList({QStringLiteral("l"), QStringLiteral("test.zip")}));
}

void ArchiveExtractorTest::testGetArchiveInfo7z()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult result;
    result.started = true;
    result.exitCode = 0;
    result.stdOutput =
        "   Date      Time    Attr         Size   Compressed  Name\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "2026-02-05 18:40:00 .....       812000       400000  file.nes\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "2026-02-05 18:40:00              812000       400000  1 files\n";
    extractor.enqueueResult(result);

    ArchiveInfo info = extractor.getArchiveInfo("test.7z");
    QCOMPARE(info.format, ArchiveFormat::SevenZip);
    QCOMPARE(info.fileCount, 1);
    QCOMPARE(info.contents.first(), QStringLiteral("file.nes"));
    QCOMPARE(info.entrySizes.value(QStringLiteral("file.nes")), static_cast<qint64>(812000));
    QCOMPARE(info.uncompressedSize, static_cast<qint64>(812000));
}

void ArchiveExtractorTest::testGetArchiveInfo7zWrappedFilename()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult result;
    result.started = true;
    result.exitCode = 0;
    // Simulate real 7z output with a filename that wraps to a second line.
    // The word "Time" in the game title previously triggered a false-positive
    // header filter (contains("Time")).
    result.stdOutput =
        "7-Zip [64] 17.05 : Copyright (c) 1999-2021 Igor Pavlov : 2017-08-28\n"
        "\n"
        "Path = test.7z\n"
        "Type = 7z\n"
        "Physical Size = 23009755\n"
        "Headers Size = 258\n"
        "Method = LZMA2:48m\n"
        "Solid = +\n"
        "Blocks = 1\n"
        "\n"
        "   Date      Time    Attr         Size   Compressed  Name\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "2020-07-21 16:10:39 ....A          112     23009497  CDRomance.url\n"
        "2017-06-30 14:59:08 ....A     33554432               Legend of Zelda, The - Ocar\n"
        "ina of Time (USA) (Rev 2).z64\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "2020-07-21 16:10:39           33554544     23009497  2 files\n";
    extractor.enqueueResult(result);

    ArchiveInfo info = extractor.getArchiveInfo("test.7z");
    QCOMPARE(info.format, ArchiveFormat::SevenZip);
    QCOMPARE(info.fileCount, 2);

    // CDRomance.url should be parsed
    QVERIFY(info.contents.contains(QStringLiteral("CDRomance.url")));
    QCOMPARE(info.entrySizes.value(QStringLiteral("CDRomance.url")), static_cast<qint64>(112));

    // The wrapped filename must be reassembled correctly
    const QString expectedName = QStringLiteral("Legend of Zelda, The - Ocarina of Time (USA) (Rev 2).z64");
    QVERIFY2(info.contents.contains(expectedName),
             qPrintable("Expected '" + expectedName + "' in contents: " + info.contents.join(", ")));
    QCOMPARE(info.entrySizes.value(expectedName), static_cast<qint64>(33554432));
}

void ArchiveExtractorTest::testGetArchiveInfo7zNoDateEntries()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult result;
    result.started = true;
    result.exitCode = 0;
    // Some 7z archives have entries without date/time fields.
    // Also tests that WARNINGS metadata doesn't interfere.
    result.stdOutput =
        "7-Zip [64] 17.05 : Copyright (c) 1999-2021 Igor Pavlov : 2017-08-28\n"
        "\n"
        "Path = test.7z\n"
        "Type = 7z\n"
        "WARNINGS:\n"
        "There are data after the end of archive\n"
        "Physical Size = 252257749\n"
        "Tail Size = 38\n"
        "Headers Size = 176\n"
        "Method = LZMA:96m\n"
        "Solid = +\n"
        "Blocks = 1\n"
        "\n"
        "   Date      Time    Attr         Size   Compressed  Name\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "                    .....    616494480    252257573  Silent Hill (USA).bin\n"
        "                    .....           83               Silent Hill (USA).cue\n"
        "------------------- ----- ------------ ------------  ------------------------\n"
        "                             616494563    252257573  2 files\n"
        "\n"
        "Warnings: 1\n";
    extractor.enqueueResult(result);

    ArchiveInfo info = extractor.getArchiveInfo("test.7z");
    QCOMPARE(info.format, ArchiveFormat::SevenZip);
    QCOMPARE(info.fileCount, 2);
    QVERIFY(info.contents.contains(QStringLiteral("Silent Hill (USA).bin")));
    QVERIFY(info.contents.contains(QStringLiteral("Silent Hill (USA).cue")));
    QCOMPARE(info.entrySizes.value(QStringLiteral("Silent Hill (USA).bin")), static_cast<qint64>(616494480));
    QCOMPARE(info.entrySizes.value(QStringLiteral("Silent Hill (USA).cue")), static_cast<qint64>(83));
}

void ArchiveExtractorTest::testGetArchiveInfoRar()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult result;
    result.started = true;
    result.exitCode = 0;
    result.stdOutput =
        "Name             Size   Packed Ratio  Date    Time   Attr CRC\n"
        "file.nes        812000  400000  49%  02-05-26 18:40  -rw- 12AB34CD\n";
    extractor.enqueueResult(result);
    extractor.setUnrarPath("/bin/sh");
    extractor.setSevenZipPath(QString());

    ArchiveInfo info = extractor.getArchiveInfo("test.rar");
    QCOMPARE(info.format, ArchiveFormat::RAR);
    QCOMPARE(info.fileCount, 1);
    QCOMPARE(info.contents.first(), QStringLiteral("file.nes"));
    QCOMPARE(info.entrySizes.value(QStringLiteral("file.nes")), static_cast<qint64>(812000));
    QCOMPARE(info.uncompressedSize, static_cast<qint64>(812000));
}

void ArchiveExtractorTest::testExtractZip()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);
    extractor.enqueueResult(versionResult);

    FakeProcessResult listResult;
    listResult.started = true;
    listResult.exitCode = 0;
    listResult.stdOutput =
        "Archive: test.zip\n"
        "  Length      Date    Time    Name\n"
        "---------  ---------- -----   ----\n"
        "   10  2020-01-01 00:00   a.bin\n"
        "   10  2020-01-01 00:00   b.bin\n"
        "---------                     -------\n"
        "   20                     2 files\n";
    extractor.enqueueResult(listResult);

    FakeProcessResult extractResult;
    extractResult.started = true;
    extractResult.exitCode = 0;
    extractResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(extractResult);
    extractor.fakeFiles = {"a.bin", "b.bin"};
    extractor.setUnzipPath("/bin/sh");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString archivePath = dir.path() + "/test.zip";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QVERIFY(archive.write("zip") == 3);
    archive.close();

    ExtractionResult result = extractor.extract(archivePath, dir.path(), false);
    QVERIFY(result.success);
    QCOMPARE(result.filesExtracted, 2);
    QCOMPARE(extractor.lastProgram, QStringLiteral("/bin/sh"));
    QCOMPARE(extractor.lastArgs, QStringList({QStringLiteral("-o"), archivePath, QStringLiteral("-d"), dir.path()}));
}

void ArchiveExtractorTest::testExtract7zCreatesSubfolderAndTracksFiles()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);
    extractor.enqueueResult(versionResult);
    extractor.enqueueResult(versionResult);

    FakeProcessResult listResult;
    listResult.started = true;
    listResult.exitCode = 0;
    listResult.stdOutput =
        "2026-02-05 18:40  .....       812000       400000  disc.chd\n"
        "2026-02-05 18:40  .....          128           64  .remus.md\n";
    extractor.enqueueResult(listResult);

    FakeProcessResult extractResult;
    extractResult.started = true;
    extractResult.exitCode = 0;
    extractResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(extractResult);
    extractor.fakeFiles = {"disc.chd", ".remus.md"};
    extractor.setSevenZipPath("/bin/sh");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString archivePath = dir.path() + "/bundle.7z";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QVERIFY(archive.write("7z") == 2);
    archive.close();

    ExtractionResult result = extractor.extract(archivePath, dir.path(), true);
    QVERIFY(result.success);
    QCOMPARE(result.outputDir, dir.path() + "/bundle");
    QCOMPARE(result.filesExtracted, 2);
    QVERIFY(result.extractedFiles.contains(result.outputDir + "/disc.chd"));
    QVERIFY(result.extractedFiles.contains(result.outputDir + "/.remus.md"));
    QCOMPARE(extractor.lastArgs, QStringList({QStringLiteral("x"), archivePath, QStringLiteral("-o") + result.outputDir, QStringLiteral("-y")}));
}

void ArchiveExtractorTest::testExtractRarFallsBackToSevenZip()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult listResult;
    listResult.started = true;
    listResult.exitCode = 0;
    listResult.stdOutput =
        "Name             Size   Packed Ratio  Date    Time   Attr CRC\n"
        "file.nes        812000  400000  49%  02-05-26 18:40  -rw- 12AB34CD\n";
    extractor.enqueueResult(listResult);

    extractor.enqueueResult(versionResult);

    FakeProcessResult extractResult;
    extractResult.started = true;
    extractResult.exitCode = 0;
    extractResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(extractResult);
    extractor.fakeFiles = {"file.nes"};
    extractor.setUnrarPath(QString());
    extractor.setSevenZipPath("/bin/sh");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString archivePath = dir.path() + "/test.rar";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QVERIFY(archive.write("rar") == 3);
    archive.close();

    ExtractionResult result = extractor.extract(archivePath, dir.path(), false);
    QVERIFY(result.success);
    QCOMPARE(result.filesExtracted, 1);
    QCOMPARE(extractor.lastArgs, QStringList({QStringLiteral("x"), archivePath, QStringLiteral("-o") + dir.path(), QStringLiteral("-y")}));
}

void ArchiveExtractorTest::testExtractFileZipReturnsBasenameInOutputDir()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult extractResult;
    extractResult.started = true;
    extractResult.exitCode = 0;
    extractResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(extractResult);
    extractor.setUnzipPath("/bin/sh");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString archivePath = dir.path() + "/test.zip";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QVERIFY(archive.write("zip") == 3);
    archive.close();

    ExtractionResult result = extractor.extractFile(archivePath, "nested/file.bin", dir.path());

    // The extractFile code verifies the file exists; create it so the check passes.
    {
        QFile f(dir.path() + "/file.bin");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
    }
    extractor.enqueueResult(versionResult);
    extractor.enqueueResult(extractResult);
    result = extractor.extractFile(archivePath, "nested/file.bin", dir.path());

    QVERIFY(result.success);
    QCOMPARE(result.filesExtracted, 1);
    QCOMPARE(result.extractedFiles.first(), dir.path() + "/file.bin");
    QCOMPARE(extractor.lastArgs, QStringList({archivePath, QStringLiteral("nested/file.bin"), QStringLiteral("-d"), dir.path()}));
}

void ArchiveExtractorTest::testExtractRejectsUnsafeArchiveEntries()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);

    FakeProcessResult listResult;
    listResult.started = true;
    listResult.exitCode = 0;
    listResult.stdOutput =
        "Archive: test.zip\n"
        "  Length      Date    Time    Name\n"
        "---------  ---------- -----   ----\n"
        "   10  2020-01-01 00:00   ../evil.bin\n"
        "---------                     -------\n"
        "   10                     1 file\n";
    extractor.enqueueResult(listResult);
    extractor.setUnzipPath("/bin/sh");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString archivePath = dir.path() + "/test.zip";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QVERIFY(archive.write("zip") == 3);
    archive.close();

    const ExtractionResult result = extractor.extract(archivePath, dir.path(), false);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("unsafe path entries")));
}

void ArchiveExtractorTest::testBatchExtractCanBeCancelledAfterFirstItem()
{
    FakeArchiveExtractor extractor;
    FakeProcessResult versionResult;
    versionResult.started = true;
    versionResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(versionResult);
    extractor.enqueueResult(versionResult);

    FakeProcessResult listResult;
    listResult.started = true;
    listResult.exitCode = 0;
    listResult.stdOutput =
        "Archive: one.zip\n"
        "  Length      Date    Time    Name\n"
        "---------  ---------- -----   ----\n"
        "   10  2020-01-01 00:00   file.bin\n"
        "---------                     -------\n"
        "   10                     1 file\n";
    extractor.enqueueResult(listResult);

    FakeProcessResult extractResult;
    extractResult.started = true;
    extractResult.exitCode = 0;
    extractResult.exitStatus = QProcess::NormalExit;
    extractor.enqueueResult(extractResult);
    extractor.fakeFiles = {"file.bin"};
    extractor.setUnzipPath("/bin/sh");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString firstArchive = dir.path() + "/one.zip";
    const QString secondArchive = dir.path() + "/two.zip";
    for (const QString &archivePath : {firstArchive, secondArchive}) {
        QFile archive(archivePath);
        QVERIFY(archive.open(QIODevice::WriteOnly));
        QVERIFY(archive.write("zip") == 3);
        archive.close();
    }

    QObject::connect(&extractor, &ArchiveExtractor::batchProgress, &extractor,
                     [&extractor](int completed, int) {
                         if (completed == 1) {
                             extractor.cancel();
                         }
                     });

    const QList<ExtractionResult> results = extractor.batchExtract({firstArchive, secondArchive}, dir.path(), true);
    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().success);
}

void ArchiveExtractorTest::testExtractUnsupported()
{
    FakeArchiveExtractor extractor;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString archivePath = dir.path() + "/test.unknown";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    QVERIFY(archive.write("data") == 4);
    archive.close();

    ExtractionResult result = extractor.extract(archivePath, dir.path(), false);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Unsupported archive format"));
}

QTEST_MAIN(ArchiveExtractorTest)
#include "test_archive_extractor.moc"
