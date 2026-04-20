/**
 * @file test_metadata_field_gap.cpp
 * @brief Unit tests for computeFieldGap() and enrichMissingFields() skip logic.
 *
 * Verifies that:
 * 1. computeFieldGap() correctly identifies empty / zero fields.
 * 2. All required fields are present in the ProviderFields::REQUIRED_FIELDS set.
 * 3. The CAPABILITIES map covers every required field in at least one provider.
 * 4. Local providers (localdatabase, gametdb) appear in the capability map.
 * 5. Remote providers that lack a required field are correctly identified as
 *    candidates to skip (capability intersection with gap is empty).
 */

#include <QtTest>
#include <QSet>
#include <QString>

#include "../src/metadata/provider_orchestrator.h"
#include "../src/core/constants/provider_fields.h"

using namespace Remus;
using namespace Remus::Constants::ProviderFields;

class MetadataFieldGapTest : public QObject
{
    Q_OBJECT

private slots:
    void fieldGap_emptyMetadata_allRequired();
    void fieldGap_fullMetadata_noGap();
    void fieldGap_partialMetadata_correctGap();
    void requiredFields_containsAll8();
    void capabilities_localProvidersPresent();
    void capabilities_allRequiredFieldsCoveredBySomeProvider();
    void capabilities_intersect_detectsSkipCandidate();
};

// ---------------------------------------------------------------------------

void MetadataFieldGapTest::fieldGap_emptyMetadata_allRequired()
{
    GameMetadata empty;
    const ProviderOrchestrator::FieldSet gap = ProviderOrchestrator::computeFieldGap(empty);
    QCOMPARE(gap, REQUIRED_FIELDS);
}

void MetadataFieldGapTest::fieldGap_fullMetadata_noGap()
{
    GameMetadata full;
    full.title       = QStringLiteral("Test Game");
    full.publisher   = QStringLiteral("Test Pub");
    full.developer   = QStringLiteral("Test Dev");
    full.releaseDate = QStringLiteral("1990-01-01");
    full.genres      = { QStringLiteral("Action") };
    full.players     = 2;
    full.description = QStringLiteral("A fine game.");
    full.boxArtUrl   = QStringLiteral("https://example.com/art.jpg");

    const ProviderOrchestrator::FieldSet gap = ProviderOrchestrator::computeFieldGap(full);
    QVERIFY(gap.isEmpty());
}

void MetadataFieldGapTest::fieldGap_partialMetadata_correctGap()
{
    GameMetadata partial;
    partial.title     = QStringLiteral("Some Title");
    partial.publisher = QStringLiteral("Publisher");
    // developer, releaseDate, genres, players, description, boxArtUrl all empty/zero

    const ProviderOrchestrator::FieldSet gap = ProviderOrchestrator::computeFieldGap(partial);

    QVERIFY(!gap.contains(QLatin1String(TITLE)));
    QVERIFY(!gap.contains(QLatin1String(PUBLISHER)));
    QVERIFY(gap.contains(QLatin1String(DEVELOPER)));
    QVERIFY(gap.contains(QLatin1String(RELEASE_DATE)));
    QVERIFY(gap.contains(QLatin1String(GENRES)));
    QVERIFY(gap.contains(QLatin1String(PLAYERS)));
    QVERIFY(gap.contains(QLatin1String(DESCRIPTION)));
    QVERIFY(gap.contains(QLatin1String(BOX_ART_URL)));
    QCOMPARE(gap.size(), 6);
}

void MetadataFieldGapTest::requiredFields_containsAll8()
{
    QCOMPARE(REQUIRED_FIELDS.size(), 8);
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(TITLE)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(PUBLISHER)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(DEVELOPER)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(RELEASE_DATE)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(GENRES)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(PLAYERS)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(DESCRIPTION)));
    QVERIFY(REQUIRED_FIELDS.contains(QLatin1String(BOX_ART_URL)));
}

void MetadataFieldGapTest::capabilities_localProvidersPresent()
{
    QVERIFY(CAPABILITIES.contains(QStringLiteral("localdatabase")));
    QVERIFY(CAPABILITIES.contains(QStringLiteral("gametdb")));

    // Both local providers must supply box art (the field most commonly missing
    // from remote-only scraping solutions).
    QVERIFY(CAPABILITIES[QStringLiteral("localdatabase")].contains(QLatin1String(BOX_ART_URL)));
    QVERIFY(CAPABILITIES[QStringLiteral("gametdb")].contains(QLatin1String(BOX_ART_URL)));
}

void MetadataFieldGapTest::capabilities_allRequiredFieldsCoveredBySomeProvider()
{
    // Every required field must be reachable through at least one provider.
    QSet<QString> allCovered;
    for (const auto &fields : CAPABILITIES)
        allCovered.unite(fields);

    for (const QString &field : REQUIRED_FIELDS) {
        QVERIFY2(allCovered.contains(field),
                 qPrintable(QStringLiteral("Required field '%1' not covered by any provider").arg(field)));
    }
}

void MetadataFieldGapTest::capabilities_intersect_detectsSkipCandidate()
{
    // Suppose only 'description' is missing.  A provider that cannot supply
    // description should have an empty intersection with the gap.
    const ProviderOrchestrator::FieldSet gap = { QLatin1String(DESCRIPTION) };

    // localdatabase does NOT supply description per the capability map.
    const QSet<QString> &localCaps = CAPABILITIES.value(QStringLiteral("localdatabase"));
    QVERIFY(!localCaps.intersects(gap)); // should be skipped

    // screenscraper DOES supply description.
    const QSet<QString> &ssCaps = CAPABILITIES.value(QStringLiteral("screenscraper"));
    QVERIFY(ssCaps.intersects(gap)); // should NOT be skipped

    // igdb DOES supply description.
    const QSet<QString> &igdbCaps = CAPABILITIES.value(QStringLiteral("igdb"));
    QVERIFY(igdbCaps.intersects(gap));
}

QTEST_MAIN(MetadataFieldGapTest)
#include "test_metadata_field_gap.moc"
