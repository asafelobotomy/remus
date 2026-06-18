#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QTemporaryFile>

#include "../src/metadata/compendium_dat_extractor.h"

using namespace Remus;

class CompendiumDatExtractorTest : public QObject {
    Q_OBJECT

private:
    QString fixturePath(const QString &name) const {
        const QStringList candidates = {
            QString(REMUS_SOURCE_DIR) + "/tests/fixtures/" + name,
            QDir::currentPath() + "/tests/fixtures/" + name,
            QCoreApplication::applicationDirPath() + "/../../tests/fixtures/" + name,
            QCoreApplication::applicationDirPath() + "/../tests/fixtures/" + name,
        };

        for (const QString &path : candidates) {
            if (QFile::exists(path)) {
                return QDir::cleanPath(path);
            }
        }

        return { };
    }

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(QStringLiteral("Remus"));
        QCoreApplication::setApplicationName(QStringLiteral("RemusTest"));
    }

    void extractNormalizesFixtureEnvelope();
    void extractMissingFileReturnsError();
    void extractSelectsDataTrack();
    void extractMultiTrackKeepsAllDataTracks();
    void extractXmlFallback();
};

void CompendiumDatExtractorTest::extractNormalizesFixtureEnvelope() {
    const QString datPath = fixturePath(QStringLiteral("test_compendium_source.dat"));
    QVERIFY2(!datPath.isEmpty(), "Fixture test_compendium_source.dat not found");

    QString error;
    const QList<Compendium::SourceRecordEnvelope> records = Compendium::DatExtractor::extract(
        datPath, QStringLiteral("test-source"), QStringLiteral("snapshot-001"), error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(records.size(), 1);

    const Compendium::SourceRecordEnvelope &record = records.first();
    QCOMPARE(record.sourceId, QStringLiteral("test-source"));
    QCOMPARE(record.snapshotId, QStringLiteral("snapshot-001"));
    QCOMPARE(record.systemHint, QStringLiteral("Nintendo - GameCube"));
    QCOMPARE(record.titleRaw, QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(record.regionRaw, QStringLiteral("USA"));
    QCOMPARE(record.externalKey,
        QStringLiteral(
            "Nintendo - GameCube|Paper Mario: The Thousand-Year Door (USA)|Paper Mario - The Thousand-Year Door.iso"));

    QVERIFY(record.hashes.crc32.isEmpty());
    QCOMPARE(record.hashes.md5, QStringLiteral("0123456789abcdef0123456789abcdef"));
    QVERIFY(record.hashes.sha1.isEmpty());

    QCOMPARE(record.serials, QStringList { QStringLiteral("G8ME01") });
    QCOMPARE(record.fields.value(QStringLiteral("title")), QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(record.fields.value(QStringLiteral("region")), QStringLiteral("USA"));
    QCOMPARE(record.fields.value(QStringLiteral("publisher")), QStringLiteral("Nintendo"));
    QCOMPARE(record.fields.value(QStringLiteral("developer")), QStringLiteral("Intelligent Systems"));
    QCOMPARE(record.fields.value(QStringLiteral("release_year")), QStringLiteral("2004"));
    QCOMPARE(record.fields.value(QStringLiteral("players_max")), QStringLiteral("1"));
    QCOMPARE(record.fields.value(QStringLiteral("description")),
        QStringLiteral("A turn-based adventure across the Mushroom Kingdom."));

    const QJsonDocument payloadDoc = QJsonDocument::fromJson(record.payloadJson.toUtf8());
    QVERIFY(payloadDoc.isObject());
    const QJsonObject payload = payloadDoc.object();
    QCOMPARE(payload.value(QStringLiteral("system_hint")).toString(), QStringLiteral("Nintendo - GameCube"));
    QCOMPARE(payload.value(QStringLiteral("game_name")).toString(),
        QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(payload.value(QStringLiteral("serial")).toString(), QStringLiteral("G8ME01"));
    QCOMPARE(payload.value(QStringLiteral("md5")).toString(), QStringLiteral("0123456789abcdef0123456789abcdef"));
}

void CompendiumDatExtractorTest::extractMissingFileReturnsError() {
    QString error;
    const QList<Compendium::SourceRecordEnvelope> records
        = Compendium::DatExtractor::extract(QStringLiteral("/nonexistent/test_compendium_source.dat"),
            QStringLiteral("test-source"), QStringLiteral("snapshot-001"), error);

    QVERIFY(records.isEmpty());
    QVERIFY(error.contains(QStringLiteral("DAT file not found")));
}

void CompendiumDatExtractorTest::extractSelectsDataTrack() {
    // Multi-ROM game (Redump PS1 style): .cue + .bin in the same game block.
    // DatExtractor must select the .bin entry as the canonical data track.
    const QString content = QStringLiteral("clrmamepro (\n"
                                           "    name \"PlayStation\"\n"
                                           ")\n"
                                           "game (\n"
                                           "    name \"Test Game (USA)\"\n"
                                           "    serial \"SLUS-99999\"\n"
                                           "    rom ( name \"Test Game (USA).cue\" size 104 crc 00000001 )\n"
                                           "    rom ( name \"Test Game (USA).bin\" size 640000000 crc CAFEBABE )\n"
                                           ")\n");

    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    QVERIFY(tmp.open());
    tmp.write(content.toUtf8());
    tmp.close();

    QString error;
    const QList<Compendium::SourceRecordEnvelope> records = Compendium::DatExtractor::extract(
        tmp.fileName(), QStringLiteral("test-src"), QStringLiteral("snap-001"), error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    // One game → one canonical envelope (the .bin track, not the .cue).
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].titleRaw, QStringLiteral("Test Game (USA)"));
    // The .bin hash should be selected; .cue hash must not appear.
    QCOMPARE(records[0].hashes.crc32, QStringLiteral("CAFEBABE"));
}

void CompendiumDatExtractorTest::extractMultiTrackKeepsAllDataTracks() {
    const QString content = QStringLiteral("clrmamepro (\n"
                                           "    name \"PlayStation\"\n"
                                           ")\n"
                                           "game (\n"
                                           "    name \"Test Game (USA)\"\n"
                                           "    serial \"SLUS-99999\"\n"
                                           "    rom ( name \"Test Game (USA).cue\" size 104 crc 00000001 )\n"
                                           "    rom ( name \"Test Game (USA) (Track 01).bin\" size 100 crc AAAAAAAA )\n"
                                           "    rom ( name \"Test Game (USA) (Track 02).bin\" size 200 crc BBBBBBBB )\n"
                                           ")\n");

    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    QVERIFY(tmp.open());
    tmp.write(content.toUtf8());
    tmp.close();

    QString error;
    const QList<Compendium::SourceRecordEnvelope> records = Compendium::DatExtractor::extract(
        tmp.fileName(), QStringLiteral("test-src"), QStringLiteral("snap-001"), error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(records.size(), 2);
    QCOMPARE(records[0].hashes.crc32, QStringLiteral("AAAAAAAA"));
    QCOMPARE(records[1].hashes.crc32, QStringLiteral("BBBBBBBB"));
    QCOMPARE(records[0].titleRaw, records[1].titleRaw);
}

void CompendiumDatExtractorTest::extractXmlFallback() {
    // Logiqx XML format DAT — ClrMameProParser returns empty, so the extractor
    // must fall back to DatParser (XML) and still produce valid envelopes.
    const QString xmlContent = QStringLiteral("<?xml version=\"1.0\"?>\n"
                                              "<datafile>\n"
                                              "  <header>\n"
                                              "    <name>PlayStation - XML Test</name>\n"
                                              "    <description>XML fallback test</description>\n"
                                              "  </header>\n"
                                              "  <game name=\"Metal Gear Solid (USA)\">\n"
                                              "    <description>Metal Gear Solid</description>\n"
                                              "    <rom name=\"Metal Gear Solid (USA).bin\" size=\"596672160\""
                                              " crc=\"12345678\""
                                              " md5=\"abcdef1234567890abcdef1234567890\""
                                              " sha1=\"da39a3ee5e6b4b0d3255bfef95601890afd80709\"/>\n"
                                              "  </game>\n"
                                              "</datafile>\n");

    QTemporaryFile tmp;
    tmp.setAutoRemove(true);
    QVERIFY(tmp.open());
    tmp.write(xmlContent.toUtf8());
    tmp.close();

    QString error;
    const QList<Compendium::SourceRecordEnvelope> records = Compendium::DatExtractor::extract(
        tmp.fileName(), QStringLiteral("xml-src"), QStringLiteral("snap-xml"), error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].titleRaw, QStringLiteral("Metal Gear Solid (USA)"));
    QCOMPARE(records[0].systemHint, QStringLiteral("PlayStation - XML Test"));
    QCOMPARE(records[0].hashes.crc32, QStringLiteral("12345678"));
    QCOMPARE(records[0].hashes.md5, QStringLiteral("abcdef1234567890abcdef1234567890"));
}

QTEST_MAIN(CompendiumDatExtractorTest)
#include "test_compendium_dat_extractor.moc"