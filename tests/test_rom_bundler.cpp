#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "../src/core/archive_creator.h"
#include "../src/core/archive_extractor.h"
#include "../src/core/chd_converter.h"
#include "../src/core/rom_bundler.h"
#include "../src/core/database.h"

using namespace Remus;

namespace {
bool writeAll(QFile &file, const QByteArray &data)
{
    return file.write(data) == data.size();
}
}

class RomBundlerTest : public QObject
{
    Q_OBJECT

private:
    // Build a minimal FileRecord suitable for bundling tests
    static FileRecord makeFileRecord(int id, const QString &path, const QString &filename = {})
    {
        FileRecord r;
        r.id           = id;
        r.currentPath  = path;
        r.filename     = filename.isEmpty() ? QFileInfo(path).fileName() : filename;
        r.isCompressed = false;
        r.crc32        = "AABBCCDD";
        r.md5          = "abc123";
        r.sha1         = "def456";
        return r;
    }

    // Build a minimal MatchResult
    static Database::MatchResult makeMatch(const QString &title = "Test Game")
    {
        Database::MatchResult m;
        m.gameTitle   = title;
        m.matchMethod = "CRC32";
        m.confidence  = 1.0f;
        m.isRejected  = false;
        return m;
    }

    // Build minimal GameMetadata
    static GameMetadata makeMetadata(const QString &title = "Test Game")
    {
        GameMetadata meta;
        meta.title  = title;
        meta.system = "Test System";
        return meta;
    }

    // Create a small dummy file
    static bool writeFile(const QString &path, const QByteArray &data = "DUMMY ROM DATA")
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        if (!writeAll(f, data)) return false;
        f.close();
        return true;
    }

    static bool writeMinimalCueBinSet(const QString &cuePath, const QString &binPath)
    {
        if (!writeFile(binPath, QByteArray(2352 * 16, '\0'))) {
            return false;
        }

        QFile cueFile(cuePath);
        if (!cueFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }

        const QByteArray cueContents = QStringLiteral("FILE \"%1\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n")
            .arg(QFileInfo(binPath).fileName())
            .toUtf8();
        if (!writeAll(cueFile, cueContents)) {
            return false;
        }
        cueFile.close();
        return true;
    }

    static bool copyFixtureDirectoryFiles(const QString &sourceDir, const QString &destinationDir)
    {
        const QDir dir(sourceDir);
        const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            if (!QFile::copy(entry.absoluteFilePath(), destinationDir + "/" + entry.fileName())) {
                return false;
            }
        }
        return true;
    }

private slots:

    // ── isAlreadyBundled ─────────────────────────────────────────────────────

    void testIsAlreadyBundled_nonexistentPath_returnsFalse()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.filePath("nofile.db")));
        RomBundler bundler(db);

        QVERIFY(!bundler.isAlreadyBundled("/nonexistent/archive.zip"));
    }

    void testIsAlreadyBundled_plainFile_returnsFalse()
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

    void testBundle_dryRun_returnsSuccessWithoutCreatingFile()
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
        const int id = db.insertFile(rec);
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

    void testBundle_dryRun_outputPathContainsBaseName()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString romPath = tmp.filePath("Sonic The Hedgehog (USA).nes");
        QVERIFY(writeFile(romPath));

        Database db;
        QVERIFY(db.initialize(tmp.filePath("test.db")));

        FileRecord rec = makeFileRecord(0, romPath);
        rec.id = db.insertFile(rec);
        QVERIFY(rec.id > 0);

        RomBundler bundler(db);

        RomBundler::BundleConfig cfg;
        cfg.dryRun        = true;
        cfg.includeBoxArt = false;

        const QString destDir = tmp.filePath("bundles");
        RomBundler::BundleResult result = bundler.bundle(rec, makeMatch(), makeMetadata(), destDir, cfg);

        QVERIFY(result.success);
        QVERIFY(result.outputPath.contains("Sonic The Hedgehog (USA)"));
    }

    void testBundle_realZipContainsMarkerAndArtworkSubdir()
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
        rec.id = db.insertFile(rec);
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

    void testBundle_realSevenZipContainsMarkerAndArtworkSubdir()
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
        rec.id = db.insertFile(rec);
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

    void testBundle_markerUsesStoredPercentConfidence()
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
        rec.id = db.insertFile(rec);
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

    void testBundle_skipsWhenCurrentCompressedPathAlreadyBundled()
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
        original.id = db.insertFile(original);
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
        bundled.currentPath = first.outputPath;
        bundled.archivePath = romPath;
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

    void testBundle_cueDiscMediaCanBePackagedAsChd()
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

    void testBundle_multiTrackGdiCanBePackagedAsChd()
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

    void testBundle_discConversionFailsWhenReferencedTrackIsMissing()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString cuePath = tmp.filePath("disc.cue");
        QFile cueFile(cuePath);
        QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray cueContents = QByteArrayLiteral("FILE \"missing.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n");
        QVERIFY(writeAll(cueFile, cueContents));
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

    // ── BundleConfig defaults ────────────────────────────────────────────────

    void testBundleConfig_defaults()
    {
        RomBundler::BundleConfig cfg;
        QVERIFY(cfg.includeBoxArt);
        QVERIFY(!cfg.dryRun);
        QCOMPARE(cfg.outputFormat, ArchiveFormat::ZIP);
        QVERIFY(cfg.artworkPath.isEmpty());
        QVERIFY(!cfg.convertDiscsToChd);
    }

    // ── BundleResult defaults ────────────────────────────────────────────────

    void testBundleResult_defaults()
    {
        RomBundler::BundleResult result;
        QVERIFY(!result.success);
        QVERIFY(!result.skippedAlreadyBundled);
        QVERIFY(result.outputPath.isEmpty());
        QVERIFY(result.error.isEmpty());
    }
};

QTEST_MAIN(RomBundlerTest)
#include "test_rom_bundler.moc"
