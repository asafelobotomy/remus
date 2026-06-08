#include <QtTest/QtTest>
#include "local_database_provider.h"

using namespace Remus;

class ThumbnailUrlTest : public QObject {
    Q_OBJECT

private slots:
    void testSanitizeBasic() {
        // & is replaced with _
        QCOMPARE(LocalDatabaseProvider::sanitizeThumbnailName("Sonic & Knuckles"), QString("Sonic _ Knuckles"));
    }

    void testSanitizeAllInvalidChars() {
        // All invalid chars: & * / : \ < > ? | "
        QString input = "A&B*C/D:E\\F<G>H?I|J\"K";
        QString expected = "A_B_C_D_E_F_G_H_I_J_K";
        QCOMPARE(LocalDatabaseProvider::sanitizeThumbnailName(input), expected);
    }

    void testSanitizeNoChange() {
        // Normal name passes through unchanged
        QString name = "Sonic The Hedgehog (USA, Europe)";
        QCOMPARE(LocalDatabaseProvider::sanitizeThumbnailName(name), name);
    }

    void testBuildThumbnailUrl() {
        QString url = LocalDatabaseProvider::buildThumbnailUrl(
            "Sega - Mega Drive - Genesis", "Sonic The Hedgehog (World)", "Named_Boxarts");
        QVERIFY(url.startsWith("https://thumbnails.libretro.com/"));
        QVERIFY(url.contains("Sega%20-%20Mega%20Drive%20-%20Genesis"));
        QVERIFY(url.contains("Named_Boxarts"));
        QVERIFY(url.endsWith(".png"));
    }

    void testBuildThumbnailUrlEncodesSpecialChars() {
        // Game name with & should be sanitized to _ then URL-encoded
        QString url = LocalDatabaseProvider::buildThumbnailUrl(
            "Sega - Mega Drive - Genesis", "Sonic & Knuckles (World)", "Named_Boxarts");
        // & was replaced with _ in filename, so no %26 in URL
        QVERIFY(!url.contains("%26"));
        QVERIFY(url.contains("Sonic%20_%20Knuckles"));
    }

    void testBuildThumbnailUrlTypes() {
        QString base = "Nintendo - Game Boy Advance";
        QString game = "Pokemon (USA)";

        QString boxart = LocalDatabaseProvider::buildThumbnailUrl(base, game, "Named_Boxarts");
        QString snap = LocalDatabaseProvider::buildThumbnailUrl(base, game, "Named_Snaps");
        QString title = LocalDatabaseProvider::buildThumbnailUrl(base, game, "Named_Titles");

        QVERIFY(boxart.contains("Named_Boxarts"));
        QVERIFY(snap.contains("Named_Snaps"));
        QVERIFY(title.contains("Named_Titles"));
    }

