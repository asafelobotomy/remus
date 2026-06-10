#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "gametdb_provider.h"

using namespace Remus;

class GameTDBProviderTest : public QObject {
    Q_OBJECT

private:
    // Minimal Wii-style XML for testing
    static QByteArray sampleXml() {
        return QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                 "<datafile>\n"
                                 "  <game name=\"Mario Kart Wii (Europe) (EN,FR)\">\n"
                                 "    <id>RMCP01</id>\n"
                                 "    <type>Wii</type>\n"
                                 "    <region>PAL</region>\n"
                                 "    <locale lang=\"EN\">\n"
                                 "      <title>Mario Kart Wii</title>\n"
                                 "      <synopsis>Race with Mario and friends.</synopsis>\n"
                                 "    </locale>\n"
                                 "    <locale lang=\"FR\">\n"
                                 "      <title>Mario Kart Wii</title>\n"
                                 "      <synopsis>Texte francais.</synopsis>\n"
                                 "    </locale>\n"
                                 "    <developer>Nintendo EAD</developer>\n"
                                 "    <publisher>Nintendo</publisher>\n"
                                 "    <date year=\"2008\" month=\"4\" day=\"11\"/>\n"
                                 "    <genre>racing, party</genre>\n"
                                 "    <input players=\"4\"/>\n"
                                 "    <rom version=\"0\" size=\"4699979776\""
                                 "         crc=\"3a70d78b\""
                                 "         md5=\"e7b1ff1fabb0789482ce2cb0661d986e\""
                                 "         sha1=\"4fde5ce38d68f632d92c5b0d52bbab0b1d355a95\"/>\n"
                                 "  </game>\n"
                                 "  <game name=\"Super Mario Sunshine (USA) (EN)\">\n"
                                 "    <id>GMCE01</id>\n"
                                 "    <type>GameCube</type>\n"
                                 "    <region>NTSC-U</region>\n"
                                 "    <locale lang=\"EN\">\n"
                                 "      <title>Super Mario Sunshine</title>\n"
                                 "      <synopsis>Mario cleans up Isle Delfino.</synopsis>\n"
                                 "    </locale>\n"
                                 "    <developer>Nintendo EAD</developer>\n"
                                 "    <publisher>Nintendo</publisher>\n"
                                 "    <date year=\"2002\" month=\"7\" day=\"19\"/>\n"
                                 "    <genre>action, platformer</genre>\n"
                                 "    <input players=\"1\"/>\n"
                                 "    <rom version=\"0\" size=\"1459978240\" crc=\"AABB1122\"/>\n"
                                 "  </game>\n"
                                 "  <game name=\"WiiWare Title (World) (EN)\">\n"
                                 "    <id>NOROM</id>\n"
                                 "    <type>Wii</type>\n"
                                 "    <region>ALL</region>\n"
                                 "    <locale lang=\"EN\">\n"
                                 "      <title>WiiWare Title</title>\n"
                                 "      <synopsis>A digital-only game.</synopsis>\n"
                                 "    </locale>\n"
                                 "    <developer>Indie Dev</developer>\n"
                                 "    <publisher>Indie Pub</publisher>\n"
                                 "    <genre>puzzle</genre>\n"
                                 "    <input players=\"2\"/>\n"
                                 "  </game>\n"
                                 "  <game name=\"Japanese Only Game (Japan) (JA)\">\n"
                                 "    <id>JPONLY</id>\n"
                                 "    <type>Wii</type>\n"
                                 "    <region>NTSC-J</region>\n"
                                 "    <locale lang=\"JA\">\n"
                                 "      <title>Japanese Only Game</title>\n"
                                 "      <synopsis>Japan exclusive title.</synopsis>\n"
                                 "    </locale>\n"
                                 "    <developer>JP Dev</developer>\n"
                                 "    <publisher>JP Pub</publisher>\n"
                                 "    <input players=\"1\"/>\n"
                                 "    <rom crc=\"DEADBEEF\"/>\n"
                                 "  </game>\n"
                                 "</datafile>\n");
    }

    QString writeSampleXml(const QString &dir) {
        QString path = dir + "/wiitdb.xml";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(sampleXml());
        f.close();
        return path;
    }

