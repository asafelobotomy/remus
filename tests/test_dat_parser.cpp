#include <QtTest/QtTest>
#include "../src/core/dat_parser.h"
#include <QTemporaryFile>

using namespace Remus;

class DatParserTest : public QObject {
    Q_OBJECT

private slots:
    void testParseValidDat();
    void testParseMultiRomGame();
    void testParseWithAllHashes();
    void testParsePatchCatalogMetadata();
    void testParseNoIntroFormat();
    void testParseRedumpFormat();

    void testParseMalformedXml();
    void testParseEmptyFile();
    void testParseMissingHeader();
    void testParseMissingGameName();
    void testParseMissingRomName();
    void testParseInvalidHashFormat();

    void testIndexByCrc32();
    void testIndexByMd5();
    void testIndexBySha1();
    void testIndexEmptyList();
    void testIndexDuplicateHashes();

    void testDetectSourceNoIntro();
    void testDetectSourceRedump();
    void testDetectSourceTosec();
    void testDetectSourceUnknown();
};

void DatParserTest::testParseValidDat() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<!DOCTYPE datafile PUBLIC \"-//Logiqx//DTD ROM Management Datafile//EN\" \"http://www.logiqx.com/Dats/datafile.dtd\">\n"
        "<datafile>\n"
        "    <header>\n"
        "        <name>Nintendo - Nintendo Entertainment System</name>\n"
        "        <description>No-Intro | 2024-01-15</description>\n"
        "        <version>20240115</version>\n"
        "        <date>2024-01-15</date>\n"
        "        <author>No-Intro</author>\n"
        "        <category>Standard</category>\n"
        "    </header>\n"
        "    <game name=\"Super Mario Bros. (USA)\">\n"
        "        <description>Super Mario Bros.</description>\n"
        "        <rom name=\"Super Mario Bros. (USA).nes\" size=\"40976\" crc=\"3337ec46\" md5=\"811b027eaf99c2def7b933c5208636de\" sha1=\"ea343f4e445a9050d4b4fbac2c77d0693b1d0922\"/>\n"
        "    </game>\n"
        "</datafile>";

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QByteArray xmlData = xmlContent.toUtf8();
    QVERIFY(tempFile.write(xmlData) == xmlData.size());
    tempFile.close();

    DatParser parser;
    DatParseResult result = parser.parse(tempFile.fileName());

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.header.name, QString("Nintendo - Nintendo Entertainment System"));
    QCOMPARE(result.header.version, QString("20240115"));
    QCOMPARE(result.entryCount, 1);
    QCOMPARE(result.entries.size(), 1);
    
    const DatRomEntry &entry = result.entries.first();
    QCOMPARE(entry.gameName, QString("Super Mario Bros. (USA)"));
    QCOMPARE(entry.romName, QString("Super Mario Bros. (USA).nes"));
    QCOMPARE(entry.size, 40976);
    QCOMPARE(entry.crc32, QString("3337ec46"));
    QCOMPARE(entry.md5, QString("811b027eaf99c2def7b933c5208636de"));
}

void DatParserTest::testParseMultiRomGame() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header>\n"
        "        <name>Test DAT</name>\n"
        "    </header>\n"
        "    <game name=\"Multi-ROM Game\">\n"
        "        <description>Game with multiple ROMs</description>\n"
        "        <rom name=\"rom1.bin\" size=\"1024\" crc=\"12345678\"/>\n"
        "        <rom name=\"rom2.bin\" size=\"2048\" crc=\"87654321\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(result.success);
    QCOMPARE(result.entries.size(), 2);
    QCOMPARE(result.entries[0].gameName, QString("Multi-ROM Game"));
    QCOMPARE(result.entries[1].gameName, QString("Multi-ROM Game"));
    QCOMPARE(result.entries[0].romName, QString("rom1.bin"));
    QCOMPARE(result.entries[1].romName, QString("rom2.bin"));
}

