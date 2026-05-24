#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

#include "../src/services/rapatches_catalog_builder.h"
#include "../src/services/retroachievements_enricher.h"
#include "../src/services/mod_catalog_provider.h"

using namespace Remus;

class RAPatchesCatalogTest : public QObject
{
    Q_OBJECT

private slots:
    // ── parseFilename ─────────────────────────────────────────────
    void parseFilename_fullFormat();
    void parseFilename_titleOnly();
    void parseFilename_regionAndAuthor();
    void parseFilename_versionVariants();
    void parseFilename_noExtension();

    // ── parseReadme ───────────────────────────────────────────────
    void parseReadme_md5AndCrc32();
    void parseReadme_md5Only();
    void parseReadme_empty();
    void parseReadme_romName();

    // ── normaliseSystemName ───────────────────────────────────────
    void normaliseSystem_knownMappings();
    void normaliseSystem_unmapped();

    // ── normaliseTypeName ─────────────────────────────────────────
    void normaliseType_knownMappings();
    void normaliseType_unmapped();

    // ── writeCatalogJson → loadFromFile round-trip ────────────────
    void catalogJsonRoundTrip();

    // ── buildFromDirectory with mock structure ────────────────────
    void buildFromDirectory_emptyDir();
    void buildFromDirectory_patchFiles();
    void buildFromDirectory_skippedDirs();
    void buildFromDirectory_zipNoPatchMember();
    void buildFromDirectory_zipWithPatchInside();

    // ── RetroAchievementsEnricher credential logic ────────────────
    void enricher_noApiKey_gracefulSkip();
    void enricher_setApiKey();
    void enricher_envFallback();
    void enricher_enrichCatalog_noKey();
};

// ═══════════════════════════════════════════════════════════════════
// parseFilename tests
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::parseFilename_fullFormat()
{
    const auto p = RAPatchesCatalogBuilder::parseFilename(
        "ToeJam & Earl (USA, Europe) (Fr) (v0.9) (sky2048).bps");

    QCOMPARE(p.title, QStringLiteral("ToeJam & Earl"));
    QCOMPARE(p.region, QStringLiteral("USA, Europe"));
    QCOMPARE(p.language, QStringLiteral("Fr"));
    QCOMPARE(p.version, QStringLiteral("v0.9"));
    QCOMPARE(p.author, QStringLiteral("sky2048"));
    QCOMPARE(p.format, QStringLiteral("bps"));
}

void RAPatchesCatalogTest::parseFilename_titleOnly()
{
    const auto p = RAPatchesCatalogBuilder::parseFilename("SomeRomHack.ips");
    QCOMPARE(p.title, QStringLiteral("SomeRomHack"));
    QVERIFY(p.region.isEmpty());
    QVERIFY(p.author.isEmpty());
    QCOMPARE(p.format, QStringLiteral("ips"));
}

void RAPatchesCatalogTest::parseFilename_regionAndAuthor()
{
    const auto p = RAPatchesCatalogBuilder::parseFilename(
        "Super Mario World (Japan) (hacker123).bps");
    QCOMPARE(p.title, QStringLiteral("Super Mario World"));
    QCOMPARE(p.region, QStringLiteral("Japan"));
    QCOMPARE(p.author, QStringLiteral("hacker123"));
}

void RAPatchesCatalogTest::parseFilename_versionVariants()
{
    // Test "Final" as a version
    {
        const auto p = RAPatchesCatalogBuilder::parseFilename(
            "Castlevania (USA) (Final).bps");
        QCOMPARE(p.version, QStringLiteral("Final"));
    }
    // Test "Beta"
    {
        const auto p = RAPatchesCatalogBuilder::parseFilename(
            "Zelda (World) (Beta).ips");
        QCOMPARE(p.version, QStringLiteral("Beta"));
    }
}

void RAPatchesCatalogTest::parseFilename_noExtension()
{
    const auto p = RAPatchesCatalogBuilder::parseFilename("NoExtension");
    QCOMPARE(p.title, QStringLiteral("NoExtension"));
    QVERIFY(p.format.isEmpty());
}

// ═══════════════════════════════════════════════════════════════════
// parseReadme tests
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::parseReadme_md5AndCrc32()
{
    const QString content =
        "Base ROM: Sonic the Hedgehog (USA).md\n"
        "MD5: d4a9c1e15c4c3f4a3d4e5f6a7b8c9d0e\n"
        "CRC32: AABB1122\n"
        "Apply with Floating IPS\n";

    const auto r = RAPatchesCatalogBuilder::parseReadme(content);
    QCOMPARE(r.baseMd5, QStringLiteral("d4a9c1e15c4c3f4a3d4e5f6a7b8c9d0e"));
    QCOMPARE(r.baseCrc32, QStringLiteral("aabb1122"));
}

