#include <QtTest/QtTest>

#include "core/conversion_planner.h"
#include "core/constants/systems.h"

using namespace Remus;

namespace {

QString sanitizeRowName(const QString &extension, const QString &systemName)
{
    QString rowName = extension;
    rowName.remove('.');
    rowName += QStringLiteral("_") + systemName;

    for (QChar &ch : rowName) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }

    return rowName;
}

} // namespace

class ConversionPlannerTest : public QObject
{
    Q_OBJECT

private slots:
    void testEveryRecognizedExtensionPairIsClassified_data();
    void testEveryRecognizedExtensionPairIsClassified();
    void testPs1IsoPrefersChd();
    void testPs1EcmNormalizesBeforeChd();
    void testPs1PbpAutoProcessDoesNotExport();
    void testGameCubeIsoPrefersRvz();
    void testPspIsoPrefersCso();
    void testPspChdNormalizesBeforeCso();
    void testPs2CsoNormalizesBeforeChd();
    void testPs2ElfIsArchiveOnly();
    void testDreamcastCdiNormalizesBeforeChd();
    void testWiiWbfsIsNormalizationFirst();
    void testWiiWadIsArchiveOnly();
    void testPs1PbpIsExportOnly();
    void testNesRomIsArchiveOnly();
    void testSwitchPackageIsDeferred();
};

void ConversionPlannerTest::testEveryRecognizedExtensionPairIsClassified_data()
{
    QTest::addColumn<QString>("extension");
    QTest::addColumn<int>("systemId");

    for (auto it = Constants::Systems::EXTENSION_TO_SYSTEMS.begin();
         it != Constants::Systems::EXTENSION_TO_SYSTEMS.end();
         ++it) {
        for (int systemId : it.value()) {
            const auto *system = Constants::Systems::getSystem(systemId);
            QVERIFY(system != nullptr);

            const QByteArray rowName = sanitizeRowName(it.key(), system->internalName).toUtf8();
            QTest::addRow("%s", rowName.constData()) << it.key() << systemId;
        }
    }
}

void ConversionPlannerTest::testEveryRecognizedExtensionPairIsClassified()
{
    QFETCH(QString, extension);
    QFETCH(int, systemId);

    const ConversionPlanner::Plan plan = ConversionPlanner::plan({systemId, extension});

    QVERIFY(ConversionPlanner::isSystemClassified(systemId));
    QVERIFY(plan.isValid());
    QVERIFY(!ConversionPlanner::toString(plan.role).isEmpty());
    QVERIFY(!ConversionPlanner::toString(plan.action).isEmpty());
    QVERIFY(!plan.reason.isEmpty());
    QVERIFY(!plan.fallbackBehavior.isEmpty());

    if (plan.role != ConversionPlanner::FormatRole::ArchiveOnly
        && plan.role != ConversionPlanner::FormatRole::Deferred) {
        QVERIFY(!plan.canonicalExtension.isEmpty());
    }
}

void ConversionPlannerTest::testPs1IsoPrefersChd()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PSX, QStringLiteral(".iso")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("canonical"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("convert-to-chd"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".chd"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("chdman")));
}

void ConversionPlannerTest::testPs1EcmNormalizesBeforeChd()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PSX, QStringLiteral(".ecm")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("normalization-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("normalize-to-iso"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".chd"));
    QCOMPARE(plan.intermediateExtension, QStringLiteral(".iso"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("chdman")));
}

void ConversionPlannerTest::testPs1PbpAutoProcessDoesNotExport()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PSX, QStringLiteral(".pbp")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("export-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("no-op"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".chd"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("PSXPackager")));
}

void ConversionPlannerTest::testGameCubeIsoPrefersRvz()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_GAMECUBE, QStringLiteral(".iso")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("canonical"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("convert-to-rvz"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".rvz"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("dolphin-tool")));
}

void ConversionPlannerTest::testPspIsoPrefersCso()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PSP, QStringLiteral(".iso")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("canonical"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("convert-to-cso"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".cso"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("maxcso")));
}

void ConversionPlannerTest::testPspChdNormalizesBeforeCso()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PSP, QStringLiteral(".chd")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("normalization-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("normalize-to-iso"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".cso"));
    QCOMPARE(plan.intermediateExtension, QStringLiteral(".iso"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("chdman")));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("maxcso")));
}

void ConversionPlannerTest::testPs2CsoNormalizesBeforeChd()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PS2, QStringLiteral(".cso")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("normalization-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("normalize-to-iso"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".chd"));
    QCOMPARE(plan.intermediateExtension, QStringLiteral(".iso"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("maxcso")));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("chdman")));
}

void ConversionPlannerTest::testPs2ElfIsArchiveOnly()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_PS2, QStringLiteral(".elf")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("archive-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("archive-as-is"));
    QVERIFY(plan.canonicalExtension.isEmpty());
}

void ConversionPlannerTest::testDreamcastCdiNormalizesBeforeChd()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_DREAMCAST, QStringLiteral(".cdi")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("normalization-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("normalize-to-iso"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".chd"));
    QCOMPARE(plan.intermediateExtension, QStringLiteral(".iso"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("chdman")));
}

void ConversionPlannerTest::testWiiWbfsIsNormalizationFirst()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_WII, QStringLiteral(".wbfs")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("normalization-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("normalize-to-iso"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".rvz"));
    QCOMPARE(plan.intermediateExtension, QStringLiteral(".iso"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("wit")));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("dolphin-tool")));
}

void ConversionPlannerTest::testWiiWadIsArchiveOnly()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_WII, QStringLiteral(".wad")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("archive-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("archive-as-is"));
    QVERIFY(plan.canonicalExtension.isEmpty());
}

void ConversionPlannerTest::testPs1PbpIsExportOnly()
{
    ConversionPlanner::Request request;
    request.systemId = Constants::Systems::ID_PSX;
    request.extension = QStringLiteral(".pbp");
    request.intent = ConversionPlanner::PlanningIntent::ExplicitExport;

    const ConversionPlanner::Plan plan = ConversionPlanner::plan(request);

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("export-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("export-pbp"));
    QCOMPARE(plan.canonicalExtension, QStringLiteral(".chd"));
    QVERIFY(plan.requiredTools.contains(QStringLiteral("PSXPackager")));
}

void ConversionPlannerTest::testNesRomIsArchiveOnly()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_NES, QStringLiteral(".nes")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("archive-only"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("archive-as-is"));
    QVERIFY(plan.canonicalExtension.isEmpty());
}

void ConversionPlannerTest::testSwitchPackageIsDeferred()
{
    const ConversionPlanner::Plan plan = ConversionPlanner::plan({Constants::Systems::ID_SWITCH, QStringLiteral(".nsp")});

    QCOMPARE(ConversionPlanner::toString(plan.role), QStringLiteral("deferred"));
    QCOMPARE(ConversionPlanner::toString(plan.action), QStringLiteral("deferred"));
    QVERIFY(plan.canonicalExtension.isEmpty());
}

QTEST_MAIN(ConversionPlannerTest)

#include "test_conversion_planner.moc"