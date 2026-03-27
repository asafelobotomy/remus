#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/chd_converter.h"

using namespace Remus;

namespace {
bool writeAll(QFile &file, const QByteArray &data)
{
    return file.write(data) == data.size();
}
}

class FakeChdConverter : public CHDConverter
{
public:
    ProcessResult nextProcess;
    ProcessResult nextTracked;
    QString lastProgram;
    QStringList lastArgs;
    bool autoCreateTrackedOutput = false;

protected:
    ProcessResult runProcess(const QString &program, const QStringList &args, int) override
    {
        lastProgram = program;
        lastArgs = args;
        return nextProcess;
    }

    ProcessResult runProcessTracked(const QString &program, const QStringList &args, int) override
    {
        lastProgram = program;
        lastArgs = args;

        if (autoCreateTrackedOutput) {
            const int outputIndex = args.indexOf(QStringLiteral("-o"));
            if (outputIndex >= 0 && outputIndex + 1 < args.size()) {
                const QString outputPath = args.at(outputIndex + 1);
                QFileInfo info(outputPath);
                if (!QDir().mkpath(info.absolutePath()))
                    qFatal("FakeChdConverter: cannot mkpath %s", qPrintable(info.absolutePath()));
                QFile outputFile(outputPath);
                if (outputFile.open(QIODevice::WriteOnly)) {
                    if (!writeAll(outputFile, QByteArrayLiteral("output")))
                        qFatal("FakeChdConverter: write failed");
                }
            }
        }

        return nextTracked;
    }
};

class ChdConverterTest : public QObject
{
    Q_OBJECT

private slots:
    void testAvailabilityAndVersion();
    void testVerifyCHD();
    void testVerifyCHDUsesGenericErrorWhenStderrMissing();
    void testGetCHDInfo();
    void testGetCHDInfoSupportsLegacyLabelsAndFailureDefaults();
    void testConvertIso();
    void testConvertCueAndGdiIncludeConfiguredArguments();
    void testExtractChdUsesDefaultCueOutputPath();
    void testBatchConvertSupportedFormatsUsesOutputDirectory();
    void testBatchConvertUnsupported();
};

void ChdConverterTest::testAvailabilityAndVersion()
{
    FakeChdConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.exitCode = 0;
    converter.nextProcess.exitStatus = QProcess::NormalExit;
    converter.nextProcess.stdOutput = "chdman 0.1\nhelp\n";

    QVERIFY(converter.isChdmanAvailable());
    QCOMPARE(converter.getChdmanVersion(), QStringLiteral("chdman 0.1"));
}

void ChdConverterTest::testVerifyCHD()
{
    FakeChdConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.exitCode = 0;
    converter.nextProcess.stdOutput = "verified";

    VerifyResult ok = converter.verifyCHD("/tmp/test.chd");
    QVERIFY(ok.valid);
    QCOMPARE(ok.details, QStringLiteral("verified"));

    converter.nextProcess.exitCode = 1;
    converter.nextProcess.stdError = "bad";
    VerifyResult bad = converter.verifyCHD("/tmp/test.chd");
    QVERIFY(!bad.valid);
    QCOMPARE(bad.error, QStringLiteral("bad"));
}

void ChdConverterTest::testVerifyCHDUsesGenericErrorWhenStderrMissing()
{
    FakeChdConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.exitCode = 2;

    VerifyResult result = converter.verifyCHD("/tmp/test.chd");
    QVERIFY(!result.valid);
    QCOMPARE(result.error, QStringLiteral("Verification failed"));
}

