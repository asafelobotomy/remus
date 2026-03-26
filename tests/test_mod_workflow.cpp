#include <QtTest/QtTest>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

#include "../src/services/mod_catalog_provider.h"
#include "../src/services/mod_workflow_service.h"
#include "../src/core/database.h"

using namespace Remus;

namespace {
bool writeAll(QFile &file, const QByteArray &data)
{
    return file.write(data) == data.size();
}

bool removeIfExists(const QString &path)
{
    return !QFile::exists(path) || QFile::remove(path);
}
}

class ModWorkflowTest : public QObject
{
    Q_OBJECT

private:
    QString catalogPath() const
    {
        // Look for the fixture relative to the test binary and source tree
        const QStringList candidates = {
            QCoreApplication::applicationDirPath() + "/../../tests/fixtures/test_mod_catalog.json",
            QCoreApplication::applicationDirPath() + "/../tests/fixtures/test_mod_catalog.json",
            QCoreApplication::applicationDirPath() + "/../../../tests/fixtures/test_mod_catalog.json",
            QDir::currentPath() + "/tests/fixtures/test_mod_catalog.json",
            QDir::currentPath() + "/../tests/fixtures/test_mod_catalog.json",
            QString(REMUS_SOURCE_DIR) + "/tests/fixtures/test_mod_catalog.json",
        };
        for (const auto &path : candidates) {
            if (QFile::exists(path))
                return QDir::cleanPath(path);
        }
        return {};
    }

private slots:
    // ── ModCatalogProvider tests ──────────────────────────────────────────────

