#include <QtTest/QtTest>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "../src/core/disc_magic_detector.h"
#include "../src/core/archive_extractor.h"
#include "../src/core/constants/systems.h"

using namespace Remus;
using namespace Remus::Constants::Systems;

class DiscMagicDetectorTest : public QObject {
    Q_OBJECT

private slots:
    void testIsDiscImageExtension();
    void testIsNotDiscImageExtension();
    void testDetectGameCube();
    void testDetectWii();
    void testDetectDreamcast();
    void testDetectSaturn();
    void testDetectSegaCD();
    void testDetectPSP();
    void testDetectPS2();
    void testDetectPS1();
    void testPS1PS2SizeDisambiguation();
    void testEmptyDataReturnsNotDetected();
    void testTooSmallDataReturnsNotDetected();
    void testDreamcastSerialExtraction();
    void testDetectFromArchive();
};

void DiscMagicDetectorTest::testIsDiscImageExtension() {
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".iso"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".ISO"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".bin"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".cdi"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".gdi"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".cue"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".img"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".wbfs"));
    QVERIFY(DiscMagicDetector::isDiscImageExtension(".gcm"));
}

void DiscMagicDetectorTest::testIsNotDiscImageExtension() {
    QVERIFY(!DiscMagicDetector::isDiscImageExtension(".nes"));
    QVERIFY(!DiscMagicDetector::isDiscImageExtension(".sfc"));
    QVERIFY(!DiscMagicDetector::isDiscImageExtension(".gba"));
    QVERIFY(!DiscMagicDetector::isDiscImageExtension(".zip"));
    QVERIFY(!DiscMagicDetector::isDiscImageExtension(""));
}

void DiscMagicDetectorTest::testDetectGameCube() {
    // GameCube disc magic: 0xC2339F3D at offset 0x1C
    QByteArray data(0x10000, '\0');
    data[0x1C] = '\xC2';
    data[0x1D] = '\x33';
    data[0x1E] = '\x9F';
    data[0x1F] = '\x3D';

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 1400LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_GAMECUBE);
    QCOMPARE(info.systemName, QStringLiteral("GameCube"));
}

void DiscMagicDetectorTest::testDetectWii() {
    // Wii disc magic: 0x5D1C9EA3 at offset 0x18
    QByteArray data(0x10000, '\0');
    data[0x18] = '\x5D';
    data[0x19] = '\x1C';
    data[0x1A] = '\x9E';
    data[0x1B] = '\xA3';

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 4700LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_WII);
    QCOMPARE(info.systemName, QStringLiteral("Wii"));
}

void DiscMagicDetectorTest::testDetectDreamcast() {
    // Dreamcast: "SEGA SEGAKATANA" at offset 0x10
    QByteArray data(0x10000, '\0');
    QByteArray magic("SEGA SEGAKATANA", 15);
    data.replace(0x10, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 700LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_DREAMCAST);
    QCOMPARE(info.systemName, QStringLiteral("Dreamcast"));
}

void DiscMagicDetectorTest::testDetectSaturn() {
    // Saturn: "SEGA SEGASATURN" at offset 0x10
    QByteArray data(0x10000, '\0');
    QByteArray magic("SEGA SEGASATURN", 15);
    data.replace(0x10, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 700LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_SATURN);
    QCOMPARE(info.systemName, QStringLiteral("Saturn"));
}

void DiscMagicDetectorTest::testDetectSegaCD() {
    // Sega CD: "SEGADISCSYSTEM" at offset 0x10
    QByteArray data(0x10000, '\0');
    QByteArray magic("SEGADISCSYSTEM", 14);
    data.replace(0x10, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 700LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_SEGA_CD);
    QCOMPARE(info.systemName, QStringLiteral("Sega CD"));
}

void DiscMagicDetectorTest::testDetectPSP() {
    // PSP: "PSP GAME" at offset 0x8008
    QByteArray data(0x10000, '\0');
    QByteArray magic("PSP GAME", 8);
    data.replace(0x8008, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 1400LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_PSP);
    QCOMPARE(info.systemName, QStringLiteral("PSP"));
}

