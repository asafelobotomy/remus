#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "../src/core/pbp_exporter.h"

using namespace Remus;

namespace {
bool writeAll(QFile &file, const QByteArray &data) {
    return file.write(data) == data.size();
}
} // namespace

class FakePBPExporter : public PBPExporter {
public:
    using ProcessResult = ExternalToolRunner::ProcessResult;
    ProcessResult nextProcess;
    ProcessResult nextTracked;
    QString lastProgram;
    QStringList lastArgs;
    bool autoCreateTrackedOutput = false;

protected:
    ProcessResult runProcess(const QString &program, const QStringList &args, int) override {
        lastProgram = program;
        lastArgs = args;
        return nextProcess;
    }

    ProcessResult runProcessTracked(const QString &program, const QStringList &args, int) override {
        lastProgram = program;
        lastArgs = args;

        if (autoCreateTrackedOutput) {
            // PSXPackager takes <input> <output> positionally
            if (args.size() >= 2) {
                const QString outputPath = args.last();
                QFileInfo info(outputPath);
                if (!QDir().mkpath(info.absolutePath())) {
                    qFatal("FakePBPExporter: cannot mkpath %s", qPrintable(info.absolutePath()));
                }
                QFile outputFile(outputPath);
                if (outputFile.open(QIODevice::WriteOnly)) {
                    if (!writeAll(outputFile, QByteArrayLiteral("pbp"))) {
                        qFatal("FakePBPExporter: write failed");
                    }
                }
            }
        }

        return nextTracked;
    }
};

class PBPExporterTest : public QObject {
    Q_OBJECT

private slots:
    void testAvailabilityUsesConfiguredToolPath();
    void testGetVersionPrefersStderrWhenStdoutMissing();
    void testExportCueUsesDefaultPBPOutputPath();
    void testExportIsoUsesDefaultPBPOutputPath();
    void testExportReturnsErrorWhenToolFails();
};

void PBPExporterTest::testAvailabilityUsesConfiguredToolPath() {
    FakePBPExporter exporter;
    exporter.setPSXPackagerPath("/usr/local/bin/PSXPackager");
    exporter.nextProcess.started = false;

    QVERIFY(!exporter.isPSXPackagerAvailable());
    QCOMPARE(exporter.lastProgram, QStringLiteral("/usr/local/bin/PSXPackager"));
}

void PBPExporterTest::testGetVersionPrefersStderrWhenStdoutMissing() {
    FakePBPExporter exporter;
    exporter.nextProcess.started = true;
    exporter.nextProcess.stdError = QStringLiteral("PSXPackager 1.2.0");

    const QString version = exporter.getPSXPackagerVersion();
    QCOMPARE(version, QStringLiteral("PSXPackager 1.2.0"));
}

void PBPExporterTest::testExportCueUsesDefaultPBPOutputPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString cuePath = dir.path() + QStringLiteral("/game.cue");
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(cueFile, QByteArrayLiteral("FILE \"game.bin\" BINARY\n")));
    cueFile.close();

    FakePBPExporter exporter;
    exporter.autoCreateTrackedOutput = true;
    exporter.nextTracked.started = true;
    exporter.nextTracked.exitCode = 0;

    const ConversionResult result = exporter.exportToPBP(cuePath);

    QVERIFY(result.success);
    QVERIFY(result.outputPath.endsWith(QStringLiteral(".pbp")));
    // Source path must be the first positional arg
    QVERIFY(exporter.lastArgs.contains(cuePath));
}

void PBPExporterTest::testExportIsoUsesDefaultPBPOutputPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + QStringLiteral("/game.iso");
    QFile isoFile(isoPath);
    QVERIFY(isoFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(isoFile, QByteArrayLiteral("iso")));
    isoFile.close();

    FakePBPExporter exporter;
    exporter.autoCreateTrackedOutput = true;
    exporter.nextTracked.started = true;
    exporter.nextTracked.exitCode = 0;

    const ConversionResult result = exporter.exportToPBP(isoPath);

    QVERIFY(result.success);
    QVERIFY(result.outputPath.endsWith(QStringLiteral(".pbp")));
}

void PBPExporterTest::testExportReturnsErrorWhenToolFails() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString cuePath = dir.path() + QStringLiteral("/game.cue");
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(cueFile, QByteArrayLiteral("FILE \"game.bin\" BINARY\n")));
    cueFile.close();

    FakePBPExporter exporter;
    exporter.autoCreateTrackedOutput = false;
    exporter.nextTracked.started = true;
    exporter.nextTracked.exitCode = 1;
    exporter.nextTracked.stdError = QStringLiteral("error: unsupported format");

    const ConversionResult result = exporter.exportToPBP(cuePath);

    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
}

QTEST_MAIN(PBPExporterTest)
#include "test_pbp_exporter.moc"
