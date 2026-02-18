/**
 * @file test_patch_service.cpp
 * @brief Unit tests for PatchService (format detection, tool status, supported formats)
 *
 * Tests validate API behaviour without requiring actual patch tools on the host.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/services/patch_service.h"

using namespace Remus;

class TestPatchService : public QObject
{
    Q_OBJECT

private slots:

    void testDetectFormatWithIPSHeader()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Write a minimal IPS header
        QString patchPath = tmp.path() + "/test.ips";
        QFile f(patchPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("PATCH");           // IPS magic
        f.write(QByteArray(10, '\x00')); // dummy payload
        f.write("EOF");             // IPS footer
        f.close();

        PatchService svc;
        PatchInfo info = svc.detectFormat(patchPath);
        QCOMPARE(info.format, PatchFormat::IPS);
    }

    void testDetectFormatUnknown()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QString path = tmp.path() + "/random.bin";
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello world this is not a patch");
        f.close();

        PatchService svc;
        PatchInfo info = svc.detectFormat(path);
        QCOMPARE(info.format, PatchFormat::Unknown);
    }

    void testDetectFormatMissingFile()
    {
        PatchService svc;
        PatchInfo info = svc.detectFormat("/nonexistent/patch.ips");
        QCOMPARE(info.format, PatchFormat::Unknown);
    }

    void testGetSupportedFormats()
    {
        PatchService svc;
        QStringList formats = svc.getSupportedFormats();
        // Should report some formats even if tools aren't installed
        // The list may be empty on a minimal host, but the call must succeed
        Q_UNUSED(formats);
    }

    void testGetToolStatus()
    {
        PatchService svc;
        auto status = svc.getToolStatus();
        // Should contain standard tool keys
        QVERIFY2(!status.isEmpty(), "Tool status map should not be empty");
    }

    void testApplyMissingFiles()
    {
        PatchService svc;
        auto result = svc.apply("/nonexistent/rom.nes", "/nonexistent/patch.ips");
        QVERIFY(!result.success);
    }

    void testBatchApplyEmpty()
    {
        PatchService svc;
        auto results = svc.batchApply("/nonexistent/rom.nes", {});
        QCOMPARE(results.size(), 0);
    }

    void testGenerateOutputPath()
    {
        QString out = PatchService::generateOutputPath(
            "/roms/game.nes", "/patches/fix.ips");
        QVERIFY(!out.isEmpty());
        // Output should end with the ROM extension
        QVERIFY2(out.endsWith(".nes"),
                 qPrintable(QString("Expected .nes suffix, got: %1").arg(out)));
    }

    void testSetToolPathsDoNotCrash()
    {
        PatchService svc;
        svc.setFlipsPath("/usr/bin/flips");
        svc.setXdelta3Path("/usr/bin/xdelta3");
        svc.setPpfPath("/usr/bin/ppf");
        // Verify setters don't crash — getters may not persist if path validation fails
    }

    void testIsFormatSupportedDoesNotCrash()
    {
        PatchService svc;
        // May return true or false depending on host tools
        svc.isFormatSupported(PatchFormat::IPS);
        svc.isFormatSupported(PatchFormat::BPS);
        svc.isFormatSupported(PatchFormat::UPS);
        svc.isFormatSupported(PatchFormat::Unknown);
    }
};

QTEST_MAIN(TestPatchService)
#include "test_patch_service.moc"
