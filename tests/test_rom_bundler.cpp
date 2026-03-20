#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "../src/core/rom_bundler.h"
#include "../src/core/database.h"

using namespace Remus;

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
        f.write(data);
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

    // ── BundleConfig defaults ────────────────────────────────────────────────

    void testBundleConfig_defaults()
    {
        RomBundler::BundleConfig cfg;
        QVERIFY(cfg.includeBoxArt);
        QVERIFY(!cfg.dryRun);
        QCOMPARE(cfg.outputFormat, ArchiveFormat::ZIP);
        QVERIFY(cfg.artworkPath.isEmpty());
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