void ChdConverterTest::testGetCHDInfo()
{
    FakeChdConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.exitCode = 0;
    converter.nextProcess.stdOutput =
        "Input file:   /tmp/test.chd\n"
        "File Version: 5\n"
        "Logical size: 641,659,968 bytes\n"
        "Compression: cdlz (CD LZMA), cdzl (CD Deflate), cdfl (CD FLAC)\n"
        "CHD size:     307,388,422 bytes\n"
        "SHA1:         abcdef\n";

    CHDInfo info = converter.getCHDInfo("/tmp/test.chd");
    QCOMPARE(info.version, 5);
    QCOMPARE(info.logicalSize, 641659968);
    QCOMPARE(info.physicalSize, 307388422);
    QCOMPARE(info.sha1, QStringLiteral("abcdef"));
    QCOMPARE(info.compression, QStringLiteral("cdlz (CD LZMA), cdzl (CD Deflate), cdfl (CD FLAC)"));
}

void ChdConverterTest::testGetCHDInfoSupportsLegacyLabelsAndFailureDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString chdPath = dir.path() + "/legacy.chd";
    QFile inputFile(chdPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(inputFile, QByteArrayLiteral("legacy-data")));
    inputFile.close();

    FakeChdConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.exitCode = 0;
    converter.nextProcess.stdOutput =
        "CHD version: 4\n"
        "Logical size: 123,456 bytes\n"
        "Physical size: 78,901 bytes\n"
        "SHA1: deadbeef\n"
        "Compression: zlib\n";

    CHDInfo legacyInfo = converter.getCHDInfo(chdPath);
    QCOMPARE(legacyInfo.version, 4);
    QCOMPARE(legacyInfo.logicalSize, 123456);
    QCOMPARE(legacyInfo.physicalSize, 78901);
    QCOMPARE(legacyInfo.sha1, QStringLiteral("deadbeef"));
    QCOMPARE(legacyInfo.compression, QStringLiteral("zlib"));

    converter.nextProcess.started = false;
    converter.nextProcess.exitCode = -1;
    CHDInfo fallbackInfo = converter.getCHDInfo(chdPath);
    QCOMPARE(fallbackInfo.version, 0);
    QCOMPARE(fallbackInfo.logicalSize, 0);
    QCOMPARE(fallbackInfo.physicalSize, QFileInfo(chdPath).size());
    QVERIFY(fallbackInfo.sha1.isEmpty());
    QVERIFY(fallbackInfo.compression.isEmpty());
}

void ChdConverterTest::testConvertIso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString inputPath = dir.path() + "/test.iso";
    QString outputPath = dir.path() + "/test.chd";

    QFile inputFile(inputPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(inputFile, QByteArrayLiteral("data")));
    inputFile.close();

    QFile outputFile(outputPath);
    QVERIFY(outputFile.open(QIODevice::WriteOnly));
    outputFile.close();

    FakeChdConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;

    ConversionResult ok = converter.convertIsoToCHD(inputPath, outputPath);
    QVERIFY(ok.success);

    QVERIFY(QFile::remove(outputPath));
    converter.nextTracked.exitCode = 1;
    ConversionResult bad = converter.convertIsoToCHD(inputPath, outputPath);
    QVERIFY(!bad.success);
}

void ChdConverterTest::testConvertCueAndGdiIncludeConfiguredArguments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString cuePath = dir.path() + "/disc.cue";
    const QString binPath = dir.path() + "/disc.bin";
    const QString cueOutputPath = dir.path() + "/disc.chd";
    const QString gdiPath = dir.path() + "/disc.gdi";
    const QString gdiOutputPath = dir.path() + "/disc_gdi.chd";

    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(writeAll(cueFile, QByteArrayLiteral("FILE \"disc.bin\" BINARY\n")));
    cueFile.close();

    QFile binFile(binPath);
    QVERIFY(binFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(binFile, QByteArrayLiteral("bin-data")));
    binFile.close();

    QFile gdiFile(gdiPath);
    QVERIFY(gdiFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(writeAll(gdiFile, QByteArrayLiteral("1\n1 0 4 2352 track01.bin 0\n")));
    gdiFile.close();

    FakeChdConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.autoCreateTrackedOutput = true;
    converter.setCodec(CHDCodec::FLAC);
    converter.setNumProcessors(4);

    ConversionResult cueResult = converter.convertCueToCHD(cuePath, cueOutputPath);
    QVERIFY(cueResult.success);
    QCOMPARE(converter.lastProgram, QStringLiteral("chdman"));
    QCOMPARE(converter.lastArgs.value(0), QStringLiteral("createcd"));
    QCOMPARE(converter.lastArgs.value(2), cuePath);
    QCOMPARE(converter.lastArgs.value(4), cueOutputPath);
    QVERIFY(converter.lastArgs.contains(QStringLiteral("-c")));
    QVERIFY(converter.lastArgs.contains(QStringLiteral("flac")));
    QVERIFY(converter.lastArgs.contains(QStringLiteral("-np")));
    QVERIFY(converter.lastArgs.contains(QStringLiteral("4")));
    QCOMPARE(cueResult.inputSize, QFileInfo(cuePath).size() + QFileInfo(binPath).size());

    ConversionResult gdiResult = converter.convertGdiToCHD(gdiPath, gdiOutputPath);
    QVERIFY(gdiResult.success);
    QCOMPARE(converter.lastArgs.value(0), QStringLiteral("createcd"));
    QCOMPARE(converter.lastArgs.value(2), gdiPath);
    QCOMPARE(converter.lastArgs.value(4), gdiOutputPath);
}

