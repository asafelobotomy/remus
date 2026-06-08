#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include "../src/metadata/libretro_metadata_parser.h"

using namespace Remus;

class LibretroMetadataParserTest : public QObject {
    Q_OBJECT

private:
    // Write a DAT file with the given content into dir, return its path
    QString writeDat(QTemporaryDir &dir, const QString &name, const QString &content) {
        QString path = dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return { };
        f.write(content.toUtf8());
        f.close();
        return path;
    }

private slots:
    void testParseGenre();
    void testParseDeveloper();
    void testParsePublisher();
    void testParseMaxUsers();
    void testParseReleaseYear();
    void testMergedLookup();
    void testLoadAll();
    void testMissingDirectory();
    void testClearResetsState();
    void testCaseInsensitiveLookup();
};

// ── Individual type parsing ──────────────────────────────────────

void LibretroMetadataParserTest::testParseGenre() {
    // Arrange
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n"
        "  name \"Test\"\n"
        ")\n"
        "\n"
        "game (\n"
        "  comment \"Sonic The Hedgehog (USA, Europe)\"\n"
        "  genre \"Platform\"\n"
        "  rom ( crc ABCD1234 )\n"
        ")\n"
        "\n"
        "game (\n"
        "  comment \"Street Fighter II (USA)\"\n"
        "  genre \"Fighting\"\n"
        "  rom ( crc 11223344 )\n"
        ")\n");

    // Act
    LibretroMetadataParser parser;
    int count = parser.loadType(dir.path(), "genre");

    // Assert
    QCOMPARE(count, 2);
    QVERIFY(parser.contains("ABCD1234"));
    QCOMPARE(parser.lookup("ABCD1234").genre, QString("Platform"));
    QCOMPARE(parser.lookup("11223344").genre, QString("Fighting"));
}

void LibretroMetadataParserTest::testParseDeveloper() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n  name \"Test\"\n)\n\n"
        "game (\n"
        "  comment \"Sonic (USA)\"\n"
        "  developer \"Sega\"\n"
        "  rom ( crc ABCD1234 )\n"
        ")\n");

    LibretroMetadataParser parser;
    parser.loadType(dir.path(), "developer");
    QCOMPARE(parser.lookup("ABCD1234").developer, QString("Sega"));
}

void LibretroMetadataParserTest::testParsePublisher() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n  name \"Test\"\n)\n\n"
        "game (\n"
        "  comment \"Sonic (USA)\"\n"
        "  publisher \"Sega\"\n"
        "  rom ( crc ABCD1234 )\n"
        ")\n");

    LibretroMetadataParser parser;
    parser.loadType(dir.path(), "publisher");
    QCOMPARE(parser.lookup("ABCD1234").publisher, QString("Sega"));
}

void LibretroMetadataParserTest::testParseMaxUsers() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n  name \"Test\"\n)\n\n"
        "game (\n"
        "  comment \"Street Fighter II (USA)\"\n"
        "  users 2\n"
        "  rom ( crc 11223344 )\n"
        ")\n");

    LibretroMetadataParser parser;
    parser.loadType(dir.path(), "maxusers");
    QCOMPARE(parser.lookup("11223344").maxUsers, 2);
}

void LibretroMetadataParserTest::testParseReleaseYear() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n  name \"Test\"\n)\n\n"
        "game (\n"
        "  comment \"Sonic (USA)\"\n"
        "  releaseyear \"1991\"\n"
        "  rom ( crc ABCD1234 )\n"
        ")\n");

    LibretroMetadataParser parser;
    parser.loadType(dir.path(), "releaseyear");
    QCOMPARE(parser.lookup("ABCD1234").releaseYear, 1991);
}

// ── Merged metadata from multiple types ──────────────────────────

