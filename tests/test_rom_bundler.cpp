#include "test_rom_bundler_fixture.h"

// ── isAlreadyBundled ─────────────────────────────────────────────────────

void RomBundlerTest::testIsAlreadyBundled_nonexistentPath_returnsFalse()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Database db;
    QVERIFY(db.initialize(tmp.filePath("nofile.db")));
    RomBundler bundler(db);

    QVERIFY(!bundler.isAlreadyBundled("/nonexistent/archive.zip"));
}

void RomBundlerTest::testIsAlreadyBundled_plainFile_returnsFalse()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString romPath = tmp.filePath("game.nes");
    QVERIFY(writeFile(romPath));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));
    RomBundler bundler(db);

    // A plain (non-archive) file is never bundled
    QVERIFY(!bundler.isAlreadyBundled(romPath));
}

// ── bundle() dry-run ─────────────────────────────────────────────────────

void RomBundlerTest::testBundle_dryRun_returnsSuccessWithoutCreatingFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create a source ROM file
    const QString romPath = tmp.filePath("game.nes");
    QVERIFY(writeFile(romPath));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    // Register the file in DB so markFileProcessed won't crash
    FileRecord rec = makeFileRecord(0, romPath, "game.nes");
    const int id = insertTestFile(db, rec);
    QVERIFY(id > 0);
    rec.id = id;

    RomBundler bundler(db);

    const QString destDir = tmp.filePath("output");

    RomBundler::BundleConfig cfg;
    cfg.dryRun        = true;
    cfg.includeBoxArt = false;
    cfg.outputFormat  = ArchiveFormat::ZIP;

    RomBundler::BundleResult result = bundler.bundle(rec, makeMatch(), makeMetadata(), destDir, cfg);

    QVERIFY(result.success);
    QVERIFY(!result.skippedAlreadyBundled);
    QVERIFY(result.error.isEmpty());
    // Expected output path set
    QVERIFY(result.outputPath.endsWith(".zip"));
    // In dry-run mode, output directory and archive must NOT be created
    QVERIFY(!QFile::exists(destDir));
    QVERIFY(!QFile::exists(result.outputPath));
}

void RomBundlerTest::testBundle_dryRun_outputPathContainsBaseName()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString romPath = tmp.filePath("Sonic The Hedgehog (USA).nes");
    QVERIFY(writeFile(romPath));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, romPath);
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.dryRun        = true;
    cfg.includeBoxArt = false;

    const QString destDir = tmp.filePath("bundles");

    // When metadata has a title, bundler uses it for the output archive name
    RomBundler::BundleResult result = bundler.bundle(rec, makeMatch(), makeMetadata(), destDir, cfg);
    QVERIFY(result.success);
    QVERIFY(result.outputPath.contains("Test Game"));

    // When metadata title is empty, bundler falls back to the ROM filename
    RomBundler::BundleResult fallback = bundler.bundle(rec, makeMatch(""), makeMetadata(""), destDir, cfg);
    QVERIFY(fallback.success);
    QVERIFY(fallback.outputPath.contains("Sonic The Hedgehog (USA)"));
}

// ── bundle() real archives ───────────────────────────────────────────────

void RomBundlerTest::testBundle_realZipContainsMarkerAndArtworkSubdir()
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

    const QString romPath = tmp.filePath("game.nes");
    const QString artPath = tmp.filePath("boxfront.jpg");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));
    QVERIFY(writeFile(artPath, "ARTPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, romPath, "game.nes");
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = true;
    cfg.outputFormat  = ArchiveFormat::ZIP;
    cfg.artworkPath   = artPath;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(rec, makeMatch(), makeMetadata(), destDir, cfg);

    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));
    QVERIFY(bundler.isAlreadyBundled(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains(".remus.md"));
    QVERIFY(info.contents.contains("artwork/boxfront.jpg"));
}