    void testGetArtworkReturnsUrls() {
        LocalDatabaseProvider provider;

        // Create a temp DAT file
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString datContent = "clrmamepro (\n"
                             "  name \"Test System\"\n"
                             "  version \"2026.01.01\"\n"
                             ")\n\n"
                             "game (\n"
                             "  name \"Test Game (USA)\"\n"
                             "  rom ( name \"test.bin\" size 131072 crc AABB1122 )\n"
                             ")\n";

        QString datPath = tempDir.path() + "/Test System.dat";
        QFile file(datPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(datContent.toUtf8());
        file.close();

        provider.loadDatabase(datPath);

        ArtworkUrls artwork = provider.getArtwork("AABB1122");
        QVERIFY(!artwork.boxFront.isEmpty());
        QVERIFY(!artwork.screenshot.isEmpty());
        QVERIFY(!artwork.titleScreen.isEmpty());

        QVERIFY(artwork.boxFront.toString().contains("Named_Boxarts"));
        QVERIFY(artwork.screenshot.toString().contains("Named_Snaps"));
        QVERIFY(artwork.titleScreen.toString().contains("Named_Titles"));
        QVERIFY(artwork.boxFront.toString().contains("Test System"));
    }

    void testGetArtworkNotFound() {
        LocalDatabaseProvider provider;
        ArtworkUrls artwork = provider.getArtwork("DEADBEEF");
        QVERIFY(artwork.boxFront.isEmpty());
    }

    void testGetByHashIncludesArtworkUrls() {
        LocalDatabaseProvider provider;

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString datContent = "clrmamepro (\n"
                             "  name \"Sega - Genesis\"\n"
                             "  version \"2026.01.01\"\n"
                             ")\n\n"
                             "game (\n"
                             "  name \"Sonic (World)\"\n"
                             "  rom ( name \"sonic.bin\" size 524288 crc CCDD3344 )\n"
                             ")\n";

        QString datPath = tempDir.path() + "/Sega - Genesis.dat";
        QFile file(datPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(datContent.toUtf8());
        file.close();

        provider.loadDatabase(datPath);

        GameMetadata md = provider.getByHash("CCDD3344", "");
        QCOMPARE(md.title, QString("Sonic (World)"));
        QCOMPARE(md.system, QString("Sega - Genesis"));
        QVERIFY(!md.boxArtUrl.isEmpty());
        QVERIFY(md.boxArtUrl.contains("Named_Boxarts"));
        QCOMPARE(md.screenshotUrls.size(), 2);
    }

    void testArtworkUrlWithParentheses() {
        // Parentheses are valid in URI paths (RFC 3986) and are not encoded
        QString url = LocalDatabaseProvider::buildThumbnailUrl("Nintendo - NES", "Super Mario (USA)", "Named_Boxarts");
        QVERIFY(url.contains("Super%20Mario%20(USA)"));
    }

    void testStripLanguageTagsSingleCode() {
        // Single language code like (En) should be removed
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("007 Shitou - The Duel (Japan) (En)"),
            QString("007 Shitou - The Duel (Japan)"));
    }

    void testStripLanguageTagsMultipleCodes() {
        // Multi-code groups like (En,Ja) and (En,Fr,De,Es,It) should be removed
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("Streets of Rage (World) (En,Ja)"),
            QString("Streets of Rage (World)"));
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("Game (Europe) (En,Fr,De,Es,It)"), QString("Game (Europe)"));
    }

    void testStripLanguageTagsPreservesRegions() {
        // Region tags like (USA), (Japan), (World) are NOT language codes
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("Sonic (USA)"), QString("Sonic (USA)"));
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("Game (Japan)"), QString("Game (Japan)"));
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("Game (World)"), QString("Game (World)"));
    }

    void testStripLanguageTagsMixed() {
        // Preserves region + revision while stripping language codes
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags("Altered Beast (Japan, USA) (En) (Rev A)"),
            QString("Altered Beast (Japan, USA) (Rev A)"));
    }

    void testStripLanguageTagsNoTags() {
        // Name without language tags is returned unchanged
        QString name = "Sonic The Hedgehog (USA, Europe)";
        QCOMPARE(LocalDatabaseProvider::stripLanguageTags(name), name);
    }

    void testGenerateCandidatesExactOnly() {
        // Name without language tags produces exactly 1 candidate
        QStringList candidates = LocalDatabaseProvider::generateThumbnailCandidates(
            "Sega - Mega Drive - Genesis", "Sonic (USA)", "Named_Boxarts");
        QCOMPARE(candidates.size(), 1);
        QVERIFY(candidates.first().contains("Sonic%20(USA)"));
    }

    void testGenerateCandidatesWithFallback() {
        // Name with language tag produces 2 candidates: exact then stripped
        QStringList candidates = LocalDatabaseProvider::generateThumbnailCandidates(
            "Sega - Mega Drive - Genesis", "007 Shitou - The Duel (Japan) (En)", "Named_Boxarts");
        QCOMPARE(candidates.size(), 2);
        // First: exact DAT name
        QVERIFY(candidates.at(0).contains("007%20Shitou%20-%20The%20Duel%20(Japan)%20(En)"));
        // Second: language tag stripped
        QVERIFY(candidates.at(1).contains("007%20Shitou%20-%20The%20Duel%20(Japan)"));
        QVERIFY(!candidates.at(1).contains("(En)"));
    }

    void testGetByHashWithLanguageTagIncludesFallback() {
        LocalDatabaseProvider provider;

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // DAT with a game name that has a language tag
        QString datContent = "clrmamepro (\n"
                             "  name \"Sega - Mega Drive - Genesis\"\n"
                             "  version \"2026.01.01\"\n"
                             ")\n\n"
                             "game (\n"
                             "  name \"Test Game (Japan) (En)\"\n"
                             "  rom ( name \"test.bin\" size 131072 crc EEFF5566 )\n"
                             ")\n";

        QString datPath = tempDir.path() + "/Sega - Mega Drive - Genesis.dat";
        QFile file(datPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(datContent.toUtf8());
        file.close();

        provider.loadDatabase(datPath);

        GameMetadata md = provider.getByHash("EEFF5566", "");
        QVERIFY(!md.boxArtUrl.isEmpty());
        // Primary box art uses exact name
        QVERIFY(md.boxArtUrl.contains("(En)"));
        // screenshotUrls: snap(1) + title(1) + boxart fallback(1) = 3
        QCOMPARE(md.screenshotUrls.size(), 3);
        // Last entry is the box art fallback without language tag
        QVERIFY(!md.screenshotUrls.last().contains("(En)"));
        QVERIFY(md.screenshotUrls.last().contains("Named_Boxarts"));
    }
};

QTEST_MAIN(ThumbnailUrlTest)
#include "test_thumbnail_urls.moc"
