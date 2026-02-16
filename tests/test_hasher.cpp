#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <zlib.h>
#include "../src/core/hasher.h"

using namespace Remus;

namespace {
QString crc32Hex(const QByteArray &data)
{
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(data.constData()), data.size());
    return QString("%1").arg(crc, 8, 16, QChar('0')).toLower();
}
}

class HasherTest : public QObject
{
    Q_OBJECT

private slots:
    void testCalculateHashes();
    void testCalculateHashSingle();
    void testStripHeader();
    void testDetectHeaderSize();
    void testMissingFile();
};

void HasherTest::testCalculateHashes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.path() + "/rom.bin";
    const QByteArray data("rom-test-data");

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(data) == data.size());
    file.close();

    Hasher hasher;
    HashResult result = hasher.calculateHashes(filePath);

    QVERIFY(result.success);
    QCOMPARE(result.crc32, crc32Hex(data));
    QCOMPARE(result.md5, QString(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex()));
    QCOMPARE(result.sha1, QString(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex()));
}

void HasherTest::testCalculateHashSingle()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.path() + "/rom.bin";
    const QByteArray data("rom-test-data");

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(data) == data.size());
    file.close();

    Hasher hasher;
    QCOMPARE(hasher.calculateHash(filePath, "CRC32"), crc32Hex(data));
    QCOMPARE(hasher.calculateHash(filePath, "MD5"), QString(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex()));
    QCOMPARE(hasher.calculateHash(filePath, "SHA1"), QString(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex()));
    QCOMPARE(hasher.calculateHash(filePath, "UNKNOWN"), QString());
}

void HasherTest::testStripHeader()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.path() + "/headered.bin";
    const QByteArray header(16, '\x00');
    const QByteArray data("rom-test-data");

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(header) == header.size());
    QVERIFY(file.write(data) == data.size());
    file.close();

    Hasher hasher;
    HashResult result = hasher.calculateHashes(filePath, true, 16);

    QVERIFY(result.success);
    QCOMPARE(result.crc32, crc32Hex(data));
}

void HasherTest::testDetectHeaderSize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString nesPath = dir.path() + "/test.nes";
    QByteArray nesHeader("NES\x1A");
    nesHeader.append(QByteArray(12, '\x00'));
    QFile nesFile(nesPath);
    QVERIFY(nesFile.open(QIODevice::WriteOnly));
    QVERIFY(nesFile.write(nesHeader) == nesHeader.size());
    nesFile.close();

    QCOMPARE(Hasher::detectHeaderSize(nesPath, ".nes"), 16);
    QCOMPARE(Hasher::detectHeaderSize(nesPath, ".lnx"), 64);

    const QString smcPath = dir.path() + "/test.smc";
    QByteArray smcData(1536, '\x00');
    QFile smcFile(smcPath);
    QVERIFY(smcFile.open(QIODevice::WriteOnly));
    QVERIFY(smcFile.write(smcData) == smcData.size());
    smcFile.close();

    QCOMPARE(Hasher::detectHeaderSize(smcPath, ".smc"), 512);
    QCOMPARE(Hasher::detectHeaderSize(smcPath, ".bin"), 0);
}

void HasherTest::testMissingFile()
{
    Hasher hasher;
    HashResult result = hasher.calculateHashes("/no/such/file.bin");
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
    QCOMPARE(hasher.calculateHash("/no/such/file.bin", "CRC32"), QString());
}

QTEST_MAIN(HasherTest)
#include "test_hasher.moc"