void RAPatchesCatalogTest::parseReadme_md5Only()
{
    const QString content = "md5: AABBCCDD11223344AABBCCDD11223344\n";
    const auto r = RAPatchesCatalogBuilder::parseReadme(content);
    QCOMPARE(r.baseMd5, QStringLiteral("aabbccdd11223344aabbccdd11223344"));
    QVERIFY(r.baseCrc32.isEmpty());
}

void RAPatchesCatalogTest::parseReadme_empty()
{
    const auto r = RAPatchesCatalogBuilder::parseReadme(QString());
    QVERIFY(r.baseMd5.isEmpty());
    QVERIFY(r.baseCrc32.isEmpty());
    QVERIFY(r.baseRomName.isEmpty());
}

void RAPatchesCatalogTest::parseReadme_romName()
{
    const QString content =
        "Super Mario World (USA).sfc\n"
        "MD5: 00000000000000000000000000000000\n";

    const auto r = RAPatchesCatalogBuilder::parseReadme(content);
    QCOMPARE(r.baseRomName, QStringLiteral("Super Mario World (USA).sfc"));
}

// ═══════════════════════════════════════════════════════════════════
// normaliseSystemName tests
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::normaliseSystem_knownMappings()
{
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("GBA"),
             QStringLiteral("Game Boy Advance"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("SNES"),
             QStringLiteral("Super Nintendo"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("MD"),
             QStringLiteral("Sega Genesis / Mega Drive"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("N64"),
             QStringLiteral("Nintendo 64"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("NDS"),
             QStringLiteral("Nintendo DS"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("PS2"),
             QStringLiteral("PlayStation 2"));
}

void RAPatchesCatalogTest::normaliseSystem_unmapped()
{
    // Unknown names pass through unchanged
    QCOMPARE(RAPatchesCatalogBuilder::normaliseSystemName("UnknownSystem"),
             QStringLiteral("UnknownSystem"));
}

// ═══════════════════════════════════════════════════════════════════
// normaliseTypeName tests
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::normaliseType_knownMappings()
{
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("Fix"),
             QStringLiteral("fix"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("Hacks"),
             QStringLiteral("hack"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("Translation"),
             QStringLiteral("translation"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("Improvement"),
             QStringLiteral("improvement"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("MSU-1"),
             QStringLiteral("enhancement"));
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("GTConversion"),
             QStringLiteral("conversion"));
}

void RAPatchesCatalogTest::normaliseType_unmapped()
{
    // Unknown type names are lowercased
    QCOMPARE(RAPatchesCatalogBuilder::normaliseTypeName("SomeNewType"),
             QStringLiteral("somenewtype"));
}

// ═══════════════════════════════════════════════════════════════════
// Catalog JSON round-trip
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::catalogJsonRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString outputPath = dir.path() + "/test-catalog.json";

    // Build some test entries
    QList<ModEntry> mods;
    {
        ModEntry e;
        e.id       = QStringLiteral("ra-test001");
        e.title    = QStringLiteral("Sonic Russian Translation");
        e.author   = QStringLiteral("sky2048");
        e.version  = QStringLiteral("v1.0");
        e.type     = QStringLiteral("translation");
        e.system   = QStringLiteral("Sega Genesis / Mega Drive");
        e.format   = QStringLiteral("bps");
        e.baseMd5  = QStringLiteral("d4a9c1e15c4c3f4a3d4e5f6a7b8c9d0e");
        e.baseCrc32 = QStringLiteral("aabb1122");
        mods.append(e);
    }
    {
        ModEntry e;
        e.id       = QStringLiteral("ra-test002");
        e.title    = QStringLiteral("Super Mario World Kaizo");
        e.author   = QStringLiteral("romhacker");
        e.type     = QStringLiteral("hack");
        e.system   = QStringLiteral("Super Nintendo");
        e.format   = QStringLiteral("ips");
        mods.append(e);
    }

    // Write
    QString err = RAPatchesCatalogBuilder::writeCatalogJson(mods, outputPath);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(QFile::exists(outputPath));

    // Read back with ModCatalogProvider
    ModCatalogProvider provider;
    QVERIFY(provider.loadFromFile(outputPath));
    QCOMPARE(provider.allMods().size(), 2);

    // Verify data survives the round-trip
    auto loaded = provider.getModById("ra-test001");
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->title, QStringLiteral("Sonic Russian Translation"));
    QCOMPARE(loaded->author, QStringLiteral("sky2048"));
    QCOMPARE(loaded->baseMd5, QStringLiteral("d4a9c1e15c4c3f4a3d4e5f6a7b8c9d0e"));
    QCOMPARE(loaded->system, QStringLiteral("Sega Genesis / Mega Drive"));
}

// ═══════════════════════════════════════════════════════════════════
// buildFromDirectory tests
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::buildFromDirectory_emptyDir()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    RAPatchesCatalogBuilder builder;
    auto result = builder.buildFromDirectory(dir.path());

    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.mods.size(), 0);
    QCOMPARE(result.filesScanned, 0);
}

void RAPatchesCatalogTest::buildFromDirectory_patchFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create mock structure: SNES/Hacks/TestHack.bps
    QDir root(dir.path());
    root.mkpath("SNES/Hacks");

    // Create a dummy .bps file
    QFile patchFile(root.filePath("SNES/Hacks/Cool Hack (USA) (v1.2) (author42).bps"));
    QVERIFY(patchFile.open(QIODevice::WriteOnly));
    patchFile.write("FAKE-BPS");
    patchFile.close();

    // Also create a loose .ips in a different system
    root.mkpath("GBA/Fix");
    QFile fixFile(root.filePath("GBA/Fix/Bug Fix (Japan).ips"));
    QVERIFY(fixFile.open(QIODevice::WriteOnly));
    fixFile.write("FAKE-IPS");
    fixFile.close();

    RAPatchesCatalogBuilder builder;
    auto result = builder.buildFromDirectory(dir.path());

    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.filesScanned, 2);
    QCOMPARE(result.mods.size(), 2);

    // Verify the entries have correct system/type normalisation
    bool foundSnes = false, foundGba = false;
    for (const auto &mod : result.mods) {
        if (mod.system == QStringLiteral("Super Nintendo")) {
            foundSnes = true;
            QCOMPARE(mod.type, QStringLiteral("hack"));
            QCOMPARE(mod.format, QStringLiteral("bps"));
            QCOMPARE(mod.title, QStringLiteral("Cool Hack"));
        }
        if (mod.system == QStringLiteral("Game Boy Advance")) {
            foundGba = true;
            QCOMPARE(mod.type, QStringLiteral("fix"));
            QCOMPARE(mod.format, QStringLiteral("ips"));
        }
    }
    QVERIFY2(foundSnes, "Expected a Super Nintendo entry");
    QVERIFY2(foundGba, "Expected a Game Boy Advance entry");
}