void DiscMagicDetectorTest::testDetectPS2() {
    // PS2: "PLAYSTATION" at offset 0x8008 with file > 800MB
    QByteArray data(0x10000, '\0');
    QByteArray magic("PLAYSTATION", 11);
    data.replace(0x8008, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 4700LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_PS2);
}

void DiscMagicDetectorTest::testDetectPS1() {
    // PS1: "Sony Computer " at offset 0x24F8
    QByteArray data(0x10000, '\0');
    QByteArray magic("Sony Computer ", 14);
    data.replace(0x24F8, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 600LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_PSX);
    QCOMPARE(info.systemName, QStringLiteral("PlayStation"));
}

void DiscMagicDetectorTest::testPS1PS2SizeDisambiguation() {
    // PS2 "PLAYSTATION" magic at 0x8008, but file ≤ 800MB → detected as PS1
    QByteArray data(0x10000, '\0');
    QByteArray magic("PLAYSTATION", 11);
    data.replace(0x8008, magic.size(), magic);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 600LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_PSX);
    QCOMPARE(info.systemName, QStringLiteral("PlayStation"));
}

void DiscMagicDetectorTest::testEmptyDataReturnsNotDetected() {
    QByteArray data;
    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 0);
    QVERIFY(!info.detected);
    QCOMPARE(info.systemId, 0);
}

void DiscMagicDetectorTest::testTooSmallDataReturnsNotDetected() {
    // Data too small to contain any magic at expected offsets
    QByteArray data(16, '\0');
    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 16);
    QVERIFY(!info.detected);
}

void DiscMagicDetectorTest::testDreamcastSerialExtraction() {
    // Build a fake IP.BIN with "SEGA SEGAKATANA" at offset 0x10
    // and serial at +0x40, title at +0x80
    QByteArray data(0x200, '\0');
    QByteArray magic("SEGA SEGAKATANA", 15);
    data.replace(0x10, magic.size(), magic);

    // Serial at IP.BIN offset 0x40 → data offset 0x10 + 0x40 = 0x50
    QByteArray serial("HDR-0176  ", 10);
    data.replace(0x50, serial.size(), serial);

    // Title at IP.BIN offset 0x80 → data offset 0x10 + 0x80 = 0x90
    QByteArray title("SONIC ADVENTURE");
    data.replace(0x90, title.size(), title);

    DiscHeaderInfo info = DiscMagicDetector::detectFromData(data, 700LL * 1024 * 1024);
    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_DREAMCAST);
    QCOMPARE(info.serial, QStringLiteral("HDR-0176"));
    QCOMPARE(info.title, QStringLiteral("SONIC ADVENTURE"));
}

void DiscMagicDetectorTest::testDetectFromArchive() {
    // Requires 7z to pack a fake disc image.
    if (QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()) {
        QSKIP("7z not available — skipping detectFromArchive test");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Build a 64 KB fake PS2 disc image: PLAYSTATION magic at 0x8008.
    QByteArray isoData(0x10000, '\0');
    const QByteArray magic("PLAYSTATION", 11);
    isoData.replace(0x8008, magic.size(), magic);

    const QString isoName = QStringLiteral("fake.iso");
    QFile isoFile(dir.filePath(isoName));
    QVERIFY(isoFile.open(QIODevice::WriteOnly));
    isoFile.write(isoData);
    isoFile.close();

    // Pack into a 7z archive and remove the original.
    const QString archivePath = dir.filePath(QStringLiteral("test.7z"));
    QProcess packer;
    packer.setWorkingDirectory(dir.path());
    packer.start(QStringLiteral("7z"), { QStringLiteral("a"), archivePath, isoName });
    QVERIFY(packer.waitForFinished(15000) && packer.exitCode() == 0);
    QFile::remove(dir.filePath(isoName));

    // memberSize > 800 MB forces PS2 (not PS1) classification.
    const DiscHeaderInfo info = DiscMagicDetector::detectFromArchive(archivePath, isoName, 1LL * 1024 * 1024 * 1024);

    QVERIFY(info.detected);
    QCOMPARE(info.systemId, ID_PS2);
    QCOMPARE(info.systemName, QStringLiteral("PlayStation 2"));
}

QTEST_MAIN(DiscMagicDetectorTest)
#include "test_disc_magic_detector.moc"
