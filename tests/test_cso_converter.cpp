#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "../src/core/cso_converter.h"

using namespace Remus;

namespace {
bool writeAll(QFile &file, const QByteArray &data)
{
    return file.write(data) == data.size();
}
}

class FakeCsoConverter : public CSOConverter
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
                if (!QDir().mkpath(info.absolutePath())) {
                    qFatal("FakeCsoConverter: cannot mkpath %s", qPrintable(info.absolutePath()));
                }
                QFile outputFile(outputPath);
                if (outputFile.open(QIODevice::WriteOnly)) {
                    if (!writeAll(outputFile, QByteArrayLiteral("output"))) {
                        qFatal("FakeCsoConverter: write failed");
                    }
                }
            }
        }

        return nextTracked;
    }
};

class CsoConverterTest : public QObject
{
    Q_OBJECT

private slots:
    void testAvailabilityUsesConfiguredToolPath();
    void testGetVersionPrefersStderrWhenStdoutMissing();
    void testConvertIsoUsesDefaultOutputPath();
    void testExtractCsoUsesDefaultIsoOutputPath();
    void testBatchConvertSupportedFormatsUsesOutputDirectory();
    void testBatchConvertUnsupportedFormat();
};

void CsoConverterTest::testAvailabilityUsesConfiguredToolPath()
{
    FakeCsoConverter converter;
    converter.setMaxcsoPath(QStringLiteral("/custom/maxcso"));
    converter.nextProcess.started = true;

    QVERIFY(converter.isMaxcsoAvailable());
    QCOMPARE(converter.lastProgram, QStringLiteral("/custom/maxcso"));
    const QStringList expectedArgs{QStringLiteral("--help")};
    QCOMPARE(converter.lastArgs, expectedArgs);
}

void CsoConverterTest::testGetVersionPrefersStderrWhenStdoutMissing()
{
    FakeCsoConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.stdError = QStringLiteral("maxcso 1.13.0\nusage");

    QCOMPARE(converter.getMaxcsoVersion(), QStringLiteral("maxcso 1.13.0"));
}

void CsoConverterTest::testConvertIsoUsesDefaultOutputPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + QStringLiteral("/game.iso");
    QFile inputFile(isoPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(inputFile, QByteArrayLiteral("iso-data")));
    inputFile.close();

    FakeCsoConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.autoCreateTrackedOutput = true;

    const ConversionResult result = converter.convertIsoToCSO(isoPath);
    QVERIFY(result.success);
    QCOMPARE(result.outputPath, dir.path() + QStringLiteral("/game.cso"));
    QCOMPARE(converter.lastProgram, QStringLiteral("maxcso"));
    const QStringList expectedArgs{
        isoPath,
        QStringLiteral("-o"),
        dir.path() + QStringLiteral("/game.cso")
    };
    QCOMPARE(converter.lastArgs, expectedArgs);
}

void CsoConverterTest::testExtractCsoUsesDefaultIsoOutputPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString csoPath = dir.path() + QStringLiteral("/game.cso");
    QFile inputFile(csoPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(inputFile, QByteArrayLiteral("cso-data")));
    inputFile.close();

    FakeCsoConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.autoCreateTrackedOutput = true;

    const ConversionResult result = converter.extractCSOToIso(csoPath);
    QVERIFY(result.success);
    QCOMPARE(result.outputPath, dir.path() + QStringLiteral("/game.iso"));
    const QStringList expectedArgs{
        QStringLiteral("--decompress"),
        csoPath,
        QStringLiteral("-o"),
        dir.path() + QStringLiteral("/game.iso")
    };
    QCOMPARE(converter.lastArgs, expectedArgs);
}

void CsoConverterTest::testBatchConvertSupportedFormatsUsesOutputDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + QStringLiteral("/batch.iso");
    const QString outputDir = dir.path() + QStringLiteral("/out");

    QFile isoFile(isoPath);
    QVERIFY(isoFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(isoFile, QByteArrayLiteral("iso")));
    isoFile.close();

    FakeCsoConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.autoCreateTrackedOutput = true;

    const QList<ConversionResult> results = converter.batchConvert({isoPath}, outputDir);
    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().success);
    QCOMPARE(results.first().outputPath, outputDir + QStringLiteral("/batch.cso"));
}

void CsoConverterTest::testBatchConvertUnsupportedFormat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString textPath = dir.path() + QStringLiteral("/note.txt");
    QFile textFile(textPath);
    QVERIFY(textFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(textFile, QByteArrayLiteral("not an iso")));
    textFile.close();

    FakeCsoConverter converter;

    const QList<ConversionResult> results = converter.batchConvert({textPath});
    QCOMPARE(results.size(), 1);
    QVERIFY(!results.first().success);
    QVERIFY(results.first().error.contains(QStringLiteral("Unsupported format")));
}

QTEST_MAIN(CsoConverterTest)
#include "test_cso_converter.moc"