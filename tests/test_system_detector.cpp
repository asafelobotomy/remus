#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/core/system_detector.h"

using namespace Remus;

class SystemDetectorTest : public QObject
{
    Q_OBJECT

private slots:
    void testDetectIsoGameCubeByHeaderMagic();
    void testDetectIsoWiiByHeaderMagic();
    void testDetectIsoPs2ByBootSignature();
};

void SystemDetectorTest::testDetectIsoGameCubeByHeaderMagic()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + "/gamecube_test.iso";
    QFile f(isoPath);
    QVERIFY(f.open(QIODevice::WriteOnly));

    QByteArray data(0x40, '\0');
    // GameCube magic 0xC2339F3D at offset 0x1C (big-endian)
    data[0x1C] = static_cast<char>(0xC2);
    data[0x1D] = static_cast<char>(0x33);
    data[0x1E] = static_cast<char>(0x9F);
    data[0x1F] = static_cast<char>(0x3D);
    QCOMPARE(f.write(data), data.size());
    f.close();

    SystemDetector detector;
    QCOMPARE(detector.detectSystem(QStringLiteral(".iso"), isoPath), QStringLiteral("GameCube"));
}

void SystemDetectorTest::testDetectIsoWiiByHeaderMagic()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + "/wii_test.iso";
    QFile f(isoPath);
    QVERIFY(f.open(QIODevice::WriteOnly));

    QByteArray data(0x40, '\0');
    // Wii magic 0x5D1C9EA3 at offset 0x18 (big-endian)
    data[0x18] = static_cast<char>(0x5D);
    data[0x19] = static_cast<char>(0x1C);
    data[0x1A] = static_cast<char>(0x9E);
    data[0x1B] = static_cast<char>(0xA3);
    QCOMPARE(f.write(data), data.size());
    f.close();

    SystemDetector detector;
    QCOMPARE(detector.detectSystem(QStringLiteral(".iso"), isoPath), QStringLiteral("Wii"));
}

void SystemDetectorTest::testDetectIsoPs2ByBootSignature()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString isoPath = dir.path() + "/ps2_test.iso";
    QFile f(isoPath);
    QVERIFY(f.open(QIODevice::WriteOnly));

    QByteArray data(1024, '\0');
    const QByteArray sig = "BOOT2 = cdrom0:\\\\SLUS_123.45;1";
    data.replace(0, sig.size(), sig);
    QCOMPARE(f.write(data), data.size());
    f.close();

    SystemDetector detector;
    QCOMPARE(detector.detectSystem(QStringLiteral(".iso"), isoPath), QStringLiteral("PlayStation 2"));
}

QTEST_MAIN(SystemDetectorTest)
#include "test_system_detector.moc"
