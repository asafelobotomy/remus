#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "../src/metadata/local_database_provider.h"

using namespace Remus;

class LocalDatabaseProviderTest : public QObject
{
    Q_OBJECT

private:
    static bool writeFile(const QString &path, const QByteArray &content)
    {
        const QFileInfo fileInfo(path);
        QDir().mkpath(fileInfo.absolutePath());

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }

        if (file.write(content) != content.size()) {
            return false;
        }

        file.close();
        return true;
    }

private slots:
    void searchByNameFindsSerialOnlyEntries();
    void getByHashMergesCrcAndSerialEnrichment();
};

void LocalDatabaseProviderTest::searchByNameFindsSerialOnlyEntries()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString datPath = tmpDir.filePath(QStringLiteral("Nintendo - GameCube.dat"));
    QVERIFY(writeFile(
        datPath,
        QByteArrayLiteral(
            "clrmamepro (\n"
            "    name \"Nintendo - GameCube\"\n"
            "    description \"GameCube test DAT\"\n"
            ")\n"
            "game (\n"
            "    name \"Paper Mario: The Thousand-Year Door (USA)\"\n"
            "    region \"USA\"\n"
            "    serial \"G8ME01\"\n"
            "    rom ( name \"Paper Mario - The Thousand-Year Door.iso\" size 1459978240 serial \"G8ME01\" )\n"
            ")\n")));

    const QString metadataRoot = tmpDir.filePath(QStringLiteral("metadata"));
    QVERIFY(writeFile(
        metadataRoot + QStringLiteral("/developer/Nintendo - GameCube.dat"),
        QByteArrayLiteral(
            "clrmamepro (\n"
            "  name \"Nintendo - GameCube\"\n"
            ")\n"
            "game (\n"
            "  name \"TTYD Alias\"\n"
            "  serial \"G8ME01\"\n"
            "  developer \"Intelligent Systems\"\n"
            "  publisher \"Nintendo\"\n"
            "  releaseyear \"2004\"\n"
            "  users 1\n"
            ")\n")));

    LocalDatabaseProvider provider;
    QCOMPARE(provider.loadDatabase(datPath), 1);
    provider.loadMetadata(metadataRoot);

    const QList<SearchResult> results = provider.searchByName(QStringLiteral("Paper Mario"), QStringLiteral("GameCube"));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().title, QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(results.first().id, QStringLiteral("G8ME01"));

    const GameMetadata metadata = provider.getById(results.first().id);
    QCOMPARE(metadata.title, QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(metadata.externalIds.value(QStringLiteral("serial")), QStringLiteral("G8ME01"));
    QCOMPARE(metadata.publisher, QStringLiteral("Nintendo"));
    QCOMPARE(metadata.developer, QStringLiteral("Intelligent Systems"));
    QCOMPARE(metadata.releaseDate, QStringLiteral("2004"));
    QCOMPARE(metadata.players, 1);
}

void LocalDatabaseProviderTest::getByHashMergesCrcAndSerialEnrichment()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QString datPath = tmpDir.filePath(QStringLiteral("Nintendo - GameCube.dat"));
    QVERIFY(writeFile(
        datPath,
        QByteArrayLiteral(
            "clrmamepro (\n"
            "    name \"Nintendo - GameCube\"\n"
            "    description \"GameCube test DAT\"\n"
            ")\n"
            "game (\n"
            "    name \"Paper Mario: The Thousand-Year Door (USA)\"\n"
            "    serial \"G8ME01\"\n"
            "    rom ( name \"Paper Mario - The Thousand-Year Door.iso\" size 1459978240 crc AABB1122 serial \"G8ME01\" )\n"
            ")\n")));

    const QString metadataRoot = tmpDir.filePath(QStringLiteral("metadata"));
    QVERIFY(writeFile(
        metadataRoot + QStringLiteral("/genre/Nintendo - GameCube.dat"),
        QByteArrayLiteral(
            "clrmamepro (\n"
            "  name \"Nintendo - GameCube\"\n"
            ")\n"
            "game (\n"
            "  name \"Genre Source\"\n"
            "  genre \"Role-Playing\"\n"
            "  rom ( crc AABB1122 )\n"
            ")\n")));
    QVERIFY(writeFile(
        metadataRoot + QStringLiteral("/developer/Nintendo - GameCube.dat"),
        QByteArrayLiteral(
            "clrmamepro (\n"
            "  name \"Nintendo - GameCube\"\n"
            ")\n"
            "game (\n"
            "  name \"Serial Source\"\n"
            "  serial \"G8ME01\"\n"
            "  developer \"Intelligent Systems\"\n"
            "  publisher \"Nintendo\"\n"
            "  releaseyear \"2004\"\n"
            "  users 1\n"
            ")\n")));

    LocalDatabaseProvider provider;
    QCOMPARE(provider.loadDatabase(datPath), 1);
    provider.loadMetadata(metadataRoot);

    const GameMetadata metadata = provider.getByHash(QStringLiteral("AABB1122"), QStringLiteral("GameCube"));
    QCOMPARE(metadata.title, QStringLiteral("Paper Mario: The Thousand-Year Door (USA)"));
    QCOMPARE(metadata.externalIds.value(QStringLiteral("serial")), QStringLiteral("G8ME01"));
    QCOMPARE(metadata.publisher, QStringLiteral("Nintendo"));
    QCOMPARE(metadata.developer, QStringLiteral("Intelligent Systems"));
    QCOMPARE(metadata.releaseDate, QStringLiteral("2004"));
    QCOMPARE(metadata.players, 1);
    QCOMPARE(metadata.genres, QStringList{QStringLiteral("Role-Playing")});
}

QTEST_MAIN(LocalDatabaseProviderTest)
#include "test_local_database_provider.moc"