#include <QtTest/QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "../src/core/disc_converter.h"
#include "../src/core/external_tool_runner.h"

using namespace Remus;

namespace {

class TestDiscConverter : public DiscConverter {
public:
    using DiscConverter::DiscConverter;
    using DiscConverter::getDefaultOutputPath;
    using DiscConverter::getFileSize;
    using DiscConverter::runToolConversion;

    ExternalToolRunner::ProcessResult nextTracked;
    QString outputPathToCreate;

protected:
    ExternalToolRunner::ProcessResult runProcessTracked(const QString &program, const QStringList &args, int) override {
        Q_UNUSED(program);
        Q_UNUSED(args);

        if (!outputPathToCreate.isEmpty()) {
            QFile outputFile(outputPathToCreate);
            if (outputFile.open(QIODevice::WriteOnly)) {
                outputFile.write(QByteArray(100, 'x'));
            }
        }

        return nextTracked;
    }
};

} // namespace

class DiscConverterTest : public QObject {
    Q_OBJECT

private slots:
    void getDefaultOutputPath_replacesExtension();
    void getFileSize_reportsExistingFile();
    void runToolConversion_successPopulatesResultAndSignals();
    void runToolConversion_processStartFailureSetsError();
    void cancelEmitsConversionCancelled();
};

void DiscConverterTest::getDefaultOutputPath_replacesExtension() {
    TestDiscConverter converter;
    QCOMPARE(converter.getDefaultOutputPath(QStringLiteral("/tmp/disc/game.iso"), QStringLiteral("cso")),
        QStringLiteral("/tmp/disc/game.cso"));
}

void DiscConverterTest::getFileSize_reportsExistingFile() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString path = tempDir.filePath(QStringLiteral("sample.bin"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArray(12, 'a')) == 12);
    file.close();

    TestDiscConverter converter;
    QCOMPARE(converter.getFileSize(path), 12);
    QCOMPARE(converter.getFileSize(tempDir.filePath(QStringLiteral("missing.bin"))), 0);
}

void DiscConverterTest::runToolConversion_successPopulatesResultAndSignals() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inputPath = tempDir.filePath(QStringLiteral("input.iso"));
    const QString outputPath = tempDir.filePath(QStringLiteral("output.cso"));

    QFile inputFile(inputPath);
    QVERIFY(inputFile.open(QIODevice::WriteOnly));
    QVERIFY(inputFile.write(QByteArray(200, 'b')) == 200);
    inputFile.close();

    TestDiscConverter converter;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;
    converter.nextTracked.exitStatus = QProcess::NormalExit;
    converter.outputPathToCreate = outputPath;

    QSignalSpy startedSpy(&converter, &DiscConverter::conversionStarted);
    QSignalSpy completedSpy(&converter, &DiscConverter::conversionCompleted);

    const ConversionResult result = converter.runToolConversion(QStringLiteral("/bin/fake-tool"),
        { QStringLiteral("--convert") }, QStringLiteral("Fake Tool"), inputPath, outputPath);

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(completedSpy.count(), 1);
    QVERIFY(result.success);
    QCOMPARE(result.inputPath, inputPath);
    QCOMPARE(result.outputPath, outputPath);
    QCOMPARE(result.inputSize, 200);
    QCOMPARE(result.outputSize, 100);
    QCOMPARE(result.compressionRatio, 0.5);
}

void DiscConverterTest::runToolConversion_processStartFailureSetsError() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString inputPath = tempDir.filePath(QStringLiteral("input.iso"));
    const QString outputPath = tempDir.filePath(QStringLiteral("output.cso"));

    TestDiscConverter converter;
    converter.nextTracked.started = false;

    QSignalSpy errorSpy(&converter, &DiscConverter::errorOccurred);
    const ConversionResult result = converter.runToolConversion(QStringLiteral("/bin/missing-tool"),
        { QStringLiteral("--convert") }, QStringLiteral("Missing Tool"), inputPath, outputPath);

    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("Failed to start")));
    QCOMPARE(errorSpy.count(), 1);
}

void DiscConverterTest::cancelEmitsConversionCancelled() {
    TestDiscConverter converter;
    QSignalSpy cancelledSpy(&converter, &DiscConverter::conversionCancelled);
    converter.cancel();
    QCOMPARE(cancelledSpy.count(), 1);
}

QTEST_MAIN(DiscConverterTest)

#include "test_disc_converter.moc"
