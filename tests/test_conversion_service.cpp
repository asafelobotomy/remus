/**
 * @file test_conversion_service.cpp
 * @brief Unit tests for ConversionService (tool detection, basic extraction)
 *
 * External tools (chdman, 7z, unzip) may or may not be present.
 * Tests validate the service API without requiring actual disc images.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/services/conversion_service.h"

using namespace Remus;

class TestConversionService : public QObject
{
    Q_OBJECT

private slots:

    void testIsChdmanAvailableDoesNotCrash()
    {
        ConversionService svc;
        // Just verify the call doesn't crash — result depends on host
        bool avail = svc.isChdmanAvailable();
        Q_UNUSED(avail);
    }

    void testGetChdmanVersionDoesNotCrash()
    {
        ConversionService svc;
        QString ver = svc.getChdmanVersion();
        Q_UNUSED(ver);
    }

    void testGetArchiveToolStatusReturnsMap()
    {
        ConversionService svc;
        auto status = svc.getArchiveToolStatus();
        // Map should contain known formats
        QVERIFY2(!status.isEmpty(), "Tool status map should not be empty");
    }

    void testGetArchiveCompressionToolStatusReturnsMap()
    {
        ConversionService svc;
        auto status = svc.getArchiveCompressionToolStatus();
        QVERIFY2(!status.isEmpty(), "Compression tool status map should not be empty");
    }

    void testCanExtractInvalidPath()
    {
        ConversionService svc;
        bool result = svc.canExtract("/nonexistent/file.zip");
        Q_UNUSED(result);
        // Should not crash
    }

    void testConvertToCHDMissingFile()
    {
        ConversionService svc;
        auto result = svc.convertToCHD("/nonexistent/game.cue");
        QVERIFY(!result.success);
    }

    void testExtractCHDMissingFile()
    {
        ConversionService svc;
        auto result = svc.extractCHD("/nonexistent/game.chd");
        QVERIFY(!result.success);
    }

    void testSetChdmanPathDoesNotCrash()
    {
        ConversionService svc;
        svc.setChdmanPath("/usr/bin/chdman");
        // Just verify it doesn't crash
    }

    void testIsRunningInitiallyFalse()
    {
        ConversionService svc;
        QVERIFY(!svc.isRunning());
    }

    void testExtractArchiveMissingFile()
    {
        ConversionService svc;
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        auto result = svc.extractArchive("/nonexistent/archive.zip", tmp.path());
        QVERIFY(!result.success);
    }

    void testCompressToArchiveNoFiles()
    {
        ConversionService svc;
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        auto result = svc.compressToArchive(
            {}, tmp.path() + "/empty.zip", ArchiveFormat::ZIP);
        QVERIFY(!result.success);
    }
};

QTEST_MAIN(TestConversionService)
#include "test_conversion_service.moc"
