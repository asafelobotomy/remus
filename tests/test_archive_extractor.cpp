#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/archive_extractor.h"

using namespace Remus;

class FakeArchiveExtractor : public ArchiveExtractor
{
public:
    ProcessResult nextResult;
    QStringList fakeFiles;

protected:
    ProcessResult runProcess(const QString &, const QStringList &, int) override
    {
        return nextResult;
    }

    ProcessResult runProcessTracked(const QString &, const QStringList &, int) override
    {
        return nextResult;
    }

    QStringList listFiles(const QString &) const override
    {
        return fakeFiles;
    }
};

class ArchiveExtractorTest : public QObject
{
    Q_OBJECT

private slots:
    void testDetectFormat();
    void testGetArchiveInfoZip();
    void testGetArchiveInfo7z();
    void testGetArchiveInfoRar();
    void testExtractZip();
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

void ArchiveExtractorTest::testGetArchiveInfoZip()
{
    FakeArchiveExtractor extractor;
    extractor.nextResult.started = true;
    extractor.nextResult.exitCode = 0;
    extractor.nextResult.stdOutput =
        "Archive: test.zip\n"
        "  Length      Date    Time    Name\n"
        "---------  ---------- -----   ----\n"
        "   10  2020-01-01 00:00   file1.bin\n"
        "---------                     -------\n"
        "   10                     1 file\n";

    ArchiveInfo info = extractor.getArchiveInfo("test.zip");
    QCOMPARE(info.format, ArchiveFormat::ZIP);
    QCOMPARE(info.fileCount, 1);
    QCOMPARE(info.contents.first(), QStringLiteral("file1.bin"));
}

void ArchiveExtractorTest::testGetArchiveInfo7z()
{
    FakeArchiveExtractor extractor;
    extractor.nextResult.started = true;
    extractor.nextResult.exitCode = 0;
    extractor.nextResult.stdOutput =
        "2026-02-05 18:40  .....       812000       400000  file.nes\n";

    ArchiveInfo info = extractor.getArchiveInfo("test.7z");
    QCOMPARE(info.format, ArchiveFormat::SevenZip);
    QCOMPARE(info.fileCount, 1);
    QCOMPARE(info.contents.first(), QStringLiteral("file.nes"));
}

void ArchiveExtractorTest::testGetArchiveInfoRar()
{
    FakeArchiveExtractor extractor;
    extractor.nextResult.started = true;
    extractor.nextResult.exitCode = 0;
    extractor.nextResult.stdOutput =
        "Name             Size   Packed Ratio  Date    Time   Attr CRC\n"
        "file.nes        812000  400000  49%  02-05-26 18:40  -rw- 12AB34CD\n";

    ArchiveInfo info = extractor.getArchiveInfo("test.rar");
    QCOMPARE(info.format, ArchiveFormat::RAR);
    QCOMPARE(info.fileCount, 1);
    QCOMPARE(info.contents.first(), QStringLiteral("file.nes"));
}

void ArchiveExtractorTest::testExtractZip()
{
    FakeArchiveExtractor extractor;
    extractor.nextResult.started = true;
    extractor.nextResult.exitCode = 0;
    extractor.fakeFiles = {"a.bin", "b.bin"};

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString archivePath = dir.path() + "/test.zip";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    archive.write("zip");
    archive.close();

    ExtractionResult result = extractor.extract(archivePath, dir.path(), false);
    QVERIFY(result.success);
    QCOMPARE(result.filesExtracted, 2);
}

void ArchiveExtractorTest::testExtractUnsupported()
{
    FakeArchiveExtractor extractor;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString archivePath = dir.path() + "/test.unknown";
    QFile archive(archivePath);
    QVERIFY(archive.open(QIODevice::WriteOnly));
    archive.write("data");
    archive.close();

    ExtractionResult result = extractor.extract(archivePath, dir.path(), false);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Unsupported archive format"));
}

QTEST_MAIN(ArchiveExtractorTest)
#include "test_archive_extractor.moc"