void ChdConverterTest::testExtractChdUsesDefaultCueOutputPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString chdPath = dir.path() + "/disc.chd";
    QFile chdFile(chdPath);
    QVERIFY(chdFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(chdFile, QByteArrayLiteral("chd-data")));
    chdFile.close();

    FakeChdConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.autoCreateTrackedOutput = true;

    ConversionResult result = converter.extractCHDToCue(chdPath);
    QVERIFY(result.success);
    QCOMPARE(result.outputPath, dir.path() + "/disc.cue");
    QCOMPARE(converter.lastArgs.value(0), QStringLiteral("extractcd"));
    QCOMPARE(converter.lastArgs.value(2), chdPath);
    QCOMPARE(converter.lastArgs.value(4), dir.path() + "/disc.cue");
}

void ChdConverterTest::testBatchConvertSupportedFormatsUsesOutputDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString cuePath = dir.path() + "/batch.cue";
    const QString isoPath = dir.path() + "/batch.iso";
    const QString gdiPath = dir.path() + "/batch.gdi";
    const QString outputDir = dir.path() + "/out";

    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(writeAll(cueFile, QByteArrayLiteral("FILE \"batch.bin\" BINARY\n")));
    cueFile.close();

    QFile cueBinFile(dir.path() + "/batch.bin");
    QVERIFY(cueBinFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(cueBinFile, QByteArrayLiteral("bin")));
    cueBinFile.close();

    QFile isoFile(isoPath);
    QVERIFY(isoFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(isoFile, QByteArrayLiteral("iso")));
    isoFile.close();

    QFile gdiFile(gdiPath);
    QVERIFY(gdiFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(writeAll(gdiFile, QByteArrayLiteral("1\n1 0 4 2352 track01.bin 0\n")));
    gdiFile.close();

    FakeChdConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.autoCreateTrackedOutput = true;

    const QList<ConversionResult> results = converter.batchConvert({cuePath, isoPath, gdiPath}, outputDir);
    QCOMPARE(results.size(), 3);
    QVERIFY(results.at(0).success);
    QVERIFY(results.at(1).success);
    QVERIFY(results.at(2).success);
    QCOMPARE(results.at(0).outputPath, outputDir + "/batch.chd");
    QCOMPARE(results.at(1).outputPath, outputDir + "/batch.chd");
    QCOMPARE(results.at(2).outputPath, outputDir + "/batch.chd");
}

void ChdConverterTest::testBatchConvertUnsupported()
{
    FakeChdConverter converter;
    QStringList inputs = {"/tmp/file.txt"};

    QList<ConversionResult> results = converter.batchConvert(inputs);
    QCOMPARE(results.size(), 1);
    QVERIFY(!results.first().success);
    QVERIFY(results.first().error.contains("Unsupported format"));
}

QTEST_MAIN(ChdConverterTest)
#include "test_chd_converter.moc"