private slots:

    void testLoadDatabase() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        QString xmlPath = writeSampleXml(tmpDir.path());
        int count = provider.loadDatabase(xmlPath);
        QCOMPARE(count, 4);
    }

    void testLoadDatabases() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        writeSampleXml(tmpDir.path());

        GameTDBProvider provider;
        int count = provider.loadDatabases(tmpDir.path());
        QCOMPARE(count, 4);
    }

    void testGetByHashCRC32() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        GameMetadata md = provider.getByHash("3A70D78B", "");
        QCOMPARE(md.title, QString("Mario Kart Wii"));
        QCOMPARE(md.system, QString("Wii"));
        QCOMPARE(md.developer, QString("Nintendo EAD"));
        QCOMPARE(md.publisher, QString("Nintendo"));
        QCOMPARE(md.players, 4);
        QCOMPARE(md.releaseDate, QString("2008-04-11"));
        QVERIFY(md.genres.contains("racing"));
        QVERIFY(md.genres.contains("party"));
        QCOMPARE(md.providerId, QString("gametdb"));
    }

    void testGetByHashes_fallsBackToMd5WhenCrcMissing() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        const GameMetadata md = provider.getByHashes(QString(), QStringLiteral("e7b1ff1fabb0789482ce2cb0661d986e"),
            QString(), QString(), QStringLiteral("crc32"));
        QCOMPARE(md.title, QStringLiteral("Mario Kart Wii"));
    }

    void testGetByHashMD5() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        // MD5 lookup (lowercase input should still match)
        GameMetadata md = provider.getByHash("e7b1ff1fabb0789482ce2cb0661d986e", "");
        QCOMPARE(md.title, QString("Mario Kart Wii"));
    }

    void testGetByHashSHA1() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        // SHA1 lookup
        GameMetadata md = provider.getByHash("4fde5ce38d68f632d92c5b0d52bbab0b1d355a95", "");
        QCOMPARE(md.title, QString("Mario Kart Wii"));
    }

    void testGetByHashNotFound() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        GameMetadata md = provider.getByHash("0000FFFF", "");
        QVERIFY(md.title.isEmpty());
    }

    void testGetById() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        GameMetadata md = provider.getById("GMCE01");
        QCOMPARE(md.title, QString("Super Mario Sunshine"));
        QCOMPARE(md.system, QString("GameCube"));
        QCOMPARE(md.region, QString("NTSC-U"));
        QCOMPARE(md.players, 1);
        QVERIFY(md.externalIds.contains("gametdb"));
        QCOMPARE(md.externalIds["gametdb"], QString("GMCE01"));
    }

    void testSearchByName() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        auto results = provider.searchByName("Mario", "", "");
        QCOMPARE(results.size(), 2); // Mario Kart Wii + Super Mario Sunshine
    }

    void testSearchByNameRespectsSystemAndRegion() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        auto wiiResults = provider.searchByName("Mario", "Wii", "");
        QCOMPARE(wiiResults.size(), 1);
        QCOMPARE(wiiResults.first().title, QString("Mario Kart Wii"));

        auto gcResults = provider.searchByName("Mario", "GameCube", "NTSC-U");
        QCOMPARE(gcResults.size(), 1);
        QCOMPARE(gcResults.first().title, QString("Super Mario Sunshine"));

        auto palResults = provider.searchByName("Mario", "", "PAL");
        QCOMPARE(palResults.size(), 1);
        QCOMPARE(palResults.first().title, QString("Mario Kart Wii"));
    }

    void testSearchByNameCaseInsensitive() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        auto results = provider.searchByName("mario kart", "", "");
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().title, QString("Mario Kart Wii"));
    }

    void testJapaneseFallbackLocale() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        // JPONLY has only JA locale — should use it as fallback
        GameMetadata md = provider.getByHash("DEADBEEF", "");
        QCOMPARE(md.title, QString("Japanese Only Game"));
        QCOMPARE(md.description, QString("Japan exclusive title."));
    }

    void testCdnPlatformCode() {
        QCOMPARE(GameTDBProvider::cdnPlatformCode("Wii"), QString("wii"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("GameCube"), QString("wii"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("WiiWare"), QString("wii"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("DS"), QString("ds"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("DSiWare"), QString("ds"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("3DS"), QString("3ds"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("3DSWare"), QString("3ds"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("WiiU"), QString("wiiu"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("Switch"), QString("switch"));
        QCOMPARE(GameTDBProvider::cdnPlatformCode("PS3"), QString("ps3"));
        QVERIFY(GameTDBProvider::cdnPlatformCode("Unknown").isEmpty());
    }

    void testCdnRegionCode() {
        QCOMPARE(GameTDBProvider::cdnRegionCode("NTSC-U"), QString("US"));
        QCOMPARE(GameTDBProvider::cdnRegionCode("PAL"), QString("EN"));
        QCOMPARE(GameTDBProvider::cdnRegionCode("NTSC-J"), QString("JA"));
        QCOMPARE(GameTDBProvider::cdnRegionCode("ALL"), QString("US"));
        QCOMPARE(GameTDBProvider::cdnRegionCode("KOR"), QString("KO"));
    }

    void testBuildArtworkUrl() {
        QString url = GameTDBProvider::buildArtworkUrl("wii", "cover", "US", "RMGE01", "png");
        QCOMPARE(url, QString("https://art.gametdb.com/wii/cover/US/RMGE01.png"));
    }

    void testGetArtworkWii() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        ArtworkUrls artwork = provider.getArtwork("RMCP01");
        QVERIFY(!artwork.boxFront.isEmpty());
        QVERIFY(artwork.boxFront.toString().contains("art.gametdb.com"));
        QVERIFY(artwork.boxFront.toString().contains("wii/cover/EN/RMCP01.png"));

        // Wii should also have 3D box
        QVERIFY(!artwork.boxFull.isEmpty());
        QVERIFY(artwork.boxFull.toString().contains("cover3D"));
    }

    void testGetArtworkGameCube() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        ArtworkUrls artwork = provider.getArtwork("GMCE01");
        QVERIFY(!artwork.boxFront.isEmpty());
        // GameCube uses "wii" CDN path
        QVERIFY(artwork.boxFront.toString().contains("wii/cover/US/GMCE01.png"));
    }

    void testGetArtworkNotFound() {
        GameTDBProvider provider;
        ArtworkUrls artwork = provider.getArtwork("NONEXIST");
        QVERIFY(artwork.boxFront.isEmpty());
    }

    void testBoxArtInMetadata() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        GameMetadata md = provider.getByHash("3A70D78B", "");
        QVERIFY(!md.boxArtUrl.isEmpty());
        QVERIFY(md.boxArtUrl.contains("art.gametdb.com"));
        QVERIFY(md.boxArtUrl.contains("cover"));
    }

    void testNoRomEntryStillLoads() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        // NOROM has no rom element — should still be findable by ID
        GameMetadata md = provider.getById("NOROM");
        QCOMPARE(md.title, QString("WiiWare Title"));
        QCOMPARE(md.developer, QString("Indie Dev"));
        QCOMPARE(md.players, 2);
    }

    void testReleaseDateFormats() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        GameTDBProvider provider;
        provider.loadDatabase(writeSampleXml(tmpDir.path()));

        // Full date
        GameMetadata mk = provider.getById("RMCP01");
        QCOMPARE(mk.releaseDate, QString("2008-04-11"));

        // No date element — NOROM has no <date>
        GameMetadata norom = provider.getById("NOROM");
        QVERIFY(norom.releaseDate.isEmpty());
    }

    void testProviderAttributes() {
        GameTDBProvider provider;
        QCOMPARE(provider.name(), QString("GameTDB"));
        QCOMPARE(provider.requiresAuth(), false);
        QCOMPARE(provider.priority(), 150);
    }
};

QTEST_MAIN(GameTDBProviderTest)
#include "test_gametdb_provider.moc"
