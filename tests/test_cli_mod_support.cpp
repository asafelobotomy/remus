#include <QtTest/QtTest>

#include <QCommandLineParser>

#include "../src/cli/cli_mod_support.h"

using namespace Remus;

class CliModSupportTest : public QObject
{
    Q_OBJECT

private slots:
    void parseOptions_acceptsJsonAliasAndSort();
    void parseOptions_rejectsInvalidSort();
    void filterCatalogMods_appliesCombinedFilters();
    void sortListedMods_sortsByRatingDescending();
    void sortListedMods_sortsByTitleAscending();
};

namespace {

void configureParser(QCommandLineParser &parser, const QStringList &args)
{
    parser.addOption(QCommandLineOption({"json", "mod-json"}, ""));
    parser.addOption(QCommandLineOption("mod-system", "", "system"));
    parser.addOption(QCommandLineOption("mod-author", "", "author"));
    parser.addOption(QCommandLineOption("mod-type", "", "type"));
    parser.addOption(QCommandLineOption("mod-format", "", "format"));
    parser.addOption(QCommandLineOption("mod-source-url", "", "text"));
    parser.addOption(QCommandLineOption("mod-sort", "", "field"));
    parser.addOption(QCommandLineOption("mod-min-rating", "", "rating"));
    parser.addOption(QCommandLineOption("mod-min-downloads", "", "count"));
    parser.addOption(QCommandLineOption("mod-no-system-fallback", ""));
    parser.process(args);
}

ModEntry makeMod(const QString &id,
                 const QString &title,
                 const QString &author,
                 const QString &type,
                 const QString &format,
                 const QString &system,
                 double         rating,
                 int            downloads,
                 const QString &sourceUrl = QString())
{
    ModEntry mod;
    mod.id = id;
    mod.title = title;
    mod.author = author;
    mod.type = type;
    mod.format = format;
    mod.system = system;
    mod.rating = rating;
    mod.downloads = downloads;
    mod.sourceUrl = sourceUrl;
    return mod;
}

} // namespace

void CliModSupportTest::parseOptions_acceptsJsonAliasAndSort()
{
    QCommandLineParser parser;
    configureParser(parser, {QStringLiteral("test"), QStringLiteral("--json"), QStringLiteral("--mod-sort"), QStringLiteral("downloads")});

    ModQueryOptions options;
    QString error;
    QVERIFY(parseModQueryOptions(parser, options, error));
    QVERIFY(error.isEmpty());
    QVERIFY(options.jsonOutput);
    QCOMPARE(options.sortBy, QStringLiteral("downloads"));
}

void CliModSupportTest::parseOptions_rejectsInvalidSort()
{
    QCommandLineParser parser;
    configureParser(parser, {QStringLiteral("test"), QStringLiteral("--mod-sort"), QStringLiteral("unknown")});

    ModQueryOptions options;
    QString error;
    QVERIFY(!parseModQueryOptions(parser, options, error));
    QVERIFY(error.contains("Invalid value for --mod-sort"));
}

void CliModSupportTest::filterCatalogMods_appliesCombinedFilters()
{
    const QList<ModEntry> mods = {
        makeMod("a", "Alpha", "Test Author", "hack", "ips", "SNES", 4.5, 1000, "https://example.com/a"),
        makeMod("b", "Beta", "Other Author", "translation", "bps", "SNES", 3.0, 200, "https://example.com/b"),
        makeMod("c", "Gamma", "Test Author", "hack", "ips", "Genesis", 4.7, 1200, "https://example.com/c")
    };

    ModQueryOptions options;
    options.authorFilter = QStringLiteral("Test");
    options.typeFilter = QStringLiteral("hack");
    options.formatFilter = QStringLiteral("ips");
    options.minRating = 4.6;
    options.minDownloads = 1000;

    const auto filtered = filterCatalogMods(mods, options);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().id, QStringLiteral("c"));
}

void CliModSupportTest::sortListedMods_sortsByRatingDescending()
{
    QList<ListedMod> rows = {
        {makeMod("a", "Alpha", "A", "hack", "ips", "SNES", 3.2, 100), {}},
        {makeMod("b", "Beta", "B", "hack", "ips", "SNES", 4.8, 100), {}},
        {makeMod("c", "Gamma", "C", "hack", "ips", "SNES", 4.1, 100), {}}
    };

    ModQueryOptions options;
    options.sortBy = QStringLiteral("rating");

    rows = sortListedMods(rows, options);
    QCOMPARE(rows.at(0).mod.id, QStringLiteral("b"));
    QCOMPARE(rows.at(1).mod.id, QStringLiteral("c"));
    QCOMPARE(rows.at(2).mod.id, QStringLiteral("a"));
}

void CliModSupportTest::sortListedMods_sortsByTitleAscending()
{
    QList<ListedMod> rows = {
        {makeMod("c", "Gamma", "C", "hack", "ips", "SNES", 3.2, 100), {}},
        {makeMod("a", "Alpha", "A", "hack", "ips", "SNES", 4.8, 100), {}},
        {makeMod("b", "Beta", "B", "hack", "ips", "SNES", 4.1, 100), {}}
    };

    ModQueryOptions options;
    options.sortBy = QStringLiteral("title");

    rows = sortListedMods(rows, options);
    QCOMPARE(rows.at(0).mod.title, QStringLiteral("Alpha"));
    QCOMPARE(rows.at(1).mod.title, QStringLiteral("Beta"));
    QCOMPARE(rows.at(2).mod.title, QStringLiteral("Gamma"));
}

QTEST_MAIN(CliModSupportTest)
#include "test_cli_mod_support.moc"