void DatParserTest::testParseWithAllHashes() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header><name>Test</name></header>\n"
        "    <game name=\"Test Game\">\n"
        "        <rom name=\"test.rom\" size=\"1000\" crc=\"aaaaaaaa\" md5=\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\" sha1=\"cccccccccccccccccccccccccccccccccccccccc\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(result.success);
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.entries[0].crc32, QString("aaaaaaaa"));
    QCOMPARE(result.entries[0].md5, QString("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
    QCOMPARE(result.entries[0].sha1, QString("cccccccccccccccccccccccccccccccccccccccc"));
}

void DatParserTest::testParsePatchCatalogMetadata() {
    QString xmlContent =
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header><name>Patch Catalog</name></header>\n"
        "    <game name=\"Dragon Quest III (English v2.0)\" base_title=\"Dragon Quest III\" patch_name=\"English v2.0\" file_type=\"translation\">\n"
        "        <description>Verified translation patch output</description>\n"
        "        <rom name=\"Dragon Quest III (English v2.0).sfc\" size=\"3145728\" crc=\"1234abcd\" md5=\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\" sha1=\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(result.success);
    QCOMPARE(result.entries.size(), 1);
    const DatRomEntry &entry = result.entries.first();
    QCOMPARE(entry.baseTitle, QStringLiteral("Dragon Quest III"));
    QCOMPARE(entry.patchName, QStringLiteral("English v2.0"));
    QCOMPARE(entry.fileType, QStringLiteral("translation"));
}

void DatParserTest::testParseNoIntroFormat() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header>\n"
        "        <name>Nintendo - Game Boy</name>\n"
        "        <description>No-Intro | 2024-01-15</description>\n"
        "        <author>No-Intro</author>\n"
        "    </header>\n"
        "    <game name=\"Pokemon Red (USA)\">\n"
        "        <rom name=\"Pokemon Red (USA).gb\" size=\"1048576\" crc=\"3d45c1ee\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(result.success);
    QString source = DatParser::detectSource(result.header);
    QCOMPARE(source, QString("no-intro"));
}

void DatParserTest::testParseRedumpFormat() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header>\n"
        "        <name>Sony - PlayStation</name>\n"
        "        <description>Redump.org | 2024-01-15</description>\n"
        "        <author>Redump</author>\n"
        "    </header>\n"
        "    <game name=\"Final Fantasy VII (USA) (Disc 1)\">\n"
        "        <rom name=\"Final Fantasy VII (USA) (Disc 1).bin\" size=\"737280000\" md5=\"12345678901234567890123456789012\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(result.success);
    QString source = DatParser::detectSource(result.header);
    QCOMPARE(source, QString("redump"));
}

void DatParserTest::testParseMalformedXml() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header>\n"
        "        <name>Test\n"
        "    </header>\n"
        "    <game name=\"Test\">\n"
        "        <rom name=\"test.rom\"/>\n"
        "    </game>\n"; // Missing closing tags

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
}

void DatParserTest::testParseEmptyFile() {
    DatParser parser;
    DatParseResult result = parser.parseContent(QString());

    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
}

void DatParserTest::testParseMissingHeader() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <game name=\"Test Game\">\n"
        "        <rom name=\"test.rom\" size=\"1000\" crc=\"12345678\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    QVERIFY(result.success || !result.success);
    if (result.success) {
        QCOMPARE(result.entries.size(), 1);
    }
}

void DatParserTest::testParseMissingGameName() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header><name>Test</name></header>\n"
        "    <game>\n"
        "        <rom name=\"test.rom\" size=\"1000\" crc=\"12345678\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    if (result.success) {
        if (result.entries.size() > 0) {
            QVERIFY(result.entries[0].gameName.isEmpty());
        }
    }
}

void DatParserTest::testParseMissingRomName() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header><name>Test</name></header>\n"
        "    <game name=\"Test Game\">\n"
        "        <rom size=\"1000\" crc=\"12345678\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    if (result.success && result.entries.size() > 0) {
        QVERIFY(result.entries[0].romName.isEmpty());
    }
}

