#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "archive_creator.h"
#include "archive_extractor.h"

using namespace Remus;

class ArchiveCreatorTest : public QObject {
    Q_OBJECT

private slots:
    void testCompressSuccessReturnsOutputPath();
    void testCompressFailureReturnsError();
    void testBatchCompressResultCount();
    void testCanCompressQueryWithFakePaths();
    void testCompressDirectoryContentsPreservesRelativePaths();
    void testCompressMixedInputFilesFromDifferentDirectories();
    void testCompressContinuesBelowFailureThreshold();
    void testCompressFailsAtOneToThreeFailureRatio();
    void testRoundTripZip();
};

// ─────────────────────────────────────────────────────────────────────────────

void ArchiveCreatorTest::testCompressSuccessReturnsOutputPath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Write a small file to compress
    const QString inputFile = tmp.filePath("input.bin");
    {
        QFile f(inputFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(128, 'A'));
    }

    ArchiveCreator creator;
    const QString outputPath = tmp.filePath("out.zip");
    const CompressionResult result = creator.compress({inputFile}, outputPath, ArchiveFormat::ZIP);

    QVERIFY(result.success);
    QCOMPARE(result.outputPath, outputPath);
    QVERIFY(QFileInfo::exists(outputPath));
    QCOMPARE(result.filesCompressed, 1);
}

void ArchiveCreatorTest::testCompressFailureReturnsError()
{
    ArchiveCreator creator;
    // Use an empty output path — should fail validation
    const CompressionResult result = creator.compress({"/nonexistent/path.bin"}, QString(), ArchiveFormat::ZIP);

    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
}

void ArchiveCreatorTest::testBatchCompressResultCount()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create two source directories, each containing one file
    for (const QString &name : {QStringLiteral("dir1"), QStringLiteral("dir2")}) {
        const QString dirPath = tmp.filePath(name);
        QDir().mkpath(dirPath);
        QFile f(dirPath + "/file.bin");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
    }

    ArchiveCreator creator;
    const QString outputDir = tmp.filePath("archives");
    const QList<CompressionResult> results = creator.batchCompress(
        {tmp.filePath("dir1"), tmp.filePath("dir2")},
        outputDir, ArchiveFormat::ZIP);

    QCOMPARE(results.size(), 2);
    QVERIFY(results[0].success);
    QVERIFY(results[1].success);
}

void ArchiveCreatorTest::testCanCompressQueryWithFakePaths()
{
    ArchiveCreator creator;
    QVERIFY(creator.canCompress(ArchiveFormat::ZIP));
    QVERIFY(!creator.canCompress(ArchiveFormat::SevenZip));
    QVERIFY(!creator.canCompress(ArchiveFormat::RAR));
    QVERIFY(!creator.canCompress(ArchiveFormat::Unknown));
}

void ArchiveCreatorTest::testCompressDirectoryContentsPreservesRelativePaths()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create a directory tree
    const QString rootDir = tmp.filePath("srcdir");
    QDir().mkpath(rootDir + "/sub");
    {
        QFile f1(rootDir + "/top.txt");
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write("top");

        QFile f2(rootDir + "/sub/nested.txt");
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write("nested");
    }

    ArchiveCreator creator;
    const QString outputPath = tmp.filePath("result.zip");
    const CompressionResult result = creator.compressDirectoryContents(rootDir, outputPath, ArchiveFormat::ZIP);

    QVERIFY(result.success);

    // Verify archive contents using ArchiveExtractor::getArchiveInfo
    ArchiveExtractor extractor;
    const ArchiveInfo info = extractor.getArchiveInfo(outputPath);
    QCOMPARE(info.fileCount, 2);
    QVERIFY(info.contents.contains(QStringLiteral("top.txt")));
    QVERIFY(info.contents.contains(QStringLiteral("sub/nested.txt")));
}

void ArchiveCreatorTest::testCompressMixedInputFilesFromDifferentDirectories()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString dirA = tmp.filePath("dir_a");
    const QString dirB = tmp.filePath("dir_b");
    QDir().mkpath(dirA);
    QDir().mkpath(dirB);

    const QString fileA = dirA + "/alpha.bin";
    const QString fileB = dirB + "/beta.bin";
    {
        QFile alpha(fileA);
        QVERIFY(alpha.open(QIODevice::WriteOnly));
        alpha.write("alpha");

        QFile beta(fileB);
        QVERIFY(beta.open(QIODevice::WriteOnly));
        beta.write("beta");
    }

    ArchiveCreator creator;
    const QString outputPath = tmp.filePath("multi.zip");
    const CompressionResult result = creator.compress({fileA, fileB}, outputPath, ArchiveFormat::ZIP);

    QVERIFY(result.success);
    QCOMPARE(result.filesCompressed, 2);

    ArchiveExtractor extractor;
    const ArchiveInfo info = extractor.getArchiveInfo(outputPath);
    QCOMPARE(info.fileCount, 2);
    QVERIFY(info.contents.contains(QStringLiteral("alpha.bin")));
    QVERIFY(info.contents.contains(QStringLiteral("beta.bin")));
}

void ArchiveCreatorTest::testCompressContinuesBelowFailureThreshold()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString validFile = tmp.filePath("valid.bin");
    {
        QFile file(validFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("valid");
    }

    ArchiveCreator creator;
    const QString outputPath = tmp.filePath("partial.zip");
    const CompressionResult result = creator.compress(
        {validFile, tmp.filePath("missing1.bin"), tmp.filePath("missing2.bin")},
        outputPath,
        ArchiveFormat::ZIP);

    QVERIFY(result.success);
    QCOMPARE(result.filesCompressed, 1);
    QCOMPARE(result.failedFiles, 2);

    ArchiveExtractor extractor;
    const ArchiveInfo info = extractor.getArchiveInfo(outputPath);
    QCOMPARE(info.fileCount, 1);
    QVERIFY(info.contents.contains(QStringLiteral("valid.bin")));
}

void ArchiveCreatorTest::testCompressFailsAtOneToThreeFailureRatio()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString validFile = tmp.filePath("valid.bin");
    {
        QFile file(validFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("valid");
    }

    ArchiveCreator creator;
    const QString outputPath = tmp.filePath("ratio.zip");
    const CompressionResult result = creator.compress(
        {
            validFile,
            tmp.filePath("missing1.bin"),
            tmp.filePath("missing2.bin"),
            tmp.filePath("missing3.bin")
        },
        outputPath,
        ArchiveFormat::ZIP);

    QVERIFY(!result.success);
    QCOMPARE(result.filesCompressed, 1);
    QCOMPARE(result.failedFiles, 3);
}

void ArchiveCreatorTest::testRoundTripZip()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create source file
    const QByteArray payload(256, 'Z');
    const QString srcFile = tmp.filePath("data.bin");
    {
        QFile f(srcFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(payload);
    }

    // Compress
    ArchiveCreator creator;
    const QString archivePath = tmp.filePath("roundtrip.zip");
    const CompressionResult compResult = creator.compress({srcFile}, archivePath, ArchiveFormat::ZIP);
    QVERIFY(compResult.success);

    // Extract
    ArchiveExtractor extractor;
    const QString outDir = tmp.filePath("extracted");
    const ExtractionResult exResult = extractor.extract(archivePath, outDir, false);
    QVERIFY(exResult.success);

    // Verify extracted file content
    QFile out(outDir + "/data.bin");
    QVERIFY(out.open(QIODevice::ReadOnly));
    QCOMPARE(out.readAll(), payload);
}

QTEST_MAIN(ArchiveCreatorTest)
#include "test_archive_creator.moc"
