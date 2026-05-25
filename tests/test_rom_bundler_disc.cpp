#include "test_rom_bundler_fixture.h"

// ── disc conversion ──────────────────────────────────────────────────────

void RomBundlerTest::testBundle_binPrimaryWithCueChildCanBePackagedAsChd()
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

    FileRecord bin = makeFileRecord(0, binPath, "track01.bin");
    bin.id = insertTestFile(db, bin);
    QVERIFY(bin.id > 0);
    QVERIFY(db.updateFileHashes(bin.id, "AABBCCDD", "bin-md5", "bin-sha1"));

    FileRecord cue = makeFileRecord(0, cuePath, "disc.cue");
    cue.parentFileId = bin.id;
    cue.isPrimary = false;
    cue.id = insertTestFile(db, cue);
    QVERIFY(cue.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Chd;

    const QString destDir = tmp.filePath("bundles");
    const RomBundler::BundleResult result = bundler.bundle(
        bin, makeMatch("Disc Test"), makeMetadata("Disc Test"), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains(".remus.md"));
    QVERIFY(info.contents.contains("disc.chd"));
    QVERIFY(!info.contents.contains("disc.cue"));
    QVERIFY(!info.contents.contains("track01.bin"));

    const FileRecord bundled = db.getFileById(bin.id);
    QCOMPARE(bundled.archiveInternalPath, QStringLiteral("disc.chd"));
    QCOMPARE(bundled.filename, QStringLiteral("disc.chd"));
    QCOMPARE(bundled.extension, QStringLiteral(".chd"));
    QVERIFY(!bundled.hashCalculated);
    QVERIFY(bundled.crc32.isEmpty());
    QVERIFY(bundled.md5.isEmpty());
    QVERIFY(bundled.sha1.isEmpty());
    QVERIFY(bundled.fileSize > 0);
}