void LibretroMetadataParserTest::testMergedLookup() {
    // Arrange: separate dirs for genre and developer, same CRC
    QTemporaryDir genreDir;
    QTemporaryDir devDir;
    QVERIFY(genreDir.isValid());
    QVERIFY(devDir.isValid());

    writeDat(genreDir, "Test.dat",
        "clrmamepro (\n  name \"Test\"\n)\n\n"
        "game (\n"
        "  comment \"Sonic (USA)\"\n"
        "  genre \"Platform\"\n"
        "  rom ( crc ABCD1234 )\n"
        ")\n");

    writeDat(devDir, "Test.dat",
        "clrmamepro (\n  name \"Test\"\n)\n\n"
        "game (\n"
        "  comment \"Sonic (USA)\"\n"
        "  developer \"Sega\"\n"
        "  rom ( crc ABCD1234 )\n"
        ")\n");

    // Act: load both types into same parser
    LibretroMetadataParser parser;
    parser.loadType(genreDir.path(), "genre");
    parser.loadType(devDir.path(), "developer");

    // Assert: both fields merged under same CRC
    QCOMPARE(parser.size(), 1);
    auto meta = parser.lookup("ABCD1234");
    QCOMPARE(meta.genre, QString("Platform"));
    QCOMPARE(meta.developer, QString("Sega"));
}

// ── loadAll with real directory structure ─────────────────────────

void LibretroMetadataParserTest::testLoadAll() {
    QTemporaryDir root;
    QVERIFY(root.isValid());

    // Create subdirs
    QDir(root.path()).mkpath("genre");
    QDir(root.path()).mkpath("developer");

    writeDat(root, "genre/System.dat",
        "clrmamepro (\n  name \"System\"\n)\n\n"
        "game (\n"
        "  comment \"Game A\"\n"
        "  genre \"RPG\"\n"
        "  rom ( crc AAAA1111 )\n"
        ")\n"
        "game (\n"
        "  comment \"Game B\"\n"
        "  genre \"Action\"\n"
        "  rom ( crc BBBB2222 )\n"
        ")\n");

    writeDat(root, "developer/System.dat",
        "clrmamepro (\n  name \"System\"\n)\n\n"
        "game (\n"
        "  comment \"Game A\"\n"
        "  developer \"DevCo\"\n"
        "  rom ( crc AAAA1111 )\n"
        ")\n");

    LibretroMetadataParser parser;
    int unique = parser.loadAll(root.path());

    QCOMPARE(unique, 2);
    QCOMPARE(parser.lookup("AAAA1111").genre, QString("RPG"));
    QCOMPARE(parser.lookup("AAAA1111").developer, QString("DevCo"));
    QCOMPARE(parser.lookup("BBBB2222").genre, QString("Action"));
    QVERIFY(parser.lookup("BBBB2222").developer.isEmpty());
}

// ── Edge cases ───────────────────────────────────────────────────

void LibretroMetadataParserTest::testMissingDirectory() {
    LibretroMetadataParser parser;
    int result = parser.loadAll("/nonexistent/path/12345");
    QCOMPARE(result, 0);
    QCOMPARE(parser.size(), 0);
}

void LibretroMetadataParserTest::testClearResetsState() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n  name \"T\"\n)\n\n"
        "game (\n  comment \"G\"\n  genre \"X\"\n  rom ( crc 11111111 )\n)\n");

    LibretroMetadataParser parser;
    parser.loadType(dir.path(), "genre");
    QVERIFY(parser.size() > 0);

    parser.clear();
    QCOMPARE(parser.size(), 0);
    QVERIFY(!parser.contains("11111111"));
}

void LibretroMetadataParserTest::testCaseInsensitiveLookup() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    writeDat(dir, "Test.dat",
        "clrmamepro (\n  name \"T\"\n)\n\n"
        "game (\n  comment \"G\"\n  genre \"RPG\"\n  rom ( crc abcd1234 )\n)\n");

    LibretroMetadataParser parser;
    parser.loadType(dir.path(), "genre");

    // Lookup with uppercase should find the entry
    QVERIFY(parser.contains("ABCD1234"));
    QCOMPARE(parser.lookup("abcd1234").genre, QString("RPG"));
}

QTEST_MAIN(LibretroMetadataParserTest)
#include "test_libretro_metadata_parser.moc"
