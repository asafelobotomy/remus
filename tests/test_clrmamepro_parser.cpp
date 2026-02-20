#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "../src/metadata/clrmamepro_parser.h"

using namespace Remus;

/**
 * @brief Unit tests for ClrMameProParser
 *
 * Tests ClrMamePro DAT file parsing with:
 * - Single-game DAT files
 * - Multi-ROM game blocks
 * - Full hash coverage (CRC32, MD5, SHA1)
 * - Header parsing
 * - Malformed / empty input handling
 */
class ClrMameProParserTest : public QObject {
    Q_OBJECT

private:
    // Write content to a temporary file and return its path.
    // The QTemporaryFile is opened, written, and closed; the caller owns it.
    QString writeTempDat(QTemporaryFile &tmp, const QString &content)
    {
        if (!tmp.open()) return {};
        tmp.write(content.toUtf8());
        tmp.close();
        return tmp.fileName();
    }

private slots:
    // ── Parsing happy-path tests ──────────────────────────────────
    void testParseSingleGame();
    void testParseMultipleGames();
    void testParseAllHashFields();
    void testParseRegionExtracted();

    // ── Header parsing tests ──────────────────────────────────────
    void testParseHeader();
    void testParseHeaderMissingBlock();

    // ── Edge-case / error handling ────────────────────────────────
    void testParseEmptyFile();
    void testParseNonExistentFile();
    void testParseNoGameBlocks();
};

// ─────────────────────────────────────────────────────────────────
// Happy-path tests
// ─────────────────────────────────────────────────────────────────

void ClrMameProParserTest::testParseSingleGame()
{
    QString content =
        "clrmamepro (\n"
        "    name \"Test System\"\n"
        "    description \"Test\"\n"
        ")\n"
        "game (\n"
        "    name \"Sonic The Hedgehog (USA)\"\n"
        "    description \"Sonic The Hedgehog\"\n"
        "    rom ( name \"Sonic The Hedgehog (USA).md\" size 524288 crc F9394E97 )\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    QList<ClrMameProEntry> entries = ClrMameProParser::parse(path);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].gameName, QString("Sonic The Hedgehog (USA)"));
    QCOMPARE(entries[0].crc32.toLower(), QString("f9394e97"));
    QCOMPARE(entries[0].size, qint64(524288));
}

void ClrMameProParserTest::testParseMultipleGames()
{
    QString content =
        "game (\n"
        "    name \"Game One (USA)\"\n"
        "    description \"Game One\"\n"
        "    rom ( name \"Game One (USA).md\" size 1048576 crc AABBCCDD )\n"
        ")\n"
        "game (\n"
        "    name \"Game Two (Europe)\"\n"
        "    description \"Game Two\"\n"
        "    rom ( name \"Game Two (Europe).md\" size 2097152 crc 11223344 )\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    QList<ClrMameProEntry> entries = ClrMameProParser::parse(path);

    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries[0].gameName, QString("Game One (USA)"));
    QCOMPARE(entries[1].gameName, QString("Game Two (Europe)"));
}

void ClrMameProParserTest::testParseAllHashFields()
{
    QString content =
        "game (\n"
        "    name \"Hash Test Game (USA)\"\n"
        "    description \"Hash Test\"\n"
        "    rom ( name \"test.md\" size 100 crc DEADBEEF "
        "md5 0123456789abcdef0123456789abcdef "
        "sha1 da39a3ee5e6b4b0d3255bfef95601890afd80709 )\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    QList<ClrMameProEntry> entries = ClrMameProParser::parse(path);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].crc32.toLower(), QString("deadbeef"));
    QCOMPARE(entries[0].md5.toLower(), QString("0123456789abcdef0123456789abcdef"));
    QCOMPARE(entries[0].sha1.toLower(),
             QString("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
}

void ClrMameProParserTest::testParseRegionExtracted()
{
    QString content =
        "game (\n"
        "    name \"Super Contra (USA, Europe)\"\n"
        "    description \"Super Contra\"\n"
        "    rom ( name \"Super Contra (USA, Europe).md\" size 512000 crc CAFEBABE )\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    QList<ClrMameProEntry> entries = ClrMameProParser::parse(path);

    QCOMPARE(entries.size(), 1);
    // Region is extracted from the parenthetical portion of the name
    QVERIFY(!entries[0].region.isEmpty());
}

// ─────────────────────────────────────────────────────────────────
// Header parsing tests
// ─────────────────────────────────────────────────────────────────

void ClrMameProParserTest::testParseHeader()
{
    QString content =
        "clrmamepro (\n"
        "    name \"Sega - Mega Drive\"\n"
        "    description \"No-Intro | 2024-01-01\"\n"
        "    version \"20240101\"\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    QMap<QString, QString> header = ClrMameProParser::parseHeader(path);

    QVERIFY(header.contains("name"));
    QCOMPARE(header["name"], QString("Sega - Mega Drive"));
}

void ClrMameProParserTest::testParseHeaderMissingBlock()
{
    // DAT file without a clrmamepro header block
    QString content =
        "game (\n"
        "    name \"Some Game (USA)\"\n"
        "    rom ( name \"game.md\" size 1 crc FFFFFFFF )\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    // Should return an empty map, not crash
    QMap<QString, QString> header = ClrMameProParser::parseHeader(path);
    QVERIFY(header.isEmpty());
}

// ─────────────────────────────────────────────────────────────────
// Edge-case / error-handling tests
// ─────────────────────────────────────────────────────────────────

void ClrMameProParserTest::testParseEmptyFile()
{
    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, "");
    QVERIFY(!path.isEmpty());

    QList<ClrMameProEntry> entries = ClrMameProParser::parse(path);
    QVERIFY(entries.isEmpty());
}

void ClrMameProParserTest::testParseNonExistentFile()
{
    // Parser must return empty list rather than crash on missing file
    QList<ClrMameProEntry> entries =
        ClrMameProParser::parse("/nonexistent/path/to/file.dat");
    QVERIFY(entries.isEmpty());
}

void ClrMameProParserTest::testParseNoGameBlocks()
{
    // Valid header, no game entries
    QString content =
        "clrmamepro (\n"
        "    name \"Empty System\"\n"
        ")\n";

    QTemporaryFile tmp;
    QString path = writeTempDat(tmp, content);
    QVERIFY(!path.isEmpty());

    QList<ClrMameProEntry> entries = ClrMameProParser::parse(path);
    QVERIFY(entries.isEmpty());
}

QTEST_MAIN(ClrMameProParserTest)
#include "test_clrmamepro_parser.moc"