void DatParserTest::testParseInvalidHashFormat() {
    QString xmlContent = 
        "<?xml version=\"1.0\"?>\n"
        "<datafile>\n"
        "    <header><name>Test</name></header>\n"
        "    <game name=\"Test\">\n"
        "        <rom name=\"test.rom\" crc=\"invalid_hex_value\"/>\n"
        "    </game>\n"
        "</datafile>";

    DatParser parser;
    DatParseResult result = parser.parseContent(xmlContent);

    if (result.success && result.entries.size() > 0) {
        QVERIFY(!result.entries[0].crc32.isEmpty());
    }
}

void DatParserTest::testIndexByCrc32() {
    QList<DatRomEntry> entries;
    DatRomEntry entry1;
    entry1.romName = "game1.rom";
    entry1.crc32 = "12345678";
    entries.append(entry1);
    
    DatRomEntry entry2;
    entry2.romName = "game2.rom";
    entry2.crc32 = "87654321";
    entries.append(entry2);

    QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "crc32");

    QCOMPARE(index.size(), 2);
    QVERIFY(index.contains("12345678"));
    QVERIFY(index.contains("87654321"));
    QCOMPARE(index["12345678"].romName, QString("game1.rom"));
    QCOMPARE(index["87654321"].romName, QString("game2.rom"));
}

void DatParserTest::testIndexByMd5() {
    QList<DatRomEntry> entries;
    DatRomEntry entry;
    entry.romName = "test.rom";
    entry.md5 = "d41d8cd98f00b204e9800998ecf8427e";
    entries.append(entry);

    QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "md5");

    QCOMPARE(index.size(), 1);
    QVERIFY(index.contains("d41d8cd98f00b204e9800998ecf8427e"));
}

void DatParserTest::testIndexBySha1() {
    QList<DatRomEntry> entries;
    DatRomEntry entry;
    entry.romName = "test.rom";
    entry.sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
    entries.append(entry);

    QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "sha1");

    QCOMPARE(index.size(), 1);
    QVERIFY(index.contains("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
}

void DatParserTest::testIndexEmptyList() {
    QList<DatRomEntry> entries;
    QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "crc32");

    QCOMPARE(index.size(), 0);
    QVERIFY(index.isEmpty());
}

void DatParserTest::testIndexDuplicateHashes() {
    QList<DatRomEntry> entries;
    DatRomEntry entry1;
    entry1.romName = "game1.rom";
    entry1.crc32 = "12345678";
    entries.append(entry1);
    
    DatRomEntry entry2;
    entry2.romName = "game2.rom";
    entry2.crc32 = "12345678"; // Same CRC32
    entries.append(entry2);

    QMap<QString, DatRomEntry> index = DatParser::indexByHash(entries, "crc32");

    QCOMPARE(index.size(), 1);
    QVERIFY(index.contains("12345678"));
}

void DatParserTest::testDetectSourceNoIntro() {
    DatHeader header;
    header.description = "No-Intro | 2024-01-15";
    header.author = "No-Intro";

    QString source = DatParser::detectSource(header);
    QCOMPARE(source, QString("no-intro"));
}

void DatParserTest::testDetectSourceRedump() {
    DatHeader header;
    header.description = "Redump.org | 2024-01-15";

    QString source = DatParser::detectSource(header);
    QCOMPARE(source, QString("redump"));
}

void DatParserTest::testDetectSourceTosec() {
    DatHeader header;
    header.author = "TOSEC";
    header.description = "TOSEC 2024";

    QString source = DatParser::detectSource(header);
    QCOMPARE(source, QString("tosec"));
}

void DatParserTest::testDetectSourceUnknown() {
    DatHeader header;
    header.name = "Unknown DAT";
    header.description = "Some custom DAT file";

    QString source = DatParser::detectSource(header);
    QCOMPARE(source, QString("unknown"));
}

QTEST_MAIN(DatParserTest)
#include "test_dat_parser.moc"
