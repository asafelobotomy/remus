#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/metadata/romhacking_scraper.h"

using namespace Remus;

class RomhackingScraperTest : public QObject {
    Q_OBJECT

private slots:
    void writeCatalogJsonRoundTrip();
    void searchRequiresQuery();
};

void RomhackingScraperTest::writeCatalogJsonRoundTrip() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    ModEntry entry;
    entry.id = QStringLiteral("1234");
    entry.title = QStringLiteral("Example Hack");
    entry.type = QStringLiteral("hack");
    entry.sourceUrl = QStringLiteral("https://www.romhacking.net/hacks/1234/");

    RomhackingScraper scraper;
    const QString outputPath = tempDir.filePath(QStringLiteral("catalog.json"));
    QString error;
    QVERIFY(scraper.writeCatalogJson({ entry }, outputPath, &error));
    QVERIFY(QFileInfo(outputPath).exists());
}

void RomhackingScraperTest::searchRequiresQuery() {
    RomhackingScraper scraper;
    QString error;
    const auto mods = scraper.search({ QString(), QString(), 10 }, &error);
    QVERIFY(mods.isEmpty());
    QVERIFY(error.contains(QStringLiteral("required")));
}

QTEST_MAIN(RomhackingScraperTest)
#include "test_romhacking_scraper.moc"