void RomBundlerTest::testBundle_binPrimaryWithCueChildKeepsPrimaryPayloadWhenBundlingOriginal()
{
    ArchiveCreator creator;
    ArchiveExtractor extractor;
    if (!creator.canCompress(ArchiveFormat::ZIP)) {
        QSKIP("zip tool not available");
    }
    if (!extractor.canExtract(ArchiveFormat::ZIP)) {
        QSKIP("unzip tool not available");
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString cuePath = tmp.filePath("disc.cue");
    const QString binPath = tmp.filePath("track01.bin");
    QVERIFY(writeMinimalCueBinSet(cuePath, binPath));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord bin = makeFileRecord(0, binPath, "track01.bin");
    bin.id = insertTestFile(db, bin);
    QVERIFY(bin.id > 0);
    QVERIFY(db.updateFileHashes(bin.id, "AABBCCDD", "bin-md5", "bin-sha1"));

    FileRecord cue = makeFileRecord(0, cuePath, "disc.cue");
    cue.parentFileId = bin.id;
    cue.isPrimary = false;
    cue.id = insertTestFile(db, cue);
    QVERIFY(cue.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Original;

    const QString destDir = tmp.filePath("bundles");
    const RomBundler::BundleResult result = bundler.bundle(
        bin, makeMatch("Disc Test"), makeMetadata("Disc Test"), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains("track01.bin"));
    QVERIFY(info.contents.contains("disc.cue"));

    const FileRecord bundled = db.getFileById(bin.id);
    QCOMPARE(bundled.archiveInternalPath, QStringLiteral("track01.bin"));
    QCOMPARE(bundled.filename, QStringLiteral("track01.bin"));
    QCOMPARE(bundled.extension, QStringLiteral(".bin"));
    QCOMPARE(bundled.md5, QStringLiteral("bin-md5"));
    QCOMPARE(bundled.sha1, QStringLiteral("bin-sha1"));
    QVERIFY(bundled.hashCalculated);
}

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
    cue.id = insertTestFile(db, cue);
    QVERIFY(cue.id > 0);

    FileRecord bin = makeFileRecord(0, binPath, "track01.bin");
    bin.parentFileId = cue.id;
    bin.isPrimary = false;
    bin.id = insertTestFile(db, bin);
    QVERIFY(bin.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Chd;

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
    gdi.id = insertTestFile(db, gdi);
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
        track.id = insertTestFile(db, track);
        QVERIFY(track.id > 0);
    }

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Chd;

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
    cue.id = insertTestFile(db, cue);
    QVERIFY(cue.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Chd;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(cue, makeMatch("Broken Disc"), makeMetadata("Broken Disc"), destDir, cfg);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Referenced disc file not found"));
    QVERIFY(!QFile::exists(destDir + "/disc.zip"));
}

void RomBundlerTest::testBundle_gameCubeIsoPrefersRvzWhenDiscOptimizationRequested()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString isoPath = tmp.filePath("disc.iso");
    QVERIFY(writeFile(isoPath, QByteArray(4096, '\0')));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord disc = makeFileRecord(0, isoPath, "disc.iso");
    disc.systemId = Remus::Constants::Systems::ID_PSX;
    disc.id = insertTestFile(db, disc);
    QVERIFY(disc.id > 0);

    Database::MatchResult match = makeMatch("GameCube Disc");
    match.systemId = Remus::Constants::Systems::ID_GAMECUBE;

    GameMetadata metadata = makeMetadata("GameCube Disc");
    metadata.system = QStringLiteral("GameCube");

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.dryRun = true;
    cfg.outputFormat = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Chd;

    const QString destDir = tmp.filePath("bundles");
    const RomBundler::BundleResult result = bundler.bundle(disc, match, metadata, destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(result.archiveEntries.contains(".remus.md"));
    QVERIFY(result.archiveEntries.contains("disc.rvz"));
    QVERIFY(!result.archiveEntries.contains("disc.chd"));
}

// ── struct defaults ──────────────────────────────────────────────────────

void RomBundlerTest::testBundle_discManifestWithTraversalReference_failsSafely()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Place the manifest inside a subdirectory so "../" points within tmp —
    // a classic path-traversal attempt embedded in a disc manifest.
    QVERIFY(QDir(tmp.path()).mkpath("game"));
    const QString cuePath       = tmp.filePath("game/disc.cue");
    const QString outsideBinPath = tmp.filePath("sneaky.bin");

    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray cueContents = QByteArrayLiteral(
        "FILE \"../sneaky.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n");
    QVERIFY(romBundlerWriteAll(cueFile, cueContents));
    cueFile.close();

    // Create the target one level above game/ — proves it's reachable without the guard.
    QVERIFY(writeFile(outsideBinPath, QByteArray(2352 * 4, '\x00')));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord cue = makeFileRecord(0, cuePath, "disc.cue");
    cue.id = insertTestFile(db, cue);
    QVERIFY(cue.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat  = ArchiveFormat::ZIP;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Original;

    const RomBundler::BundleResult result = bundler.bundle(
        cue, makeMatch("Traversal Game"), makeMetadata("Traversal Game"),
        tmp.filePath("bundles"), cfg);

    QVERIFY(!result.success);
    QVERIFY2(result.error.contains(QStringLiteral("unsafe"), Qt::CaseInsensitive),
             qPrintable(result.error));
}

void RomBundlerTest::testBundleConfig_defaults()
{
    RomBundler::BundleConfig cfg;
    QVERIFY(cfg.includeBoxArt);
    QVERIFY(!cfg.dryRun);
    QCOMPARE(cfg.outputFormat, ArchiveFormat::ZIP);
    QVERIFY(cfg.artworkPath.isEmpty());
    QCOMPARE(cfg.discOutputFormat, RomBundler::DiscOutputFormat::Original);
}

void RomBundlerTest::testBundleResult_defaults()
{
    RomBundler::BundleResult result;
    QVERIFY(!result.success);
    QVERIFY(!result.skippedAlreadyBundled);
    QVERIFY(result.outputPath.isEmpty());
    QVERIFY(result.archiveEntries.isEmpty());
    QVERIFY(result.error.isEmpty());
}
