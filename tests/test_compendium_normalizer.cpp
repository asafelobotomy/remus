#include <QtTest/QtTest>

#include "../src/metadata/compendium_normalizer.h"
#include "../src/metadata/compendium_types.h"
#include "../src/core/constants/system_ids.h"

using namespace Remus::Compendium;
using namespace Remus::Constants::Systems;

class CompendiumNormalizerTest : public QObject {
    Q_OBJECT

private slots:
    // resolveRegionCode
    void regionCode_usa();
    void regionCode_europe();
    void regionCode_japan();
    void regionCode_ntscU();
    void regionCode_pal();
    void regionCode_ntscJ();
    void regionCode_world();
    void regionCode_commaDelimited();
    void regionCode_empty();
    void regionCode_unknown();

    // resolveSystemId
    void systemId_nes();
    void systemId_genesis();
    void systemId_psx();
    void systemId_empty();
    void systemId_unknown();

    // normalize
    void normalize_setsSystemAndRegion();
    void normalize_unknownHintsProduceZeroAndEmpty();
    void normalize_normalizesRegionInFields();
    void normalize_removesUnmappableRegionFromFields();
};

// ── resolveRegionCode ─────────────────────────────────────────────────────────

void CompendiumNormalizerTest::regionCode_usa() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("USA")), QStringLiteral("USA"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("us")), QStringLiteral("USA"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("NTSC-U")), QStringLiteral("USA"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("America")), QStringLiteral("USA"));
}

void CompendiumNormalizerTest::regionCode_europe() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("Europe")), QStringLiteral("EUR"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("EUR")), QStringLiteral("EUR"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("PAL")), QStringLiteral("EUR"));
}

void CompendiumNormalizerTest::regionCode_japan() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("Japan")), QStringLiteral("JPN"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("JPN")), QStringLiteral("JPN"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("JP")), QStringLiteral("JPN"));
}

void CompendiumNormalizerTest::regionCode_ntscU() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("NTSC")), QStringLiteral("USA"));
}

void CompendiumNormalizerTest::regionCode_pal() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("PAL-E")), QStringLiteral("EUR"));
}

void CompendiumNormalizerTest::regionCode_ntscJ() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("NTSC-J")), QStringLiteral("JPN"));
}

void CompendiumNormalizerTest::regionCode_world() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("World")), QStringLiteral("WORLD"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("Worldwide")), QStringLiteral("WORLD"));
}

void CompendiumNormalizerTest::regionCode_commaDelimited() {
    // Only the first token is used; "USA, Europe" → "USA"
    CompendiumNormalizer n;
    QCOMPARE(n.resolveRegionCode(QStringLiteral("USA, Europe")), QStringLiteral("USA"));
    QCOMPARE(n.resolveRegionCode(QStringLiteral("Japan, USA")), QStringLiteral("JPN"));
}

void CompendiumNormalizerTest::regionCode_empty() {
    CompendiumNormalizer n;
    QVERIFY(n.resolveRegionCode(QString()).isEmpty());
    QVERIFY(n.resolveRegionCode(QStringLiteral("")).isEmpty());
}

void CompendiumNormalizerTest::regionCode_unknown() {
    CompendiumNormalizer n;
    QVERIFY(n.resolveRegionCode(QStringLiteral("Narnia")).isEmpty());
    QVERIFY(n.resolveRegionCode(QStringLiteral("XYZ")).isEmpty());
}

// ── resolveSystemId ───────────────────────────────────────────────────────────

void CompendiumNormalizerTest::systemId_nes() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveSystemId(QStringLiteral("Nintendo - Nintendo Entertainment System")), ID_NES);
}

void CompendiumNormalizerTest::systemId_genesis() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveSystemId(QStringLiteral("Sega - Mega Drive - Genesis")), ID_GENESIS);
}

void CompendiumNormalizerTest::systemId_psx() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveSystemId(QStringLiteral("Sony - PlayStation")), ID_PSX);
}

void CompendiumNormalizerTest::systemId_empty() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveSystemId(QString()), 0);
}

void CompendiumNormalizerTest::systemId_unknown() {
    CompendiumNormalizer n;
    QCOMPARE(n.resolveSystemId(QStringLiteral("Not a Real System")), 0);
}

// ── normalize ────────────────────────────────────────────────────────────────

void CompendiumNormalizerTest::normalize_setsSystemAndRegion() {
    CompendiumNormalizer n;

    SourceRecordEnvelope rec;
    rec.systemHint = QStringLiteral("Nintendo - Nintendo Entertainment System");
    rec.regionRaw = QStringLiteral("USA");

    n.normalize(rec);

    QCOMPARE(rec.resolvedSystemId, ID_NES);
    QCOMPARE(rec.resolvedRegionCode, QStringLiteral("USA"));
}

void CompendiumNormalizerTest::normalize_unknownHintsProduceZeroAndEmpty() {
    CompendiumNormalizer n;

    SourceRecordEnvelope rec;
    rec.systemHint = QStringLiteral("Imaginary Console 9000");
    rec.regionRaw = QStringLiteral("Outer Space");

    n.normalize(rec);

    QCOMPARE(rec.resolvedSystemId, 0);
    QVERIFY(rec.resolvedRegionCode.isEmpty());
}

void CompendiumNormalizerTest::normalize_normalizesRegionInFields() {
    // Raw region strings in rec.fields["region"] must be mapped to canonical
    // region_codes so the merge resolver can safely propagate them to
    // games.primary_region_code (which has an FK to regions(region_code)).
    CompendiumNormalizer n;

    SourceRecordEnvelope rec;
    rec.systemHint = QStringLiteral("Sony - PlayStation");
    rec.regionRaw = QStringLiteral("Japan");
    rec.fields.insert(QStringLiteral("region"), QStringLiteral("Japan"));

    n.normalize(rec);

    QCOMPARE(rec.resolvedRegionCode, QStringLiteral("JPN"));
    QCOMPARE(rec.fields.value(QStringLiteral("region")), QStringLiteral("JPN"));
}

void CompendiumNormalizerTest::normalize_removesUnmappableRegionFromFields() {
    // Unmappable region strings must be removed from rec.fields to prevent
    // the merge resolver from writing an invalid value to primary_region_code.
    CompendiumNormalizer n;

    SourceRecordEnvelope rec;
    rec.systemHint = QStringLiteral("Sony - PlayStation");
    rec.regionRaw = QStringLiteral("Narnia");
    rec.fields.insert(QStringLiteral("region"), QStringLiteral("Narnia"));

    n.normalize(rec);

    QVERIFY(rec.resolvedRegionCode.isEmpty());
    QVERIFY(!rec.fields.contains(QStringLiteral("region")));
}

QTEST_MAIN(CompendiumNormalizerTest)
#include "test_compendium_normalizer.moc"
