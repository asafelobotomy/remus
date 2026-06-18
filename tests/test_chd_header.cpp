#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/core/chd_header.h"

using namespace Remus;

namespace {

QByteArray buildChdV5Header(const QByteArray &sha1Bytes) {
    QByteArray header(124, '\0');
    header.replace(0, 8, QByteArrayLiteral("MComprHD"));
    header[12] = '\x05'; // version 5 (little-endian u32 at offset 12)
    header.replace(64, sha1Bytes.size(), sha1Bytes);
    return header;
}

} // namespace

class ChdHeaderTest : public QObject {
    Q_OBJECT

private slots:
    void testReadV5HeaderDigest();
    void testRejectsNonV5AndMissingFile();
};

void ChdHeaderTest::testReadV5HeaderDigest() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QByteArray sha1Bytes = QByteArray::fromHex("0123456789abcdef0123456789abcdef01234567");
    const QString chdPath = tempDir.filePath(QStringLiteral("disc.chd"));

    QFile file(chdPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(buildChdV5Header(sha1Bytes)) == 124);
    file.close();

    const ChdHeaderDigest digest = readChdHeaderDigest(chdPath);
    QVERIFY(digest.valid);
    QCOMPARE(digest.version, 5);
    QCOMPARE(digest.sha1, QString::fromLatin1(sha1Bytes.toHex()));
}

void ChdHeaderTest::testRejectsNonV5AndMissingFile() {
    const ChdHeaderDigest missing = readChdHeaderDigest(QStringLiteral("/no/such/file.chd"));
    QVERIFY(!missing.valid);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QByteArray header(124, '\0');
    header.replace(0, 8, QByteArrayLiteral("MComprHD"));
    header[12] = '\x04'; // version 4

    const QString chdPath = tempDir.filePath(QStringLiteral("legacy.chd"));
    QFile file(chdPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(header) == 124);
    file.close();

    const ChdHeaderDigest legacy = readChdHeaderDigest(chdPath);
    QVERIFY(!legacy.valid);
}

QTEST_MAIN(ChdHeaderTest)
#include "test_chd_header.moc"
