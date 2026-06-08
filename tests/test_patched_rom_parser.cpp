#include <QtTest/QtTest>

#include "../src/core/patched_rom_parser.h"

using namespace Remus;

class PatchedRomParserTest : public QObject {
    Q_OBJECT

private slots:
    void detectsTranslationTags();
    void detectsHackTags();
    void preservesOfficialReleaseNames();
    void detectsPrototypeTags();
};

void PatchedRomParserTest::detectsTranslationTags() {
    const PatchedRomInfo info = PatchedRomParser::parse("Dragon Quest III (English v2.0)[Addendum].sfc");

    QCOMPARE(info.baseTitle, QStringLiteral("Dragon Quest III"));
    QCOMPARE(info.fileType, QStringLiteral("translation"));
    QVERIFY(info.isPatched);
    QCOMPARE(info.patchName, QStringLiteral("English v2.0 Addendum"));
}

void PatchedRomParserTest::detectsHackTags() {
    const PatchedRomInfo info = PatchedRomParser::parse("Final Fantasy VI (USA) [BNW v2.1].sfc");

    QCOMPARE(info.baseTitle, QStringLiteral("Final Fantasy VI"));
    QCOMPARE(info.fileType, QStringLiteral("hack"));
    QVERIFY(info.isPatched);
    QCOMPARE(info.patchName, QStringLiteral("BNW v2.1"));
}

void PatchedRomParserTest::preservesOfficialReleaseNames() {
    const PatchedRomInfo info = PatchedRomParser::parse("Sonic The Hedgehog (USA, Europe).md");

    QCOMPARE(info.baseTitle, QStringLiteral("Sonic The Hedgehog"));
    QCOMPARE(info.fileType, QStringLiteral("official"));
    QVERIFY(!info.isPatched);
    QVERIFY(info.patchName.isEmpty());
}

void PatchedRomParserTest::detectsPrototypeTags() {
    const PatchedRomInfo info = PatchedRomParser::parse("Star Fox 2 (Proto).sfc");

    QCOMPARE(info.baseTitle, QStringLiteral("Star Fox 2"));
    QCOMPARE(info.fileType, QStringLiteral("prototype"));
    QVERIFY(!info.isPatched);
}

QTEST_MAIN(PatchedRomParserTest)
#include "test_patched_rom_parser.moc"