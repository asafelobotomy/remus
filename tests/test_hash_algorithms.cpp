#include <QtTest>
#include "core/constants/hash_algorithms.h"

using namespace Remus::Constants;

class HashAlgorithmsTest : public QObject {
    Q_OBJECT

private slots:
    void detectFromLength();
    void validateHashes();
    void displayConversions();
};

void HashAlgorithmsTest::detectFromLength()
{
    QCOMPARE(HashAlgorithms::detectFromLength(8), QStringLiteral("crc32"));
    QCOMPARE(HashAlgorithms::detectFromLength(32), QStringLiteral("md5"));
    QCOMPARE(HashAlgorithms::detectFromLength(40), QStringLiteral("sha1"));
    QVERIFY(HashAlgorithms::detectFromLength(10).isEmpty());
}

void HashAlgorithmsTest::validateHashes()
{
    QVERIFY(HashAlgorithms::isValidHash("1bc674be", "crc32"));
    QVERIFY(HashAlgorithms::isValidHash(QString(32, QChar('a')), "md5"));
    QVERIFY(HashAlgorithms::isValidHash(QString(40, QChar('b')), "sha1"));
    QVERIFY(!HashAlgorithms::isValidHash("short", "md5"));
}

void HashAlgorithmsTest::displayConversions()
{
    QCOMPARE(HashAlgorithms::displayName("md5"), QStringLiteral("MD5"));
    QCOMPARE(HashAlgorithms::displayName("sha1"), QStringLiteral("SHA1"));
    QCOMPARE(HashAlgorithms::displayName("crc32"), QStringLiteral("CRC32"));
    QCOMPARE(HashAlgorithms::toAlgorithmId("MD5"), QStringLiteral("md5"));
    QCOMPARE(HashAlgorithms::toAlgorithmId("SHA1"), QStringLiteral("sha1"));
    QCOMPARE(HashAlgorithms::toAlgorithmId("CRC32"), QStringLiteral("crc32"));
    QVERIFY(HashAlgorithms::isValidAlgorithm("md5"));
    QVERIFY(HashAlgorithms::allAlgorithms().contains("sha1"));
}

QTEST_MAIN(HashAlgorithmsTest)
#include "test_hash_algorithms.moc"
