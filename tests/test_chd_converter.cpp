#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/chd_converter.h"

using namespace Remus;

class FakeChdConverter : public CHDConverter
{
public:
    ProcessResult nextProcess;
    ProcessResult nextTracked;

protected:
    ProcessResult runProcess(const QString &, const QStringList &, int) override
    {
        return nextProcess;
    }

    ProcessResult runProcessTracked(const QString &, const QStringList &, int) override
    {
        return nextTracked;
    }
};

class ChdConverterTest : public QObject
{
    Q_OBJECT

private slots:
    void testAvailabilityAndVersion();
    void testVerifyCHD();
    void testGetCHDInfo();
    void testConvertIso();
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

    CHDVerifyResult ok = converter.verifyCHD("/tmp/test.chd");
    QVERIFY(ok.valid);
    QCOMPARE(ok.details, QStringLiteral("verified"));

    converter.nextProcess.exitCode = 1;
    converter.nextProcess.stdError = "bad";
    CHDVerifyResult bad = converter.verifyCHD("/tmp/test.chd");
    QVERIFY(!bad.valid);
    QCOMPARE(bad.error, QStringLiteral("bad"));
}

void ChdConverterTest::testGetCHDInfo()
{
    FakeChdConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.exitCode = 0;
    converter.nextProcess.stdOutput =
        "CHD version: 5\n"
        "Logical size: 1234\n"
        "SHA1: abcdef\n"
        "Compression: lzma\n";

    CHDInfo info = converter.getCHDInfo("/tmp/test.chd");
    QCOMPARE(info.version, 5);
    QCOMPARE(info.logicalSize, 1234);
    QCOMPARE(info.sha1, QStringLiteral("abcdef"));
    QCOMPARE(info.compression, QStringLiteral("lzma"));
}

void ChdConverterTest::testConvertIso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString inputPath = dir.path() + "/test.iso";
    QString outputPath = dir.path() + "/test.chd";

    QFile inputFile(inputPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    inputFile.write("data");
    inputFile.close();

    QFile outputFile(outputPath);
    QVERIFY(outputFile.open(QIODevice::WriteOnly));
    outputFile.close();

    FakeChdConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;

    CHDConversionResult ok = converter.convertIsoToCHD(inputPath, outputPath);
    QVERIFY(ok.success);

    QFile::remove(outputPath);
    converter.nextTracked.exitCode = 1;
    CHDConversionResult bad = converter.convertIsoToCHD(inputPath, outputPath);
    QVERIFY(!bad.success);
}

void ChdConverterTest::testBatchConvertUnsupported()
{
    FakeChdConverter converter;
    QStringList inputs = {"/tmp/file.txt"};

    QList<CHDConversionResult> results = converter.batchConvert(inputs);
    QCOMPARE(results.size(), 1);
    QVERIFY(!results.first().success);
    QVERIFY(results.first().error.contains("Unsupported format"));
}

QTEST_MAIN(ChdConverterTest)
#include "test_chd_converter.moc"