void RomBundlerTest::testBundle_realSevenZipContainsMarkerAndArtworkSubdir()
{
    ArchiveCreator creator;
    ArchiveExtractor extractor;
    if (!creator.canCompress(ArchiveFormat::SevenZip)) {
        QSKIP("7z tool not available");
    }
    if (!extractor.canExtract(ArchiveFormat::SevenZip)) {
        QSKIP("7z extraction tool not available");
    }

    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString romPath = tmp.filePath("game.nes");
    const QString artPath = tmp.filePath("boxfront.jpg");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));
    QVERIFY(writeFile(artPath, "ARTPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, romPath, "game.nes");
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = true;
    cfg.outputFormat  = ArchiveFormat::SevenZip;
    cfg.artworkPath   = artPath;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(rec, makeMatch(), makeMetadata(), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));
    QVERIFY(bundler.isAlreadyBundled(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains(".remus.md"));
    QVERIFY(info.contents.contains("artwork/boxfront.jpg"));
}

void RomBundlerTest::testBundle_updatesStoredFileStateToArchivePayload()
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

    const QString romPath = tmp.filePath("game.nes");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, romPath, "game.nes");
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(rec, makeMatch(), makeMetadata(), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));

    const FileRecord bundled = db.getFileById(rec.id);
    QVERIFY(bundled.isCompressed);
    QCOMPARE(bundled.currentPath, result.outputPath);
    QCOMPARE(bundled.archivePath, result.outputPath);
    QCOMPARE(bundled.archiveInternalPath, QStringLiteral("game.nes"));
    QCOMPARE(bundled.filename, QStringLiteral("game.nes"));
    QCOMPARE(bundled.extension, QStringLiteral(".nes"));
    QCOMPARE(bundled.fileSize, static_cast<qint64>(10));
}

void RomBundlerTest::testBundleStaged_updatesStoredFileStateAndReturnsOutputPath()
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

    const QString romPath = tmp.filePath("patched.nes");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, romPath, "patched.nes");
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundleStaged(rec, makeMatch(), makeMetadata(), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(!result.outputPath.isEmpty());
    QVERIFY(QFile::exists(result.outputPath));

    const FileRecord bundled = db.getFileById(rec.id);
    QVERIFY(bundled.isCompressed);
    QCOMPARE(bundled.currentPath, result.outputPath);
    QCOMPARE(bundled.archivePath, result.outputPath);
    QCOMPARE(bundled.archiveInternalPath, QStringLiteral("patched.nes"));
    QCOMPARE(bundled.filename, QStringLiteral("patched.nes"));
    QCOMPARE(bundled.extension, QStringLiteral(".nes"));
    QCOMPARE(bundled.fileSize, static_cast<qint64>(10));
}

void RomBundlerTest::testBundle_compressedNestedPayloadIsFlattenedToArchiveRoot()
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

    const QString sourceDir = tmp.filePath("source");
    QVERIFY(QDir().mkpath(sourceDir + "/Nested Game"));
    const QString nestedRom = sourceDir + "/Nested Game/game.nes";
    QVERIFY(writeFile(nestedRom, "ROMPAYLOAD"));

    const QString sourceArchive = tmp.filePath("nested.zip");
    const CompressionResult compressed = creator.compressDirectoryContents(
        sourceDir, sourceArchive, ArchiveFormat::ZIP);
    QVERIFY2(compressed.success, qPrintable(compressed.error));
    QVERIFY(QFile::exists(sourceArchive));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, sourceArchive, "game.nes");
    rec.isCompressed = true;
    rec.archivePath = sourceArchive;
    rec.archiveInternalPath = "Nested Game/game.nes";
    rec.extension = ".nes";
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat = ArchiveFormat::ZIP;

    const QString destDir = tmp.filePath("bundles");
    const RomBundler::BundleResult result = bundler.bundle(
        rec, makeMatch("Nested Game"), makeMetadata("Nested Game"), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));
    QVERIFY(QFile::exists(result.outputPath));

    const ArchiveInfo info = extractor.getArchiveInfo(result.outputPath);
    QVERIFY(info.contents.contains(".remus.md"));
    QVERIFY(info.contents.contains("game.nes"));
    QVERIFY(!info.contents.contains("Nested Game/game.nes"));

    bool hasNestedFolder = false;
    for (const QString &entry : info.contents) {
        if (entry.startsWith("Nested Game/")) {
            hasNestedFolder = true;
            break;
        }
    }
    QVERIFY(!hasNestedFolder);
}

