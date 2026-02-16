#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/header_detector.h"

using namespace Remus;

class HeaderDetectorTest : public QObject
{
    Q_OBJECT

private slots:
    void testDetectNES();
    void testDetectLynx();
    void testDetectFDS();
    void testDetectA78();
    void testDetectSNES();
    void testStripHeader();
    void testHelpers();
};

void HeaderDetectorTest::testDetectNES()
{
    HeaderDetector detector;

    QByteArray header("NES\x1A");
    header.append(QByteArray(12, '\x00'));
    HeaderInfo info = detector.detectFromData(header, ".nes");

    QVERIFY(info.hasHeader);
    QCOMPARE(info.headerSize, 16);
    QCOMPARE(info.headerType, QStringLiteral("iNES"));
    QCOMPARE(info.systemHint, QStringLiteral("NES"));
    QVERIFY(info.valid);

    header[7] = 0x08;
    HeaderInfo nes2 = detector.detectFromData(header, ".nes");
    QCOMPARE(nes2.headerType, QStringLiteral("NES2.0"));
}

void HeaderDetectorTest::testDetectLynx()
{
    HeaderDetector detector;
    QByteArray data(64, '\x00');
    data[0] = 'L';
    data[1] = 'Y';
    data[2] = 'N';
    data[3] = 'X';
    QByteArray name("TestGame");
    data.replace(10, name.size(), name);

    HeaderInfo info = detector.detectFromData(data, ".lnx");
    QVERIFY(info.hasHeader);
    QCOMPARE(info.headerType, QStringLiteral("Lynx"));
    QCOMPARE(info.systemHint, QStringLiteral("Atari Lynx"));
    QVERIFY(info.info.contains("TestGame"));
}

void HeaderDetectorTest::testDetectFDS()
{
    HeaderDetector detector;
    QByteArray data("FDS\x1A");
    data.append(QByteArray(12, '\x00'));
    data[4] = 2;

    HeaderInfo info = detector.detectFromData(data, ".fds");
    QVERIFY(info.hasHeader);
    QCOMPARE(info.headerType, QStringLiteral("fwNES FDS"));
    QCOMPARE(info.systemHint, QStringLiteral("Famicom Disk System"));
    QVERIFY(info.info.contains("Disk sides: 2"));
}

void HeaderDetectorTest::testDetectA78()
{
    HeaderDetector detector;
    QByteArray data(128, '\x00');
    data[1] = 'A';
    data[2] = 'T';
    data[3] = 'A';
    data[4] = 'R';
    data[5] = 'I';
    data[6] = '7';
    data[7] = '8';
    data[8] = '0';
    data[9] = '0';
    QByteArray title("Test Title");
    data.replace(17, title.size(), title);

    HeaderInfo info = detector.detectFromData(data, ".a78");
    QVERIFY(info.hasHeader);
    QCOMPARE(info.headerType, QStringLiteral("A78"));
    QCOMPARE(info.systemHint, QStringLiteral("Atari 7800"));
    QVERIFY(info.info.contains("Test Title"));
}

void HeaderDetectorTest::testDetectSNES()
{
    HeaderDetector detector;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString smcPath = dir.path() + "/test.smc";
    QByteArray data(262656, '\x00');
    QFile file(smcPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(data) == data.size());
    file.close();

    HeaderInfo info = detector.detect(smcPath);
    QVERIFY(info.hasHeader);
    QCOMPARE(info.headerType, QStringLiteral("SMC"));
    QCOMPARE(info.headerSize, 512);
    QCOMPARE(info.systemHint, QStringLiteral("SNES"));
}

void HeaderDetectorTest::testStripHeader()
{
    HeaderDetector detector;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString inPath = dir.path() + "/headered.nes";
    const QString outPath = dir.path() + "/stripped.nes";
    QByteArray header("NES\x1A");
    header.append(QByteArray(12, '\x00'));
    QByteArray body("rom-test-data");

    QFile inFile(inPath);
    QVERIFY(inFile.open(QIODevice::WriteOnly));
    QVERIFY(inFile.write(header) == header.size());
    QVERIFY(inFile.write(body) == body.size());
    inFile.close();

    QVERIFY(detector.stripHeader(inPath, outPath));

    QFile outFile(outPath);
    QVERIFY(outFile.open(QIODevice::ReadOnly));
    QByteArray outData = outFile.readAll();
    outFile.close();

    QCOMPARE(outData, body);
}

void HeaderDetectorTest::testHelpers()
{
    QCOMPARE(HeaderDetector::mayHaveHeader(".nes"), true);
    QCOMPARE(HeaderDetector::mayHaveHeader(".bin"), false);
    QCOMPARE(HeaderDetector::getExpectedHeaderSize(".nes"), 16);
    QCOMPARE(HeaderDetector::getExpectedHeaderSize(".lnx"), 64);
    QCOMPARE(HeaderDetector::getExpectedHeaderSize(".bin"), 0);
}

QTEST_MAIN(HeaderDetectorTest)
#include "test_header_detector.moc"
