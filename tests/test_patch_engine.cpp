#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/patch_engine.h"

using namespace Remus;

class PatchEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void testFormatDetection();
    void testApplyInvalidPatch();
    void testApplyIpsBuiltin();
    void testApplyMissingBase();
    void testCreatePatchUnsupported();
};

void PatchEngineTest::testFormatDetection()
{
    QCOMPARE(PatchEngine::formatFromExtension("ips"), PatchFormat::IPS);
    QCOMPARE(PatchEngine::formatFromExtension(".bps"), PatchFormat::BPS);
    QCOMPARE(PatchEngine::formatFromExtension(".ups"), PatchFormat::UPS);
    QCOMPARE(PatchEngine::formatFromExtension(".xdelta"), PatchFormat::XDelta3);
    QCOMPARE(PatchEngine::formatFromExtension(".ppf"), PatchFormat::PPF);
    QCOMPARE(PatchEngine::formatFromExtension("unknown"), PatchFormat::Unknown);

    QCOMPARE(PatchEngine::formatName(PatchFormat::IPS), QStringLiteral("IPS"));
    QCOMPARE(PatchEngine::formatName(PatchFormat::Unknown), QStringLiteral("Unknown"));
}

void PatchEngineTest::testApplyInvalidPatch()
{
    PatchEngine engine;
    PatchInfo info;
    info.valid = false;
    info.error = "bad";

    PatchResult result = engine.apply("/no/base", info, "");
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Invalid patch"));
}

void PatchEngineTest::testApplyIpsBuiltin()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString basePath = dir.path() + "/base.rom";
    const QString patchPath = dir.path() + "/patch.ips";

    QFile baseFile(basePath);
    QVERIFY(baseFile.open(QIODevice::WriteOnly));
    baseFile.write(QByteArray(4, '\x00'));
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
    patchFile.write(patch);
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

void PatchEngineTest::testApplyMissingBase()
{
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

void PatchEngineTest::testCreatePatchUnsupported()
{
    PatchEngine engine;
    QVERIFY(!engine.createPatch("a", "b", "c", PatchFormat::PPF));
}

QTEST_MAIN(PatchEngineTest)
#include "test_patch_engine.moc"