void RomBundlerTest::testBundle_markerUsesStoredPercentConfidence()
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

    const QString romPath = tmp.filePath("game.nes");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord rec = makeFileRecord(0, romPath, "game.nes");
    rec.id = insertTestFile(db, rec);
    QVERIFY(rec.id > 0);

    RomBundler bundler(db);

    Database::MatchResult match = makeMatch();
    match.confidence = 100.0f;

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat  = ArchiveFormat::ZIP;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult result = bundler.bundle(rec, match, makeMetadata(), destDir, cfg);
    QVERIFY2(result.success, qPrintable(result.error));

    const QString extractDir = tmp.filePath("marker");
    ExtractionResult extraction = extractor.extractFile(result.outputPath, ".remus.md", extractDir);
    QVERIFY2(extraction.success, qPrintable(extraction.error));
    QVERIFY(!extraction.extractedFiles.isEmpty());

    QFile markerFile(extraction.extractedFiles.first());
    QVERIFY(markerFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString marker = QString::fromUtf8(markerFile.readAll());

    QVERIFY(marker.contains("confidence: 100.0000"));
    QVERIFY(marker.contains("| Confidence | 100.0% |"));
    QVERIFY(!marker.contains("10000.0%"));
}

void RomBundlerTest::testBundle_skipsWhenCurrentCompressedPathAlreadyBundled()
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

    const QString romPath = tmp.filePath("game.md");
    const QString artPath = tmp.filePath("boxfront.jpg");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));
    QVERIFY(writeFile(artPath, "ARTPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord original = makeFileRecord(0, romPath, "game.md");
    original.id = insertTestFile(db, original);
    QVERIFY(original.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = true;
    cfg.outputFormat  = ArchiveFormat::ZIP;
    cfg.artworkPath   = artPath;

    const QString destDir = tmp.filePath("bundles");
    RomBundler::BundleResult first = bundler.bundle(original, makeMatch(), makeMetadata(), destDir, cfg);
    QVERIFY2(first.success, qPrintable(first.error));
    QVERIFY(QFile::exists(first.outputPath));
    QVERIFY(bundler.isAlreadyBundled(first.outputPath));

    FileRecord bundled = original;
    bundled.isCompressed = true;
    bundled.originalPath = first.outputPath;
    bundled.currentPath = first.outputPath;
    bundled.archivePath = first.outputPath;
    bundled.archiveInternalPath = "game.md";

    const QByteArray before = QFileInfo(first.outputPath).exists() ? QByteArray::number(QFileInfo(first.outputPath).lastModified().toMSecsSinceEpoch()) : QByteArray();
    QVERIFY(!before.isEmpty());

    RomBundler::BundleResult second = bundler.bundle(bundled, makeMatch(), makeMetadata(), destDir, cfg);
    QVERIFY2(second.success, qPrintable(second.error));
    QVERIFY(second.skippedAlreadyBundled);
    QCOMPARE(second.outputPath, first.outputPath);

    const QByteArray after = QByteArray::number(QFileInfo(first.outputPath).lastModified().toMSecsSinceEpoch());
    QCOMPARE(after, before);
}

void RomBundlerTest::testBundle_rebundlesWhenCurrentPathPointsToPriorBundleOutput()
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

    const QString romPath = tmp.filePath("game.md");
    QVERIFY(writeFile(romPath, "ROMPAYLOAD"));

    Database db;
    QVERIFY(db.initialize(tmp.filePath("test.db")));

    FileRecord original = makeFileRecord(0, romPath, "game.md");
    original.id = insertTestFile(db, original);
    QVERIFY(original.id > 0);

    RomBundler bundler(db);

    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt = false;
    cfg.outputFormat  = ArchiveFormat::ZIP;

    const QString firstDestDir = tmp.filePath("bundles-a");
    const RomBundler::BundleResult first = bundler.bundle(
        original, makeMatch(), makeMetadata(), firstDestDir, cfg);
    QVERIFY2(first.success, qPrintable(first.error));
    QVERIFY(QFile::exists(first.outputPath));

    FileRecord rebundleCandidate = original;
    rebundleCandidate.isCompressed = true;
    rebundleCandidate.currentPath = first.outputPath;
    rebundleCandidate.archivePath = first.outputPath;
    rebundleCandidate.archiveInternalPath = "game.md";

    const QString secondDestDir = tmp.filePath("bundles-b");
    const RomBundler::BundleResult second = bundler.bundle(
        rebundleCandidate, makeMatch(), makeMetadata(), secondDestDir, cfg);
    QVERIFY2(second.success, qPrintable(second.error));
    QVERIFY(!second.skippedAlreadyBundled);
    QVERIFY(QFile::exists(second.outputPath));
    QVERIFY(QFileInfo(second.outputPath).absolutePath() == QFileInfo(secondDestDir).absoluteFilePath());
    QVERIFY(second.outputPath != first.outputPath);
}

QTEST_MAIN(RomBundlerTest)