void RAPatchesCatalogTest::buildFromDirectory_skippedDirs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Create directories that should be skipped
    QDir root(dir.path());
    root.mkpath(".git");
    root.mkpath("Removed");
    root.mkpath("Saves");

    // Put a patch in a skipped dir — it should not be scanned
    QFile f(root.filePath("Removed/something.bps"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("FAKE");
    f.close();

    // Put a valid entry in a real system dir
    root.mkpath("NES/Fix");
    QFile valid(root.filePath("NES/Fix/Game Fix.ips"));
    QVERIFY(valid.open(QIODevice::WriteOnly));
    valid.write("FAKE-IPS");
    valid.close();

    RAPatchesCatalogBuilder builder;
    auto result = builder.buildFromDirectory(dir.path());

    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.filesScanned, 1);
    QCOMPARE(result.mods.size(), 1);
    QCOMPARE(result.mods.first().system, QStringLiteral("NES"));
}

// ═══════════════════════════════════════════════════════════════════
// RetroAchievementsEnricher tests (credential logic, no network)
// ═══════════════════════════════════════════════════════════════════

void RAPatchesCatalogTest::enricher_noApiKey_gracefulSkip()
{
    // Clear env vars to ensure no key
    qunsetenv("REMUS_RA_API_KEY");
    qunsetenv("REMUS_RA_USERNAME");

    RetroAchievementsEnricher enricher;
    QVERIFY(!enricher.hasApiKey());

    QList<ModEntry> mods;
    ModEntry e;
    e.id = QStringLiteral("test-1");
    e.baseMd5 = QStringLiteral("00000000000000000000000000000000");
    mods.append(e);

    auto result = enricher.enrichCatalog(mods);
    QVERIFY(result.skippedNoApiKey);
    QCOMPARE(result.enrichedCount, 0);
    QCOMPARE(result.skippedCount, 1);
}

void RAPatchesCatalogTest::enricher_setApiKey()
{
    qunsetenv("REMUS_RA_API_KEY");
    qunsetenv("REMUS_RA_USERNAME");

    RetroAchievementsEnricher enricher;
    QVERIFY(!enricher.hasApiKey());

    enricher.setApiKey("testuser", "testapikey123");
    QVERIFY(enricher.hasApiKey());
    QCOMPARE(enricher.effectiveUsername(), QStringLiteral("testuser"));
    QCOMPARE(enricher.effectiveApiKey(), QStringLiteral("testapikey123"));
}

