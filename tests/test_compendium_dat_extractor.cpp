#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

#include "../src/metadata/compendium_dat_extractor.h"

using namespace Remus;

class CompendiumDatExtractorTest : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString &name) const
    {
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

        return {};
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Remus"));
        QCoreApplication::setApplicationName(QStringLiteral("RemusTest"));
    }

    void extractNormalizesFixtureEnvelope();
    void extractMissingFileReturnsError();
};

void CompendiumDatExtractorTest::extractNormalizesFixtureEnvelope()
{
    const QString datPath = fixturePath(QStringLiteral("test_compendium_source.dat"));
    QVERIFY2(!datPath.isEmpty(), "Fixture test_compendium_source.dat not found");

    QString error;
    const QList<Compendium::SourceRecordEnvelope> records =
        Compendium::DatExtractor::extract(datPath,
                                          QStringLiteral("test-source"),
                                          QStringLiteral("snapshot-001"),
                                          error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(records.size(), 1);

    const Compendium::SourceRecordEnvelope &record = records.first();
    QCOMPARE(record.sourceId, QStringLiteral("test-source"));
    QCOMPARE(record.snapshotId, QStringLiteral("snapshot-001"));
    QCOMPARE(record.systemHint, QStringLiteral("Nintendo - GameCube"));
    QCOMPARE(record.titleRaw, QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(record.regionRaw, QStringLiteral("USA"));
    QCOMPARE(record.externalKey,
             QStringLiteral("Nintendo - GameCube|Paper Mario: The Thousand-Year Door (USA)|Paper Mario - The Thousand-Year Door.iso"));

    QVERIFY(record.hashes.crc32.isEmpty());
    QCOMPARE(record.hashes.md5, QStringLiteral("0123456789abcdef0123456789abcdef"));
    QVERIFY(record.hashes.sha1.isEmpty());

    QCOMPARE(record.serials, QStringList{QStringLiteral("G8ME01")});
    QCOMPARE(record.fields.value(QStringLiteral("title")),
             QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
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
    QCOMPARE(payload.value(QStringLiteral("md5")).toString(),
             QStringLiteral("0123456789abcdef0123456789abcdef"));
}

void CompendiumDatExtractorTest::extractMissingFileReturnsError()
{
    QString error;
    const QList<Compendium::SourceRecordEnvelope> records =
        Compendium::DatExtractor::extract(QStringLiteral("/nonexistent/test_compendium_source.dat"),
                                          QStringLiteral("test-source"),
                                          QStringLiteral("snapshot-001"),
                                          error);

    QVERIFY(records.isEmpty());
    QVERIFY(error.contains(QStringLiteral("DAT file not found")));
}

QTEST_MAIN(CompendiumDatExtractorTest)
#include "test_compendium_dat_extractor.moc"