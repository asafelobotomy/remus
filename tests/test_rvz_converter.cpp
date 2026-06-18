#include <QtTest/QtTest>

#include "../src/core/rvz_converter.h"

using namespace Remus;

class FakeRvzConverter : public RVZConverter {
public:
    struct FakeProcessResult {
        bool started = false;
        int exitCode = -1;
        QString stdOutput;
        QString stdError;
    };

    FakeProcessResult nextVerify;

protected:
    ProcessResult runProcess(const QString &program, const QStringList &args, int timeoutMs) override {
        Q_UNUSED(program)
        Q_UNUSED(args)
        Q_UNUSED(timeoutMs)
        ProcessResult result;
        result.started = nextVerify.started;
        result.exitCode = nextVerify.exitCode;
        result.stdOutput = nextVerify.stdOutput;
        result.stdError = nextVerify.stdError;
        return result;
    }
};

class RvzConverterTest : public QObject {
    Q_OBJECT

private slots:
    void construction_doesNotCrash();
    void isDolphinToolAvailable_noToolInPath();
    void setCompression_doesNotCrash();
    void setCompressionLevel_doesNotCrash();
    void setDolphinToolPath_doesNotCrash();
    void discContentSha1_parsesAlgorithmOnlyOutput();
};

void RvzConverterTest::construction_doesNotCrash() {
    RVZConverter converter;
    Q_UNUSED(converter)
}

void RvzConverterTest::isDolphinToolAvailable_noToolInPath() {
    RVZConverter converter;
    // dolphin-tool is not in the CI environment; availability should be false.
    converter.setDolphinToolPath(QStringLiteral("/nonexistent/dolphin-tool"));
    QVERIFY(!converter.isDolphinToolAvailable());
}

void RvzConverterTest::setCompression_doesNotCrash() {
    RVZConverter converter;
    converter.setCompression(RVZCompression::Zstd);
    converter.setCompression(RVZCompression::Bzip2);
    converter.setCompression(RVZCompression::LZMA);
    converter.setCompression(RVZCompression::LZMA2);
    converter.setCompression(RVZCompression::None);
    converter.setCompression(RVZCompression::Auto);
}

void RvzConverterTest::setCompressionLevel_doesNotCrash() {
    RVZConverter converter;
    converter.setCompressionLevel(1);
    converter.setCompressionLevel(5);
    converter.setCompressionLevel(9);
}

void RvzConverterTest::setDolphinToolPath_doesNotCrash() {
    RVZConverter converter;
    converter.setDolphinToolPath(QStringLiteral("/usr/bin/dolphin-tool"));
    converter.setDolphinToolPath(QStringLiteral("dolphin-tool"));
}

void RvzConverterTest::discContentSha1_parsesAlgorithmOnlyOutput() {
    FakeRvzConverter converter;
    converter.nextVerify.started = true;
    converter.nextVerify.exitCode = 0;
    converter.nextVerify.stdOutput = QStringLiteral("f2439bbe1ff64133050fbc00574be8478210a958\n");

    QCOMPARE(converter.discContentSha1(QStringLiteral("/tmp/test.rvz")),
        QStringLiteral("f2439bbe1ff64133050fbc00574be8478210a958"));
}

QTEST_MAIN(RvzConverterTest)
#include "test_rvz_converter.moc"
