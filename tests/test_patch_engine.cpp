#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/patch_engine.h"

using namespace Remus;

class PatchEngineTest : public QObject {
    Q_OBJECT

private slots:
    void testFormatDetection();
    void testDetectUpsChecksums();
    void testApplyInvalidPatch();
    void testApplyIpsBuiltin();
    void testApplyMissingBase();
    void testCreatePatchUnsupported();
};

void PatchEngineTest::testFormatDetection() {
    QCOMPARE(PatchEngine::formatFromExtension("ips"), PatchFormat::IPS);
    QCOMPARE(PatchEngine::formatFromExtension(".bps"), PatchFormat::BPS);
    QCOMPARE(PatchEngine::formatFromExtension(".ups"), PatchFormat::UPS);
    QCOMPARE(PatchEngine::formatFromExtension(".xdelta"), PatchFormat::XDelta3);
    QCOMPARE(PatchEngine::formatFromExtension(".ppf"), PatchFormat::PPF);
    QCOMPARE(PatchEngine::formatFromExtension("unknown"), PatchFormat::Unknown);

    QCOMPARE(PatchEngine::formatName(PatchFormat::IPS), QStringLiteral("IPS"));
    QCOMPARE(PatchEngine::formatName(PatchFormat::Unknown), QStringLiteral("Unknown"));
}

void PatchEngineTest::testApplyInvalidPatch() {
    PatchEngine engine;
    PatchInfo info;
    info.valid = false;
    info.error = "bad";

    PatchResult result = engine.apply("/no/base", info, "");
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Invalid patch"));
}

void PatchEngineTest::testDetectUpsChecksums() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString patchPath = dir.path() + "/patch.ups";
    QByteArray patch;
    patch.append("UPS1");
    patch.append(QByteArray(8, '\x00'));
    patch.append(char(0x78));
    patch.append(char(0x56));
    patch.append(char(0x34));
    patch.append(char(0x12));
    patch.append(char(0xF0));
    patch.append(char(0xDE));
    patch.append(char(0xBC));
    patch.append(char(0x9A));
    patch.append(char(0xEF));
    patch.append(char(0xCD));
    patch.append(char(0xAB));
    patch.append(char(0x90));

    QFile patchFile(patchPath);
    QVERIFY(patchFile.open(QIODevice::WriteOnly));
    QVERIFY(patchFile.write(patch) == patch.size());
    patchFile.close();

    PatchEngine engine;
    PatchInfo info = engine.detectFormat(patchPath);
    QVERIFY(info.valid);
    QCOMPARE(info.format, PatchFormat::UPS);
    QCOMPARE(info.sourceChecksum, QStringLiteral("12345678"));
    QCOMPARE(info.targetChecksum, QStringLiteral("9abcdef0"));
    QCOMPARE(info.patchChecksum, QStringLiteral("90abcdef"));
}

void PatchEngineTest::testApplyIpsBuiltin() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString basePath = dir.path() + "/base.rom";
    const QString patchPath = dir.path() + "/patch.ips";

    QFile baseFile(basePath);
    QVERIFY(baseFile.open(QIODevice::WriteOnly));
    QVERIFY(baseFile.write(QByteArray(4, '\x00')) == 4);
    baseFile.close();

    // IPS patch: PATCH + offset 0x000001 + size 0x0001 + byte 0x7F + EOF
    QByteArray patch;
    patch.append("PATCH");
    patch.append(char(0x00));
    patch.append(char(0x00));
    patch.append(char(0x01));
    patch.append(char(0x00));
    patch.append(char(0x01));
    patch.append(char(0x7F));
    patch.append("EOF");

    QFile patchFile(patchPath);
    QVERIFY(patchFile.open(QIODevice::WriteOnly));
    QVERIFY(patchFile.write(patch) == patch.size());
    patchFile.close();

    PatchEngine engine;
    PatchInfo info = engine.detectFormat(patchPath);
    QVERIFY(info.valid);

    PatchResult result = engine.apply(basePath, info, "");
    QVERIFY(result.success);

    QFile outFile(result.outputPath);
    QVERIFY(outFile.open(QIODevice::ReadOnly));
    QByteArray data = outFile.readAll();
    outFile.close();

    QCOMPARE(static_cast<unsigned char>(data[1]), 0x7F);
}

void PatchEngineTest::testApplyMissingBase() {
    PatchEngine engine;
    PatchInfo info;
    info.valid = true;
    info.format = PatchFormat::IPS;
    info.formatName = "IPS";
    info.path = "/tmp/patch.ips";

    PatchResult result = engine.apply("/no/base", info, "/tmp/out.rom");
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Base ROM file not found"));
}

void PatchEngineTest::testCreatePatchUnsupported() {
    PatchEngine engine;
    QVERIFY(!engine.createPatch("a", "b", "c", PatchFormat::PPF));
}

QTEST_MAIN(PatchEngineTest)
#include "test_patch_engine.moc"