void RAPatchesCatalogTest::enricher_envFallback()
{
    qputenv("REMUS_RA_API_KEY", "envkey456");
    qputenv("REMUS_RA_USERNAME", "envuser");

    RetroAchievementsEnricher enricher;
    QVERIFY(enricher.hasApiKey());
    QCOMPARE(enricher.effectiveApiKey(), QStringLiteral("envkey456"));
    QCOMPARE(enricher.effectiveUsername(), QStringLiteral("envuser"));

    // Explicit key overrides env
    enricher.setApiKey("explicit", "explicitkey");
    QCOMPARE(enricher.effectiveApiKey(), QStringLiteral("explicitkey"));

    // Clean up
    qunsetenv("REMUS_RA_API_KEY");
    qunsetenv("REMUS_RA_USERNAME");
}

void RAPatchesCatalogTest::enricher_enrichCatalog_noKey()
{
    qunsetenv("REMUS_RA_API_KEY");
    qunsetenv("REMUS_RA_USERNAME");

    RetroAchievementsEnricher enricher;
    QList<ModEntry> empty;

    auto result = enricher.enrichCatalog(empty);
    QVERIFY(result.skippedNoApiKey);
    QCOMPARE(result.enrichedCount, 0);
}

// ═══════════════════════════════════════════════════════════════════
// buildFromDirectory zip-branch tests
// ═══════════════════════════════════════════════════════════════════

/// A .zip file whose contents can't be listed (invalid/empty zip) must produce
/// a fallback entry with an EMPTY patchUrl — not a fabricated wrong URL.
void RAPatchesCatalogTest::buildFromDirectory_zipNoPatchMember()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    root.mkpath("SNES/Hacks");

    // A file with .zip extension that is not a valid zip archive.
    // listZipContents() will return empty, exercising the no-patch-member branch.
    QFile fakeZip(root.filePath("SNES/Hacks/Fake Hack (USA) (v1.0) (author99).zip"));
    QVERIFY(fakeZip.open(QIODevice::WriteOnly));
    fakeZip.write("not a zip archive");
    fakeZip.close();

    RAPatchesCatalogBuilder builder;
    const auto result = builder.buildFromDirectory(dir.path());

    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.filesScanned, 1);
    QCOMPARE(result.mods.size(), 1);

    const ModEntry &entry = result.mods.first();
    QCOMPARE(entry.system, QStringLiteral("Super Nintendo"));
    QCOMPARE(entry.type, QStringLiteral("hack"));
    // Must be empty — not a wrong URL built from dirname.
    QVERIFY2(entry.patchUrl.isEmpty(),
             "patchUrl must be empty when no patch member is found in zip");
    QVERIFY(!entry.sourceUrl.isEmpty());
}

/// A .zip that contains a recognizable patch file must produce a fully
/// populated entry.  Skipped if the `zip` command is not available on PATH.
void RAPatchesCatalogTest::buildFromDirectory_zipWithPatchInside()
{
    // Require the `zip` tool at runtime to create the test archive.
    const QString zipTool = QStandardPaths::findExecutable(QStringLiteral("zip"));
    if (zipTool.isEmpty())
        QSKIP("zip tool not found on PATH; skipping zip-with-patch test");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    root.mkpath("GBA/Translation");

    // Create the patch file that will go inside the zip.
    const QString patchName =
        QStringLiteral("My Game (Japan) (En) (v1.2) (AuthorName).bps");
    QTemporaryDir patchSrc;
    QVERIFY(patchSrc.isValid());
    QFile pf(QDir(patchSrc.path()).filePath(patchName));
    QVERIFY(pf.open(QIODevice::WriteOnly));
    pf.write("BPS1");
    pf.close();

    // Build the zip inside the mock repo tree.
    const QString zipPath =
        root.filePath(QStringLiteral("GBA/Translation/My Game Translation.zip"));
    QProcess proc;
    proc.setWorkingDirectory(patchSrc.path());
    proc.start(zipTool, {zipPath, patchName});
    QVERIFY(proc.waitForFinished(5000));
    QVERIFY2(proc.exitCode() == 0, "Failed to create test zip");

    RAPatchesCatalogBuilder builder;
    const auto result = builder.buildFromDirectory(dir.path());

    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.filesScanned, 1);
    QCOMPARE(result.mods.size(), 1);

    const ModEntry &entry = result.mods.first();
    QCOMPARE(entry.system, QStringLiteral("Game Boy Advance"));
    QCOMPARE(entry.type, QStringLiteral("translation"));
    QCOMPARE(entry.format, QStringLiteral("bps"));
    QVERIFY(!entry.title.isEmpty());
}

QTEST_MAIN(RAPatchesCatalogTest)
#include "test_rapatches_catalog.moc"
