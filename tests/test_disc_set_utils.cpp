#include <QtTest/QtTest>

#include "../src/core/database_types.h"
#include "../src/core/disc_set_utils.h"

using namespace Remus;

class DiscSetUtilsTest : public QObject {
    Q_OBJECT

private slots:
    void labelPath_prefersArchiveFilename();
    void groupKey_includesSystem();
    void discRowLabel_formatsNumber();
    void gameDiscSetKey_formatsStableKey();
    void applyScanDiscMetadata_setsFilenameKey();
    void applyScanDiscMetadata_clearsSingleDisc();
};

void DiscSetUtilsTest::labelPath_prefersArchiveFilename() {
    const QString label = DiscSetUtils::labelPath(
        QStringLiteral("/roms/Final Fantasy VII (Disc 1).7z"), QString(), QString(), QStringLiteral("game.bin"));
    QVERIFY(DiscSetUtils::isMultiDisc(label));
    QCOMPARE(DiscSetUtils::extractDiscNumber(label), 1);
    QCOMPARE(DiscSetUtils::extractBaseTitle(label), QStringLiteral("Final Fantasy VII"));
}

void DiscSetUtilsTest::groupKey_includesSystem() {
    const QString key
        = DiscSetUtils::groupKey(QStringLiteral("Final Fantasy VII (Disc 2).chd"), QStringLiteral("Sony PlayStation"));
    QVERIFY(key.contains(QStringLiteral("Final Fantasy VII")));
    QVERIFY(key.endsWith(QStringLiteral("|Sony PlayStation")));
}

void DiscSetUtilsTest::discRowLabel_formatsNumber() {
    QCOMPARE(DiscSetUtils::discRowLabel(QStringLiteral("Game (Disc 3).bin"), 3), QStringLiteral("Disc 3"));
}

void DiscSetUtilsTest::gameDiscSetKey_formatsStableKey() {
    QCOMPARE(DiscSetUtils::gameDiscSetKey(42, 7), QStringLiteral("game:42|7"));
}

void DiscSetUtilsTest::applyScanDiscMetadata_setsFilenameKey() {
    FileRecord rec;
    rec.filename = QStringLiteral("Final Fantasy VII (Disc 2).7z");
    rec.currentPath = QStringLiteral("/roms/Final Fantasy VII (Disc 2).7z");
    DiscSetUtils::applyScanDiscMetadata(rec, QStringLiteral("Sony PlayStation"));
    QCOMPARE(rec.discNumber, 2);
    QVERIFY(rec.discSetKey.contains(QStringLiteral("Final Fantasy VII")));
    QVERIFY(rec.discSetKey.endsWith(QStringLiteral("|Sony PlayStation")));
}

void DiscSetUtilsTest::applyScanDiscMetadata_clearsSingleDisc() {
    FileRecord rec;
    rec.filename = QStringLiteral("Chrono Trigger (USA).sfc");
    rec.currentPath = QStringLiteral("/roms/Chrono Trigger (USA).sfc");
    DiscSetUtils::applyScanDiscMetadata(rec, QStringLiteral("Super Nintendo"));
    QCOMPARE(rec.discNumber, 0);
    QVERIFY(rec.discSetKey.isEmpty());
}

QTEST_MAIN(DiscSetUtilsTest)
#include "test_disc_set_utils.moc"