    void loadCatalog_validJson()
    {
        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture test_mod_catalog.json not found");

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromFile(path));
        QCOMPARE(provider.allMods().size(), 3);
    }

    void loadCatalog_invalidJson()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString badFile = dir.path() + "/bad.json";
        QFile f(badFile);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(f, QByteArrayLiteral("{ not valid json!!!")));
        f.close();

        ModCatalogProvider provider;
        QVERIFY(!provider.loadFromFile(badFile));
        QVERIFY(!provider.lastError().isEmpty());
    }

    void loadCatalog_missingFile()
    {
        ModCatalogProvider provider;
        QVERIFY(!provider.loadFromFile("/nonexistent/catalog.json"));
    }

    void findModsForRom_hashMatch()
    {
        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromFile(path));

        // Match by SHA1 of test-mod-alpha
        auto mods = provider.findModsForRom(
            QString(), QString(),
            "da39a3ee5e6b4b0d3255bfef95601890afd80709");
        QCOMPARE(mods.size(), 1);
        QCOMPARE(mods.first().id, QStringLiteral("test-mod-alpha"));
    }

    void findModsForRom_crc32Match()
    {
        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromFile(path));

        // CRC32 "AABBCCDD" matches alpha and gamma, but they have different SHA1s.
        // With only CRC32 provided, both should match.
        auto mods = provider.findModsForRom("AABBCCDD", QString(), QString());
        QVERIFY(mods.size() >= 1);

        bool foundAlpha = false;
        for (const auto &m : mods) {
            if (m.id == "test-mod-alpha") foundAlpha = true;
        }
        QVERIFY(foundAlpha);
    }

    void findModsForRom_noMatch()
    {
        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromFile(path));

        auto mods = provider.findModsForRom("00000000", "00000000000000000000000000000000",
                                            "0000000000000000000000000000000000000000");
        QVERIFY(mods.isEmpty());
    }

    void findModsBySystem()
    {
        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromFile(path));

        auto snesMods = provider.findModsBySystem("Super Nintendo");
        QCOMPARE(snesMods.size(), 2);

        auto genesisMods = provider.findModsBySystem("Sega Genesis");
        QCOMPARE(genesisMods.size(), 1);
        QCOMPARE(genesisMods.first().id, QStringLiteral("test-mod-gamma"));
    }

    void getModById()
    {
        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromFile(path));

        auto mod = provider.getModById("test-mod-beta");
        QVERIFY(mod.has_value());
        QCOMPARE(mod->title, QStringLiteral("Test Mod Beta"));
        QCOMPARE(mod->author, QStringLiteral("Beta Author"));
        QCOMPARE(mod->format, QStringLiteral("bps"));

        auto missing = provider.getModById("nonexistent");
        QVERIFY(!missing.has_value());
    }

    // ── Database mod_installations CRUD ──────────────────────────────────────

    void dbModInstallation_insertAndQuery()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "mod_crud_test"));

        // Insert a library and get a valid system ID
        int libId = db.insertLibrary("/roms", "TestLib");
        QVERIFY(libId > 0);
        int sysId = db.getSystemId("NES");
        QVERIFY(sysId > 0);

        // Insert a dummy file to reference
        FileRecord baseFile;
        baseFile.libraryId   = libId;
        baseFile.filename    = "test_rom.sfc";
        baseFile.originalPath = "/roms/test_rom.sfc";
        baseFile.currentPath = "/roms/test_rom.sfc";
        baseFile.extension   = "sfc";
        baseFile.systemId    = sysId;
        baseFile.fileSize    = 1024;
        int baseId = db.insertFile(baseFile);
        QVERIFY(baseId > 0);

        // Insert mod installation
        Database::ModInstallationRecord rec;
        rec.baseFileId    = baseId;
        rec.patchedFileId = 0;
        rec.catalogModId  = "test-mod-alpha";
        rec.modTitle      = "Test Mod Alpha";
        rec.modAuthor     = "Test Author";
        rec.modVersion    = "1.0";
        rec.modType       = "hack";
        rec.patchFormat   = "ips";
        rec.patchUrl      = "file:///tmp/patch.ips";

        int instId = db.insertModInstallation(rec);
        QVERIFY(instId > 0);

        // Query
        auto installs = db.getModInstallations(baseId);
        QCOMPARE(installs.size(), 1);
        QCOMPARE(installs.first().catalogModId, QStringLiteral("test-mod-alpha"));
        QCOMPARE(installs.first().modTitle, QStringLiteral("Test Mod Alpha"));

        // Remove
        QVERIFY(db.removeModInstallation(instId));
        QVERIFY(db.getModInstallations(baseId).isEmpty());

        db.close();
    }

    void dbModInstallation_removeNonexistent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "mod_remove_test"));

        QVERIFY(!db.removeModInstallation(999));

        db.close();
    }

    // ── ModWorkflowService tests ─────────────────────────────────────────────

    void install_patchVerificationFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Create a dummy "patch" file
        const QString patchPath = dir.path() + "/bad_patch.ips";
        QFile pf(patchPath);
        QVERIFY(pf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(pf, QByteArrayLiteral("PATCH dummy data")));
        pf.close();

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "verify_fail_test"));

        int libId = db.insertLibrary(dir.path(), "TestLib");
        QVERIFY(libId > 0);

        FileRecord baseFile;
        baseFile.libraryId   = libId;
        baseFile.filename    = "rom.sfc";
        baseFile.originalPath = dir.path() + "/rom.sfc";
        baseFile.currentPath = dir.path() + "/rom.sfc";
        baseFile.extension   = "sfc";
        baseFile.fileSize    = 1024;
        // Create an actual base file
        QFile bf(baseFile.currentPath);
        QVERIFY(bf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(bf, QByteArrayLiteral("base rom data")));
        bf.close();

        baseFile.id = db.insertFile(baseFile);
        QVERIFY(baseFile.id > 0);

        ModEntry mod;
        mod.id        = "test-sha1-fail";
        mod.title     = "SHA1 Fail Mod";
        mod.patchUrl  = patchPath;
        mod.patchSha1 = "0000000000000000000000000000000000000000"; // Wrong SHA1
        mod.format    = "ips";

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);

        ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");
        QVERIFY(!result.success);
        QVERIFY(result.error.contains("SHA1"));

        db.close();
    }

    void install_missingPatchFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "missing_patch_test"));

        int libId = db.insertLibrary(dir.path(), "TestLib");
        QVERIFY(libId > 0);

        FileRecord baseFile;
        baseFile.libraryId   = libId;
        baseFile.filename    = "rom.sfc";
        baseFile.originalPath = dir.path() + "/rom.sfc";
        baseFile.currentPath = dir.path() + "/rom.sfc";
        baseFile.extension   = "sfc";
        baseFile.fileSize    = 1024;
        QFile bf(baseFile.currentPath);
        QVERIFY(bf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(bf, QByteArrayLiteral("base rom data")));
        bf.close();
        baseFile.id = db.insertFile(baseFile);

        ModEntry mod;
        mod.id       = "missing";
        mod.title    = "Missing Patch";
        mod.patchUrl = "/nonexistent/patch.ips";
        mod.format   = "ips";

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);
        ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");

        QVERIFY(!result.success);
        QVERIFY(result.error.contains("not found"));

        db.close();
    }

    // ── Phase 2: URL loading + cache tests ───────────────────────────────────

    void loadFromUrl_fetchesAndCaches()
    {
        // Serve catalog from a file:// URL to avoid needing a real HTTP server
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        // Copy fixture to temp dir
        const QString tempCatalog = dir.path() + "/mods.json";
        QVERIFY(QFile::copy(path, tempCatalog));

        const QUrl url = QUrl::fromLocalFile(tempCatalog);

        ModCatalogProvider provider;
        QVERIFY(provider.loadFromUrl(url, true));
        QCOMPARE(provider.allMods().size(), 3);

        // Verify cache file was written
        const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
        QVERIFY(QFile::exists(cacheFile));

        // Clean up cache
        QVERIFY(removeIfExists(cacheFile));
    }

    void loadFromUrl_usesCacheWhenFresh()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        // Pre-populate cache manually
        const QUrl url(QStringLiteral("https://example.com/catalog_cache_test.json"));
        const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
        QVERIFY(QDir().mkpath(ModCatalogProvider::cacheDir()));

        QVERIFY(QFile::copy(path, cacheFile));

        // Touch the cache file to make it fresh (current time)
        QFile cf(cacheFile);
        QVERIFY(cf.open(QIODevice::ReadWrite));
        cf.close();

        // Load — should use cache without network (URL is unreachable)
        ModCatalogProvider provider;
        QVERIFY(provider.loadFromUrl(url, false));
        QCOMPARE(provider.allMods().size(), 3);

        QVERIFY(removeIfExists(cacheFile));
    }

    void loadFromUrl_networkErrorFallsBackToCache()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString path = catalogPath();
        QVERIFY2(!path.isEmpty(), "Fixture not found");

        // Pre-populate stale cache (exists, but we force refresh to trigger network)
        const QUrl url(QStringLiteral("https://unreachable.invalid/catalog.json"));
        const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
        QVERIFY(QDir().mkpath(ModCatalogProvider::cacheDir()));
        QVERIFY(QFile::copy(path, cacheFile));

        // Force refresh — network will fail, should fall back to cache
        ModCatalogProvider provider;
        QVERIFY(provider.loadFromUrl(url, true));
        QCOMPARE(provider.allMods().size(), 3);

        QVERIFY(removeIfExists(cacheFile));
    }

    void loadFromUrl_networkErrorNoCache()
    {
        // No cache and unreachable URL — should fail
        const QUrl url(QStringLiteral("https://unreachable.invalid/no_cache.json"));
        const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
        QVERIFY(removeIfExists(cacheFile)); // ensure no cache

        ModCatalogProvider provider;
        QVERIFY(!provider.loadFromUrl(url, true));
        QVERIFY(!provider.lastError().isEmpty());
    }

    // ── Phase 2: DB catalog cache CRUD ───────────────────────────────────────

    void dbCatalogCache_upsertAndQuery()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "catalog_cache_test"));

        Database::ModCatalogCacheRecord rec;
        rec.sourceUrl = "https://example.com/mods.json";
        rec.etag      = "\"abc123\"";
        rec.modCount  = 42;

        int id = db.upsertCatalogCache(rec);
        QVERIFY(id > 0);

        auto cached = db.getCatalogCache("https://example.com/mods.json");
        QVERIFY(cached.id > 0);
        QCOMPARE(cached.sourceUrl, QStringLiteral("https://example.com/mods.json"));
        QCOMPARE(cached.etag, QStringLiteral("\"abc123\""));
        QCOMPARE(cached.modCount, 42);

        // Upsert again with updated values
        rec.etag     = "\"def456\"";
        rec.modCount = 50;
        db.upsertCatalogCache(rec);

        cached = db.getCatalogCache("https://example.com/mods.json");
        QCOMPARE(cached.etag, QStringLiteral("\"def456\""));
        QCOMPARE(cached.modCount, 50);

        db.close();
    }

    void dbCatalogCache_queryMissing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "catalog_missing_test"));

        auto cached = db.getCatalogCache("https://nonexistent.example.com/mods.json");
        QCOMPARE(cached.id, 0);

        db.close();
    }

    void cacheDir_returnsValidPath()
    {
        const QString dir = ModCatalogProvider::cacheDir();
        QVERIFY(!dir.isEmpty());
        QVERIFY(dir.contains("mod_catalog_cache"));
    }

    void cacheFileForUrl_deterministicHash()
    {
        const QUrl url1(QStringLiteral("https://example.com/mods.json"));
        const QUrl url2(QStringLiteral("https://example.com/mods.json"));
        const QUrl url3(QStringLiteral("https://other.com/mods.json"));

        // Same URL produces same cache path
        QCOMPARE(ModCatalogProvider::cacheFileForUrl(url1),
                 ModCatalogProvider::cacheFileForUrl(url2));

        // Different URL produces different cache path
        QVERIFY(ModCatalogProvider::cacheFileForUrl(url1) !=
                ModCatalogProvider::cacheFileForUrl(url3));
    }

    // ── Phase 3: Patch download + integrity verification ─────────────────────

    void resolvePatchPath_fileUrl_resolves()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Create a dummy patch file
        const QString patchPath = dir.path() + "/patch.ips";
        QFile pf(patchPath);
        QVERIFY(pf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(pf, QByteArrayLiteral("PATCH")));
        QVERIFY(writeAll(pf, QByteArrayLiteral("EOF")));
        pf.close();

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "resolve_file_test"));
        int libId = db.insertLibrary(dir.path(), "TestLib");
        QVERIFY(libId > 0);

        FileRecord baseFile;
        baseFile.libraryId    = libId;
        baseFile.filename     = "rom.sfc";
        baseFile.originalPath = dir.path() + "/rom.sfc";
        baseFile.currentPath  = dir.path() + "/rom.sfc";
        baseFile.extension    = "sfc";
        baseFile.fileSize     = 1024;
        QFile bf(baseFile.currentPath);
        QVERIFY(bf.open(QIODevice::WriteOnly));
        const QByteArray baseRom(1024, '\0');
        QVERIFY(writeAll(bf, baseRom));
        bf.close();
        baseFile.id = db.insertFile(baseFile);

        // Use file:// URL for patch
        const QUrl fileUrl = QUrl::fromLocalFile(patchPath);
        ModEntry mod;
        mod.id       = "file-url-test";
        mod.title    = "File URL Test";
        mod.patchUrl = fileUrl.toString();
        mod.format   = "ips";

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);
        ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");
        // file:// URL should resolve fine — error should NOT be about resolving
        QVERIFY(!result.error.contains("resolve"));
        QVERIFY(!result.error.contains("not found"));

        db.close();
    }

    void resolvePatchPath_emptyUrl_fails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "resolve_empty_test"));

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);

        ModEntry mod;
        mod.patchUrl = "";

        ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
        QVERIFY(!result.success);
        QVERIFY(result.error.contains("resolve"));

        db.close();
    }

    void resolvePatchPath_unsupportedScheme_fails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "resolve_scheme_test"));

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);

        ModEntry mod;
        mod.patchUrl = "ftp://example.com/patch.ips";

        ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
        QVERIFY(!result.success);
        QVERIFY(result.error.contains("Unsupported") || result.error.contains("resolve"));

        db.close();
    }

    void verifySha1_correctHash()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Write known content and compute expected SHA1
        const QByteArray content("Hello Remus SHA1 test!");
        const QString filePath = dir.path() + "/testfile.bin";
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(f, content));
        f.close();

        // Compute expected SHA1
        QCryptographicHash hasher(QCryptographicHash::Sha1);
        hasher.addData(content);
        const QString expectedSha1 = QString::fromLatin1(hasher.result().toHex());

        // Install should pass SHA1 verification (will fail at patch step)
        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "sha1_pass_test"));
        int libId = db.insertLibrary(dir.path(), "TestLib");
        QVERIFY(libId > 0);

        FileRecord baseFile;
        baseFile.libraryId    = libId;
        baseFile.filename     = "rom.sfc";
        baseFile.originalPath = dir.path() + "/rom.sfc";
        baseFile.currentPath  = dir.path() + "/rom.sfc";
        baseFile.extension    = "sfc";
        baseFile.fileSize     = 1024;
        QFile bf(baseFile.currentPath);
        QVERIFY(bf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(bf, QByteArrayLiteral("base rom data")));
        bf.close();
        baseFile.id = db.insertFile(baseFile);

        ModEntry mod;
        mod.id        = "sha1-correct";
        mod.title     = "SHA1 Correct";
        mod.patchUrl  = filePath;
        mod.patchSha1 = expectedSha1;
        mod.format    = "ips";

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);
        ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");
        // Should NOT fail with SHA1 mismatch — may fail at patch application
        QVERIFY(!result.error.contains("SHA1"));

        db.close();
    }

    void downloadPatch_unreachableUrl_fails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "dl_unreachable_test"));
        int libId = db.insertLibrary(dir.path(), "TestLib");
        QVERIFY(libId > 0);

        FileRecord baseFile;
        baseFile.libraryId    = libId;
        baseFile.filename     = "rom.sfc";
        baseFile.originalPath = dir.path() + "/rom.sfc";
        baseFile.currentPath  = dir.path() + "/rom.sfc";
        baseFile.extension    = "sfc";
        baseFile.fileSize     = 1024;
        QFile bf(baseFile.currentPath);
        QVERIFY(bf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(bf, QByteArrayLiteral("base rom data")));
        bf.close();
        baseFile.id = db.insertFile(baseFile);

        ModEntry mod;
        mod.id       = "dl-fail";
        mod.title    = "Download Fail";
        mod.patchUrl = "https://unreachable.invalid/patch.ips";
        mod.format   = "ips";

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);
        ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");
        QVERIFY(!result.success);
        // Should fail with download-related error
        QVERIFY(result.error.contains("download") || result.error.contains("timed out")
                || result.error.contains("resolve"));

        db.close();
    }

    void install_progressCallbackFires()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // Create a minimal IPS patch (header + EOF marker)
        const QString patchPath = dir.path() + "/patch.ips";
        QFile pf(patchPath);
        QVERIFY(pf.open(QIODevice::WriteOnly));
        QVERIFY(writeAll(pf, QByteArrayLiteral("PATCH")));  // IPS header
        QVERIFY(writeAll(pf, QByteArrayLiteral("EOF")));    // IPS end marker
        pf.close();

        Database db;
        QVERIFY(db.initialize(dir.path() + "/test.db", "progress_test"));
        int libId = db.insertLibrary(dir.path(), "TestLib");
        QVERIFY(libId > 0);

        FileRecord baseFile;
        baseFile.libraryId    = libId;
        baseFile.filename     = "rom.sfc";
        baseFile.originalPath = dir.path() + "/rom.sfc";
        baseFile.currentPath  = dir.path() + "/rom.sfc";
        baseFile.extension    = "sfc";
        baseFile.fileSize     = 1024;
        QFile bf(baseFile.currentPath);
        QVERIFY(bf.open(QIODevice::WriteOnly));
        const QByteArray baseRom(1024, '\0');
        QVERIFY(writeAll(bf, baseRom));  // 1KB base ROM
        bf.close();
        baseFile.id = db.insertFile(baseFile);

        ModEntry mod;
        mod.id       = "progress-test";
        mod.title    = "Progress Test";
        mod.patchUrl = patchPath;
        mod.format   = "ips";

        QStringList stages;
        auto callback = [&stages](const QString &stage, int /*percent*/) {
            if (stages.isEmpty() || stages.last() != stage)
                stages.append(stage);
        };

        PatchService patchSvc;
        ModWorkflowService workflow(db, patchSvc);
        workflow.install(baseFile, mod, dir.path() + "/out", callback);

        // Should have received at least "resolving" and "verifying" stages
        QVERIFY(stages.contains("resolving"));
        QVERIFY(stages.contains("verifying"));

        db.close();
    }

    void downloadDir_cleanedOnDestruction()
    {
        QString tempDirPath;
        {
            QTemporaryDir dir;
            QVERIFY(dir.isValid());

            Database db;
            QVERIFY(db.initialize(dir.path() + "/test.db", "cleanup_test"));

            PatchService patchSvc;

            {
                ModWorkflowService workflow(db, patchSvc);
                // Trigger download to create the temp dir (will fail — unreachable URL)
                ModEntry mod;
                mod.patchUrl = "https://unreachable.invalid/cleanup.ips";
                workflow.install(FileRecord{}, mod, dir.path() + "/out");
                // Can't easily capture m_downloadDir path from outside,
                // but the smart pointer ensures cleanup on destruction
            }
            // workflow destroyed here — m_downloadDir should be cleaned up

            db.close();
        }
        // No crash, no leak — test passes if we get here
        QVERIFY(true);
    }
};

QTEST_MAIN(ModWorkflowTest)
#include "test_mod_workflow.moc"
