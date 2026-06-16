#include <QtTest>
#include "../src/core/verification_hash_matcher.h"

using namespace Remus;
using namespace VerificationHashMatcher;

class VerificationHashMatcherTest : public QObject {
    Q_OBJECT
private slots:
    void orderedOfficialHashTypes_prefersSystemTypeAfterSha256();
    void findHashInDatEntries_fallsBackToMd5WhenPreferredCrcMissing();
    void findHashInDatEntries_patchAndOfficialShareCascade();
};

void VerificationHashMatcherTest::orderedOfficialHashTypes_prefersSystemTypeAfterSha256() {
    const QList<QString> nesOrder = orderedOfficialHashTypes(QStringLiteral("crc32"));
    QCOMPARE(nesOrder.size(), 4);
    QCOMPARE(nesOrder.at(0), QStringLiteral("sha256"));
    QCOMPARE(nesOrder.at(1), QStringLiteral("crc32"));
    QCOMPARE(nesOrder.at(2), QStringLiteral("sha1"));
    QCOMPARE(nesOrder.at(3), QStringLiteral("md5"));

    const QList<QString> psxOrder = orderedOfficialHashTypes(QStringLiteral("md5"));
    QCOMPARE(psxOrder.at(0), QStringLiteral("sha256"));
    QCOMPARE(psxOrder.at(1), QStringLiteral("md5"));
    QCOMPARE(psxOrder.at(2), QStringLiteral("sha1"));
    QCOMPARE(psxOrder.at(3), QStringLiteral("crc32"));
}

void VerificationHashMatcherTest::findHashInDatEntries_fallsBackToMd5WhenPreferredCrcMissing() {
    QMap<QString, DatRomEntry> entries;
    DatRomEntry entry;
    entry.gameName = QStringLiteral("Super Mario Bros.");
    entries.insert(QStringLiteral("811b027eaf99c2def7b933c5208636de"), entry);

    DatRomEntry matchedEntry;
    QString matchedHash;
    QString matchedHashType;
    const bool found = findHashInDatEntries(entries, QStringLiteral("crc32"), QString(),
        QStringLiteral("811b027eaf99c2def7b933c5208636de"), QString(), QString(), matchedEntry, matchedHash,
        matchedHashType);

    QVERIFY(found);
    QCOMPARE(matchedHashType, QStringLiteral("md5"));
    QCOMPARE(matchedHash, QStringLiteral("811b027eaf99c2def7b933c5208636de"));
}

void VerificationHashMatcherTest::findHashInDatEntries_patchAndOfficialShareCascade() {
    QMap<QString, DatRomEntry> official;
    QMap<QString, DatRomEntry> patch;
    DatRomEntry officialEntry;
    officialEntry.gameName = QStringLiteral("Official");
    official.insert(QStringLiteral("7b5e9e81"), officialEntry);

    DatRomEntry patchEntry;
    patchEntry.gameName = QStringLiteral("Patch");
    patch.insert(QStringLiteral("811b027eaf99c2def7b933c5208636de"), patchEntry);

    DatRomEntry matchedEntry;
    QString matchedHash;
    QString matchedHashType;

    const bool officialFound = findHashInDatEntries(official, QStringLiteral("crc32"), QStringLiteral("7b5e9e81"),
        QStringLiteral("811b027eaf99c2def7b933c5208636de"), QString(), QString(), matchedEntry, matchedHash,
        matchedHashType);
    QVERIFY(officialFound);
    QCOMPARE(matchedHashType, QStringLiteral("crc32"));

    const bool patchFound = findHashInDatEntries(patch, QStringLiteral("crc32"), QStringLiteral("7b5e9e81"),
        QStringLiteral("811b027eaf99c2def7b933c5208636de"), QString(), QString(), matchedEntry, matchedHash,
        matchedHashType);
    QVERIFY(patchFound);
    QCOMPARE(matchedHashType, QStringLiteral("md5"));
}

QTEST_MAIN(VerificationHashMatcherTest)
#include "test_verification_hash_matcher.moc"
