#include <QtTest/QtTest>

#include "../src/core/disc_set_key.h"
#include "../src/core/disc_title_parser.h"

using namespace Remus;

class DiscTitleParserTest : public QObject {
    Q_OBJECT

private slots:
    void redumpMultiDisc();
    void redumpDiscCount();
    void twinSnakesMergedStyle();
    void shenmueVariantTags();
    void shenmueVariants_shareIdentityBase();
    void residentEvilSplitPath();
    void discSetKey_splitPathDiffers();
    void discSetKey_variantPressingsShareKey();
    void singleDiscUnchanged();
    void normalizeForIdentityStripsDiscTag();
    void discSetKey_isStable();
    void discSetKey_differsByRegion();
    void legacyLibraryKey_matchesDiscSetUtils();
    void tosecDiscOfTotal();
    void audioDiscRole();
};

void DiscTitleParserTest::redumpMultiDisc() {
    const DiscTitleInfo info = DiscTitleParser::parseTitle(QStringLiteral("Final Fantasy VII (USA) (Disc 2)"));
    QVERIFY(info.isMultiDisc);
    QCOMPARE(info.discNumber, 2);
    QCOMPARE(info.baseTitle, QStringLiteral("Final Fantasy VII"));
    QCOMPARE(info.setRole, QStringLiteral("game"));
}

void DiscTitleParserTest::redumpDiscCount() {
    const DiscTitleInfo info
        = DiscTitleParser::parseTitle(QStringLiteral("Panzer Dragoon Saga (PAL) (Disc 3 of 4)"));
    QCOMPARE(info.discNumber, 3);
    QCOMPARE(info.discCount, 4);
}

void DiscTitleParserTest::twinSnakesMergedStyle() {
    const DiscTitleInfo d1
        = DiscTitleParser::parseTitle(QStringLiteral("Metal Gear Solid - The Twin Snakes (USA) (Disc 1)"));
    const DiscTitleInfo d2
        = DiscTitleParser::parseTitle(QStringLiteral("Metal Gear Solid - The Twin Snakes (USA) (Disc 2)"));
    QVERIFY(d1.isMultiDisc && d2.isMultiDisc);
    QCOMPARE(d1.baseTitle, d2.baseTitle);
    QCOMPARE(d1.identityBase, d2.identityBase);
}

void DiscTitleParserTest::shenmueVariantTags() {
    const DiscTitleInfo info
        = DiscTitleParser::parseTitle(QStringLiteral("Shenmue (USA) (Disc 3) [!][1S]"));
    QCOMPARE(info.discNumber, 3);
    QVERIFY(info.setVariant.contains(QStringLiteral("1S")));
}

void DiscTitleParserTest::shenmueVariants_shareIdentityBase() {
    const DiscTitleInfo oneS
        = DiscTitleParser::parseTitle(QStringLiteral("Shenmue (USA) (Disc 3) [!][1S]"));
    const DiscTitleInfo twoS
        = DiscTitleParser::parseTitle(QStringLiteral("Shenmue (USA) (Disc 3) [!][2S]"));
    QCOMPARE(oneS.identityBase, twoS.identityBase);
    QVERIFY(oneS.setVariant != twoS.setVariant);
}

void DiscTitleParserTest::residentEvilSplitPath() {
    const DiscTitleInfo leon
        = DiscTitleParser::parseTitle(QStringLiteral("Resident Evil 2 (USA) (Disc 1) (Leon)"));
    const DiscTitleInfo claire
        = DiscTitleParser::parseTitle(QStringLiteral("Resident Evil 2 (USA) (Disc 2) (Claire)"));
    QCOMPARE(leon.pathSubtitle, QStringLiteral("Leon"));
    QCOMPARE(claire.pathSubtitle, QStringLiteral("Claire"));
    QVERIFY(leon.identityBase != claire.identityBase);
}

void DiscTitleParserTest::singleDiscUnchanged() {
    const DiscTitleInfo info = DiscTitleParser::parseTitle(QStringLiteral("Chrono Trigger (USA).sfc"));
    QVERIFY(!info.isMultiDisc);
    QCOMPARE(info.discNumber, 0);
    QCOMPARE(info.baseTitle, QStringLiteral("Chrono Trigger"));
}

void DiscTitleParserTest::normalizeForIdentityStripsDiscTag() {
    const QString normalized
        = DiscTitleParser::normalizeForIdentity(QStringLiteral("Final Fantasy VIII (Disc 2)"));
    QCOMPARE(normalized, QStringLiteral("final fantasy viii"));
}

void DiscTitleParserTest::discSetKey_isStable() {
    const QString keyA = DiscSetKey::compute(12, QStringLiteral("Final Fantasy VII (USA) (Disc 1)"), QStringLiteral("usa"));
    const QString keyB = DiscSetKey::compute(12, QStringLiteral("Final Fantasy VII (USA) (Disc 2)"), QStringLiteral("usa"));
    QCOMPARE(keyA, keyB);
    QCOMPARE(keyA.size(), 16);
}

void DiscTitleParserTest::discSetKey_differsByRegion() {
    const QString usa = DiscSetKey::compute(12, QStringLiteral("Final Fantasy VII (Disc 1)"), QStringLiteral("usa"));
    const QString pal = DiscSetKey::compute(12, QStringLiteral("Final Fantasy VII (Disc 1)"), QStringLiteral("pal"));
    QVERIFY(usa != pal);
}

void DiscTitleParserTest::discSetKey_splitPathDiffers() {
    const QString leon = DiscSetKey::compute(
        14, QStringLiteral("Resident Evil 2 (USA) (Disc 1) (Leon)"), QStringLiteral("usa"));
    const QString claire = DiscSetKey::compute(
        14, QStringLiteral("Resident Evil 2 (USA) (Disc 2) (Claire)"), QStringLiteral("usa"));
    QVERIFY(leon != claire);
}

void DiscTitleParserTest::discSetKey_variantPressingsShareKey() {
    const QString oneS = DiscSetKey::compute(
        14, QStringLiteral("Shenmue (USA) (Disc 3) [!][1S]"), QStringLiteral("usa"));
    const QString twoS = DiscSetKey::compute(
        14, QStringLiteral("Shenmue (USA) (Disc 3) [!][2S]"), QStringLiteral("usa"));
    QCOMPARE(oneS, twoS);
}

void DiscTitleParserTest::legacyLibraryKey_matchesDiscSetUtils() {
    const QString key = DiscSetKey::legacyLibraryGroupKey(
        QStringLiteral("Final Fantasy VII (Disc 2).chd"), QStringLiteral("Sony PlayStation"));
    QCOMPARE(key, QStringLiteral("Final Fantasy VII|Sony PlayStation"));
}

void DiscTitleParserTest::tosecDiscOfTotal() {
    const DiscTitleInfo info = DiscTitleParser::parseTitle(
        QStringLiteral("Skies of Arcadia v1.002 (2000)(Sega)(US)(Disc 2 of 2)[!]"));
    QCOMPARE(info.discNumber, 2);
    QCOMPARE(info.discCount, 2);
    QVERIFY(info.isMultiDisc);
}

void DiscTitleParserTest::audioDiscRole() {
    const DiscTitleInfo info = DiscTitleParser::parseTitle(
        QStringLiteral("The 7th Guest (1994)(Philips)(EU)(Disc 2 of 2) (The Music, Die Musik)"));
    QCOMPARE(info.setRole, QStringLiteral("audio"));
}

QTEST_MAIN(DiscTitleParserTest)
#include "test_disc_title_parser.moc"
