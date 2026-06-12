#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>

#include "../src/core/ra_hasher.h"
#include "../src/core/constants/system_ids.h"

using namespace Remus;

class RaHasherTest : public QObject {
    Q_OBJECT

private slots:
    void testFullFileMd5() {
        const QByteArray payload = QByteArrayLiteral("retro achievements test payload");
        const QString expected = QString(QCryptographicHash::hash(payload, QCryptographicHash::Md5).toHex()).toLower();
        const QString actual = RaHasher::md5ForPayload(payload, Constants::Systems::ID_GBA, QStringLiteral(".gba"));
        QCOMPARE(actual, expected);
    }

    void testNesInesHeaderStrip() {
        QByteArray payload(32, '\0');
        payload[0] = 'N';
        payload[1] = 'E';
        payload[2] = 'S';
        payload[3] = char(0x1A);
        payload[16] = 'R';
        payload[17] = 'O';
        payload[18] = 'M';

        const QByteArray body = payload.mid(16);
        const QString expected = QString(QCryptographicHash::hash(body, QCryptographicHash::Md5).toHex()).toLower();
        const QString actual = RaHasher::md5ForPayload(payload, Constants::Systems::ID_NES, QStringLiteral(".nes"));
        QCOMPARE(actual, expected);
    }

    void testSnesCopierHeaderSkip() {
        QByteArray payload(8192 + 512, '\xAB');
        const QByteArray body = payload.mid(512);
        const QString expected = QString(QCryptographicHash::hash(body, QCryptographicHash::Md5).toHex()).toLower();
        const QString actual
            = RaHasher::md5ForPayload(payload, Constants::Systems::ID_SNES, QStringLiteral(".smc"), payload.size());
        QCOMPARE(actual, expected);
    }

    void testN64V64ByteSwap() {
        QByteArray payload = QByteArray::fromHex("0011223344556677");
        QByteArray swapped = payload;
        for (int i = 0; i + 1 < swapped.size(); i += 2)
            qSwap(swapped[i], swapped[i + 1]);
        const QString expected = QString(QCryptographicHash::hash(swapped, QCryptographicHash::Md5).toHex()).toLower();
        const QString actual = RaHasher::md5ForPayload(payload, Constants::Systems::ID_N64, QStringLiteral(".v64"));
        QCOMPARE(actual, expected);
    }

    void testRaConsoleMapping() {
        QVERIFY(RaHasher::hasRaMapping(Constants::Systems::ID_NES));
        QCOMPARE(RaHasher::raConsoleId(Constants::Systems::ID_NES), 7);
        QCOMPARE(RaHasher::raConsoleId(Constants::Systems::ID_SNES), 3);
    }

    void testComputeFromTempFile() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QByteArray payload = QByteArrayLiteral("game boy advance payload");
        const QString path = tmp.path() + QStringLiteral("/test.gba");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(payload), payload.size());
        file.close();

        const RaHasher::Result result = RaHasher::compute(path, Constants::Systems::ID_GBA, QStringLiteral(".gba"));
        QVERIFY(result.success);
        QCOMPARE(result.md5, RaHasher::md5ForPayload(payload, Constants::Systems::ID_GBA, QStringLiteral(".gba")));
        QVERIFY(!result.usedExternalTool);
    }
};

QTEST_MAIN(RaHasherTest)
#include "test_ra_hasher.moc"
