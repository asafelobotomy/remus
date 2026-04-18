/**
 * @file test_conversion_service.cpp
 * @brief Unit tests for ConversionService (tool detection, basic extraction)
 *
 * External tools (chdman, 7z, unzip) may or may not be present.
 * Tests validate the service API without requiring actual disc images.
 */

#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>

#include "../src/core/archive_creator.h"
#include "../src/core/archive_extractor.h"
#include "../src/core/database.h"
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

    void testConvertToChdMissingImgUsesSupportedPath()
    {
        ConversionService svc;
        auto result = svc.convertToCHD("/nonexistent/game.img");
        QVERIFY(!result.success);
        QVERIFY(result.error.contains("File not found"));
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

    void testIsMaxcsoAvailableDoesNotCrash()
    {
        ConversionService svc;
        bool avail = svc.isMaxcsoAvailable();
        Q_UNUSED(avail);
    }

    void testGetMaxcsoVersionDoesNotCrash()
    {
        ConversionService svc;
        QString ver = svc.getMaxcsoVersion();
        Q_UNUSED(ver);
    }

    void testConvertToCSOMissingFile()
    {
        ConversionService svc;
        auto result = svc.convertToCSO("/nonexistent/game.iso");
        QVERIFY(!result.success);
    }

    void testConvertToCSOUnsupportedFile()
    {
        ConversionService svc;
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString textPath = tmp.filePath("note.txt");
        QFile file(textPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("note") == 4);
        file.close();

        auto result = svc.convertToCSO(textPath);
        QVERIFY(!result.success);
        QVERIFY(result.error.contains("Unsupported file format"));
    }

    void testExtractCSOMissingFile()
    {
        ConversionService svc;
        auto result = svc.extractCSO("/nonexistent/game.cso");
        QVERIFY(!result.success);
    }

    void testExtractCSOUnsupportedFile()
    {
        ConversionService svc;
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString textPath = tmp.filePath("note.txt");
        QFile file(textPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("note") == 4);
        file.close();

        auto result = svc.extractCSO(textPath);
        QVERIFY(!result.success);
        QVERIFY(result.error.contains("Unsupported file format"));
    }

    void testSetMaxcsoPathDoesNotCrash()
    {
        ConversionService svc;
        svc.setMaxcsoPath("/usr/bin/maxcso");
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

    void testExtractArchiveWithDbUpdatePreservesNestedMemberPath()
    {
        ArchiveCreator creator;
        ArchiveExtractor extractor;
        if (!creator.canCompress(ArchiveFormat::ZIP)) {
            QSKIP("zip tool not available");
        }
        if (!extractor.canExtract(ArchiveFormat::ZIP)) {
            QSKIP("unzip tool not available");
        }

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString sourceDir = tmp.filePath("source");
        QVERIFY(QDir().mkpath(sourceDir + "/nested"));

        QFile sourceFile(sourceDir + "/nested/game.bin");
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QVERIFY(sourceFile.write("BINPAYLOAD") == 10);
        sourceFile.close();

        const QString archivePath = tmp.filePath("games.zip");
        const CompressionResult compressed = creator.compressDirectoryContents(
            sourceDir, archivePath, ArchiveFormat::ZIP);
        QVERIFY2(compressed.success, qPrintable(compressed.error));

        Database db;
        QVERIFY(db.initialize(tmp.filePath("test.db")));
        const int libraryId = db.insertLibrary(tmp.path(), "Test");
        QVERIFY(libraryId > 0);

        FileRecord rec;
        rec.libraryId = libraryId;
        rec.originalPath = archivePath;
        rec.currentPath = archivePath;
        rec.filename = "game.bin";
        rec.extension = ".bin";
        rec.isCompressed = true;
        rec.archivePath = archivePath;
        rec.archiveInternalPath = "nested/game.bin";
        rec.id = db.insertFile(rec);
        QVERIFY(rec.id > 0);

        ConversionService svc;
        const QString outputDir = tmp.filePath("extracted");
        const ExtractionResult result = svc.extractArchiveWithDbUpdate(archivePath, outputDir, &db);
        QVERIFY2(result.success, qPrintable(result.error));

        const FileRecord extracted = db.getFileById(rec.id);
        QCOMPARE(extracted.currentPath, result.outputDir + "/nested/game.bin");
        QVERIFY(QFile::exists(extracted.currentPath));
        QCOMPARE(extracted.filename, QStringLiteral("game.bin"));
        QCOMPARE(extracted.extension, QStringLiteral(".bin"));
        QCOMPARE(extracted.fileSize, static_cast<qint64>(10));
        QVERIFY(!extracted.isCompressed);
        QVERIFY(extracted.archivePath.isEmpty());
        QVERIFY(extracted.archiveInternalPath.isEmpty());
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
