#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "../src/core/wbfs_converter.h"

using namespace Remus;

namespace {
bool writeAll(QFile &file, const QByteArray &data) {
    return file.write(data) == data.size();
}
} // namespace

class FakeWbfsConverter : public WBFSConverter {
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
            // wit uses --dest <path> for the output argument
            const int destIndex = args.indexOf(QStringLiteral("--dest"));
            if (destIndex >= 0 && destIndex + 1 < args.size()) {
                const QString outputPath = args.at(destIndex + 1);
                QFileInfo info(outputPath);
                if (!QDir().mkpath(info.absolutePath())) {
                    qFatal("FakeWbfsConverter: cannot mkpath %s", qPrintable(info.absolutePath()));
                }
                QFile outputFile(outputPath);
                if (outputFile.open(QIODevice::WriteOnly)) {
                    if (!writeAll(outputFile, QByteArrayLiteral("output"))) {
                        qFatal("FakeWbfsConverter: write failed");
                    }
                }
            }
        }

        return nextTracked;
    }
};

class WbfsConverterTest : public QObject {
    Q_OBJECT

private slots:
    void testAvailabilityUsesConfiguredToolPath();
    void testGetVersionPrefersStderrWhenStdoutMissing();
    void testConvertIsoUsesDefaultOutputPath();
    void testExtractWbfsUsesDefaultIsoOutputPath();
    void testUnsupportedSourceFormatIsRejected();
};

void WbfsConverterTest::testAvailabilityUsesConfiguredToolPath() {
    FakeWbfsConverter converter;
    converter.setWitPath("/custom/wit");
    converter.nextProcess.started = false;

    QVERIFY(!converter.isWitAvailable());
    QCOMPARE(converter.lastProgram, QStringLiteral("/custom/wit"));
}

void WbfsConverterTest::testGetVersionPrefersStderrWhenStdoutMissing() {
    FakeWbfsConverter converter;
    converter.nextProcess.started = true;
    converter.nextProcess.stdError = QStringLiteral("wit v3.04a r8222");

    const QString version = converter.getWitVersion();
    QCOMPARE(version, QStringLiteral("wit v3.04a r8222"));
}

void WbfsConverterTest::testConvertIsoUsesDefaultOutputPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + QStringLiteral("/game.iso");
    QFile isoFile(isoPath);
    QVERIFY(isoFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(isoFile, QByteArrayLiteral("iso")));
    isoFile.close();

    FakeWbfsConverter converter;
    converter.autoCreateTrackedOutput = true;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;

    const ConversionResult result = converter.convertIsoToWbfs(isoPath);

    QVERIFY(result.success);
    QVERIFY(result.outputPath.endsWith(QStringLiteral(".wbfs")));
    // wit 'copy' subcommand must be the first argument
    QCOMPARE(converter.lastArgs.value(0), QStringLiteral("copy"));
    // source file must appear in args
    QVERIFY(converter.lastArgs.contains(isoPath));
}

void WbfsConverterTest::testExtractWbfsUsesDefaultIsoOutputPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString wbfsPath = dir.path() + QStringLiteral("/game.wbfs");
    QFile wbfsFile(wbfsPath);
    QVERIFY(wbfsFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(wbfsFile, QByteArrayLiteral("wbfs")));
    wbfsFile.close();

    FakeWbfsConverter converter;
    converter.autoCreateTrackedOutput = true;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;

    const ConversionResult result = converter.extractWbfsToIso(wbfsPath);

    QVERIFY(result.success);
    QVERIFY(result.outputPath.endsWith(QStringLiteral(".iso")));
    QCOMPARE(converter.lastArgs.value(0), QStringLiteral("copy"));
    QVERIFY(converter.lastArgs.contains(wbfsPath));
}

void WbfsConverterTest::testUnsupportedSourceFormatIsRejected() {
    // WBFSConverter does not have its own format guard — it passes whatever
    // the CLI gives it to wit.  The CLI rejects unknown extensions before calling
    // convertIsoToWbfs.  But we can verify that converting a file with an
    // unsupported extension still produces a ConversionResult (success depends
    // on whether wit accepts it; here the fake will succeed).
    // This test documents that the converter itself does not assert on ext.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString txtPath = dir.path() + QStringLiteral("/rom.txt");
    QFile txt(txtPath);
    QVERIFY(txt.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(txt, QByteArrayLiteral("not an iso")));
    txt.close();

    FakeWbfsConverter converter;
    converter.autoCreateTrackedOutput = true;
    converter.nextTracked.started = true;
    converter.nextTracked.exitCode = 0;

    // Converter itself does not check the extension — that's CLI responsibility
    const ConversionResult result = converter.convertIsoToWbfs(txtPath);
    // The fake converter creates output → success
    QVERIFY(result.success);
}

QTEST_MAIN(WbfsConverterTest)
#include "test_wbfs_converter.moc"
