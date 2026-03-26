#include "test_rom_bundler_fixture.h"

// ── disc conversion ──────────────────────────────────────────────────────

void RomBundlerTest::testBundle_cueDiscMediaCanBePackagedAsChd()
{
    ArchiveCreator creator;
    ArchiveExtractor extractor;
    CHDConverter converter;
    if (!creator.canCompress(ArchiveFormat::ZIP)) {
        QSKIP("zip tool not available");
    }
    if (!extractor.canExtract(ArchiveFormat::ZIP)) {
        QSKIP("unzip tool not available");
    }
    if (!converter.isChdmanAvailable()) {
        QSKIP("chdman not available");
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString cuePath = tmp.filePath("disc.cue");
    const QString binPath = tmp.filePath("track01.bin");
    QVERIFY(writeMinimalCueBinSet(cuePath, binPath));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord cue = makeFileRecord(0, cuePath, "disc.cue");
    cue.id = db.insertFile(cue);
    QVERIFY(cue.id > 0);

    FileRecord bin = makeFileRecord(0, binPath, "track01.bin");
    bin.parentFileId = cue.id;
    bin.isPrimary = false;
    bin.id = db.insertFile(bin);
    QVERIFY(bin.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.convertDiscsToChd = true;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(cue, makeMatch("Disc Test"), makeMetadata("Disc Test"), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains(".remus.md"));
    QVERIFY(info.contents.contains("disc.chd"));
    QVERIFY(!info.contents.contains("disc.cue"));
    QVERIFY(!info.contents.contains("track01.bin"));
}

void RomBundlerTest::testBundle_multiTrackGdiCanBePackagedAsChd()
{
    ArchiveCreator creator;
    ArchiveExtractor extractor;
    CHDConverter converter;
    if (!creator.canCompress(ArchiveFormat::ZIP)) {
        QSKIP("zip tool not available");
    }
    if (!extractor.canExtract(ArchiveFormat::ZIP)) {
        QSKIP("unzip tool not available");
    }
    if (!converter.isChdmanAvailable()) {
        QSKIP("chdman not available");
    }

    const QString gdiFixturePath = QFINDTESTDATA("fixtures/gdi/multi_track/disc.gdi");
    QVERIFY2(!gdiFixturePath.isEmpty(), "multi-track GDI fixture not found");

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString fixtureDir = QFileInfo(gdiFixturePath).absolutePath();
    QVERIFY(copyFixtureDirectoryFiles(fixtureDir, tmp.path()));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord gdi = makeFileRecord(0, tmp.filePath("disc.gdi"), "disc.gdi");
    gdi.id = db.insertFile(gdi);
    QVERIFY(gdi.id > 0);

    const QStringList trackNames = {
        "track01.bin",
        "track02.raw",
        "track03.bin"
    };

    for (const QString &trackName : trackNames) {
        FileRecord track = makeFileRecord(0, tmp.filePath(trackName), trackName);
        track.parentFileId = gdi.id;
        track.isPrimary = false;
        track.id = db.insertFile(track);
        QVERIFY(track.id > 0);
    }

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.convertDiscsToChd = true;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(gdi, makeMatch("Dreamcast Disc Test"), makeMetadata("Dreamcast Disc Test"), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains(".remus.md"));
    QVERIFY(info.contents.contains("disc.chd"));
    QVERIFY(!info.contents.contains("disc.gdi"));
    QVERIFY(!info.contents.contains("track01.bin"));
    QVERIFY(!info.contents.contains("track02.raw"));
    QVERIFY(!info.contents.contains("track03.bin"));
}

void RomBundlerTest::testBundle_discConversionFailsWhenReferencedTrackIsMissing()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString cuePath = tmp.filePath("disc.cue");
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray cueContents = QByteArrayLiteral(
        "FILE \"missing.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n");
    QVERIFY(romBundlerWriteAll(cueFile, cueContents));
    cueFile.close();

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord cue = makeFileRecord(0, cuePath, "disc.cue");
    cue.id = db.insertFile(cue);
    QVERIFY(cue.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.convertDiscsToChd = true;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(cue, makeMatch("Broken Disc"), makeMetadata("Broken Disc"), destDir, cfg);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Referenced disc file not found"));
    QVERIFY(!QFile::exists(destDir + "/disc.zip"));
}

// ── struct defaults ──────────────────────────────────────────────────────

void RomBundlerTest::testBundleConfig_defaults()
{
    RomBundler::BundleConfig cfg;
    QVERIFY(cfg.includeBoxArt);
    QVERIFY(!cfg.dryRun);
    QCOMPARE(cfg.outputFormat, ArchiveFormat::ZIP);
    QVERIFY(cfg.artworkPath.isEmpty());
    QVERIFY(!cfg.convertDiscsToChd);
}

void RomBundlerTest::testBundleResult_defaults()
{
    RomBundler::BundleResult result;
    QVERIFY(!result.success);
    QVERIFY(!result.skippedAlreadyBundled);
    QVERIFY(result.outputPath.isEmpty());
    QVERIFY(result.error.isEmpty());
}
