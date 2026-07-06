#include "controllers/compendium_build_options.h"

#include <QTest>

using namespace Remus;

class CompendiumBuildOptionsTest : public QObject {
    Q_OBJECT

private slots:
    void strictOfflineDisablesOnlineAll();
    void fullBuildFlagArgsIncludeRecover();
    void extendBuildArgsIncludeSources();
};

void CompendiumBuildOptionsTest::strictOfflineDisablesOnlineAll() {
    CompendiumFullBuildOptions options;
    options.strictOffline = true;
    options.onlineEnrichmentAll = true;
    normalizeFullBuildOptions(options);
    QVERIFY(options.offlineOnly);
    QVERIFY(!options.onlineEnrichmentAll);
}

void CompendiumBuildOptionsTest::fullBuildFlagArgsIncludeRecover() {
    CompendiumFullBuildOptions options;
    options.recover = true;
    const QStringList args = fullBuildFlagArgs(QStringLiteral("/tmp/remus_compendium.db"), options);
    QVERIFY(args.contains(QStringLiteral("--recover")));
    QVERIFY(args.contains(QStringLiteral("--output-db")));
}

void CompendiumBuildOptionsTest::extendBuildArgsIncludeSources() {
    CompendiumExtendBuildOptions options;
    options.enrichSources = { QStringLiteral("igdb"), QStringLiteral("screenscraper") };
    const QStringList args = extendBuildCommandArgs(QStringLiteral("/repo"), QStringLiteral("/repo/build/remus-cli"),
        QStringLiteral("/tmp/remus_compendium.db"), options);
    QCOMPARE(args.count(QStringLiteral("--enrich-source")), 2);
    QVERIFY(args.contains(QStringLiteral("igdb")));
    QVERIFY(args.contains(QStringLiteral("--enrich-compendium")));
}

QTEST_MAIN(CompendiumBuildOptionsTest)
#include "test_compendium_build_options.moc"
