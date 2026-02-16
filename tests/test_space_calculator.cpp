#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSignalSpy>
#include "core/space_calculator.h"

using namespace Remus;

class SpaceCalculatorTest : public QObject {
    Q_OBJECT

private slots:
    void cueAggregatesBinTracks();
    void gdiSumsTrackSizes();
    void actualStatsUseConvertedSize();
    void scanDirectoryAggregatesFormats();
    void formatHelpers();
};

static qint64 writeFileWithSize(const QString &path, int bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return 0;
    }
    QByteArray payload(bytes, 'A');
    file.write(payload);
    file.close();
    return file.size();
}

void SpaceCalculatorTest::cueAggregatesBinTracks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString cuePath = dir.filePath("game.cue");
    const QString binPath = dir.filePath("game (Track 1).bin");

    QVERIFY(writeFileWithSize(cuePath, 100) > 0);
    const qint64 binSize = writeFileWithSize(binPath, 1024);
    QVERIFY(binSize > 0);

    SpaceCalculator calc;
    ConversionStats stats = calc.estimateConversion(cuePath);

    QCOMPARE(stats.format, QString("BIN/CUE"));
    QCOMPARE(stats.originalSize, 100 + binSize);
    QVERIFY(stats.convertedSize > 0);
    QVERIFY(stats.savedBytes >= 0);
    QVERIFY(!SpaceCalculator::isCHD(cuePath));
    QVERIFY(SpaceCalculator::isConvertible(cuePath));
}

void SpaceCalculatorTest::gdiSumsTrackSizes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString gdiPath = dir.filePath("disc.gdi");
    const QString track1 = dir.filePath("track01.bin");
    const QString track2 = dir.filePath("track02.bin");

    const qint64 size1 = writeFileWithSize(track1, 2048);
    const qint64 size2 = writeFileWithSize(track2, 4096);
    QVERIFY(size1 > 0 && size2 > 0);

    QFile gdiFile(gdiPath);
    QVERIFY(gdiFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&gdiFile);
    out << "2" << '\n';
    out << "1 0 4 2352 track01.bin" << '\n';
    out << "2 0 4 2352 track02.bin" << '\n';
    gdiFile.close();

    SpaceCalculator calc;
    ConversionStats stats = calc.estimateConversion(gdiPath);

    QCOMPARE(stats.format, QString("GDI"));
    QCOMPARE(stats.originalSize, gdiFile.size() + size1 + size2);
    QVERIFY(stats.savedBytes >= 0);
}

void SpaceCalculatorTest::actualStatsUseConvertedSize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.filePath("game.iso");
    const QString chdPath = dir.filePath("game.chd");

    const qint64 isoSize = writeFileWithSize(isoPath, 5000);
    const qint64 chdSize = writeFileWithSize(chdPath, 2500);
    QVERIFY(isoSize > 0 && chdSize > 0);

    SpaceCalculator calc;
    ConversionStats stats = calc.getActualStats(isoPath, chdPath);

    QCOMPARE(stats.path, isoPath);
    QCOMPARE(stats.originalSize, isoSize);
    QCOMPARE(stats.convertedSize, chdSize);
    QCOMPARE(stats.savedBytes, isoSize - chdSize);
    QVERIFY(stats.compressionRatio > 0.0);
}

void SpaceCalculatorTest::scanDirectoryAggregatesFormats()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.filePath("title.iso");
    const QString chdPath = dir.filePath("title.chd");
    const QString cuePath = dir.filePath("title.cue");
    const QString binPath = dir.filePath("title.bin");

    QVERIFY(writeFileWithSize(isoPath, 3000) > 0);
    QVERIFY(writeFileWithSize(chdPath, 1200) > 0);
    QVERIFY(writeFileWithSize(cuePath, 200) > 0);
    QVERIFY(writeFileWithSize(binPath, 800) > 0);

    SpaceCalculator calc;
    QSignalSpy progressSpy(&calc, &SpaceCalculator::scanProgress);

    ConversionSummary summary = calc.scanDirectory(dir.path(), false);

    QCOMPARE(summary.totalFiles, 3);           // ISO, CHD, CUE (BIN skipped)
    QCOMPARE(summary.convertedFiles, 1);       // CHD
    QCOMPARE(summary.convertibleFiles, 2);     // ISO + CUE
    QVERIFY(summary.totalOriginalSize >= 5000);
    QVERIFY(summary.totalConvertedSize > 0);
    QVERIFY(summary.totalSavedBytes >= 0);
    QVERIFY(summary.averageCompressionRatio > 0.0);
    QVERIFY(!progressSpy.isEmpty());
}

void SpaceCalculatorTest::formatHelpers()
{
    QCOMPARE(SpaceCalculator::formatBytes(500), QString("500 bytes"));
    QCOMPARE(SpaceCalculator::formatBytes(2048), QString("2.00 KB"));
    QCOMPARE(SpaceCalculator::formatBytes(5 * 1024 * 1024), QString("5.00 MB"));

    ConversionSummary summary;
    summary.totalFiles = 1;
    summary.convertibleFiles = 1;
    summary.convertedFiles = 0;
    summary.totalOriginalSize = 4000;
    summary.totalConvertedSize = 2000;
    summary.totalSavedBytes = 2000;
    summary.averageCompressionRatio = 0.5;
    summary.sizeByFormat.insert("ISO", 4000);
    summary.countByFormat.insert("ISO", 1);

    SpaceCalculator calc;
    const QString report = calc.formatSavingsReport(summary);
    QVERIFY(report.contains("Total files scanned"));
    QVERIFY(report.contains("ISO"));
}

QTEST_MAIN(SpaceCalculatorTest)
#include "test_space_calculator.moc"
