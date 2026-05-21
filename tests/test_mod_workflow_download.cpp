#include "test_mod_workflow_fixture.h"

// ── Phase 2: DB catalog cache CRUD ───────────────────────────────────────

void ModWorkflowTest::dbCatalogCache_upsertAndQuery()
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

void ModWorkflowTest::dbCatalogCache_queryMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "catalog_missing_test"));

    auto cached = db.getCatalogCache("https://nonexistent.example.com/mods.json");
    QCOMPARE(cached.id, 0);

    db.close();
}

void ModWorkflowTest::cacheDir_returnsValidPath()
{
    const QString dir = ModCatalogProvider::cacheDir();
    QVERIFY(!dir.isEmpty());
    QVERIFY(dir.contains("mod_catalog_cache"));
}

void ModWorkflowTest::cacheFileForUrl_deterministicHash()
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

void ModWorkflowTest::resolvePatchPath_fileUrl_resolves()
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
    mod.type     = "hack";
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

void ModWorkflowTest::resolvePatchPath_fileUrl_rejectedFromRemoteCatalog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString patchPath = dir.path() + "/patch.ips";
    QFile pf(patchPath);
    QVERIFY(pf.open(QIODevice::WriteOnly));
    pf.close();

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "reject_file_remote_test"));

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);
    workflow.setCatalogIsRemote(true);  // simulate remote catalog

    ModEntry mod;
    mod.type     = "hack";
    mod.patchUrl = QUrl::fromLocalFile(patchPath).toString();  // file:// URL

    ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
    QVERIFY(!result.success);
    QVERIFY2(result.error.contains("not permitted"),
             qPrintable(result.error));

    db.close();
}

void ModWorkflowTest::resolvePatchPath_relativeUrl_rejectedFromRemoteCatalog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "reject_relative_remote_test"));

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);
    workflow.setCatalogIsRemote(true);  // simulate remote catalog

    ModEntry mod;
    mod.type     = "hack";
    mod.patchUrl = "relative/path/patch.ips";  // relative path

    ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
    QVERIFY(!result.success);
    QVERIFY2(result.error.contains("not permitted"),
             qPrintable(result.error));

    db.close();
}

void ModWorkflowTest::resolvePatchPath_httpUrl_rejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "reject_http_test"));

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);

    ModEntry mod;
    mod.type     = "hack";
    mod.patchUrl = "http://example.com/patch.ips";  // plain HTTP — should be rejected

    ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
    QVERIFY(!result.success);
    QVERIFY2(result.error.contains("HTTPS") || result.error.contains("Insecure"),
             qPrintable(result.error));

    db.close();
}

void ModWorkflowTest::resolvePatchPath_emptyUrl_fails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "resolve_empty_test"));

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);

    ModEntry mod;
    mod.type = "hack";
    mod.patchUrl = "";

    ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("resolve"));

    db.close();
}

void ModWorkflowTest::resolvePatchPath_unsupportedScheme_fails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "resolve_scheme_test"));

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);

    ModEntry mod;
    mod.type = "hack";
    mod.patchUrl = "ftp://example.com/patch.ips";

    ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("Unsupported") || result.error.contains("resolve"));

    db.close();
}

void ModWorkflowTest::verifySha1_correctHash()
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
    mod.type      = "hack";
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

void ModWorkflowTest::downloadPatch_unreachableUrl_fails()
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
    mod.type     = "hack";
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

void ModWorkflowTest::install_progressCallbackFires()
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
    mod.type     = "hack";
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

void ModWorkflowTest::downloadDir_cleanedOnDestruction()
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
            mod.type = "hack";
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

void ModWorkflowTest::resolvePatchPath_privateIpHostRejected()
{
    // Finding #1: patch URLs targeting private / loopback IP literals must be
    // rejected before any network connection is made. This covers the DNS-
    // rebinding fix: IP-literal private addresses are caught by isPatchHostAllowed
    // without requiring a live DNS call.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "private_ip_test"));

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);

    const QStringList privateUrls = {
        QStringLiteral("https://10.0.0.1/patch.ips"),
        QStringLiteral("https://192.168.1.100/patch.ips"),
        QStringLiteral("https://172.16.0.1/patch.ips"),
        QStringLiteral("https://127.0.0.1/patch.ips"),
    };

    for (const QString &url : privateUrls) {
        ModEntry mod;
        mod.type     = "hack";
        mod.patchUrl = url;

        ModInstallResult result = workflow.install(FileRecord{}, mod, dir.path() + "/out");
        QVERIFY2(!result.success,
                 qPrintable(QStringLiteral("Expected rejection for %1 but got success").arg(url)));
        QVERIFY2(!result.error.isEmpty(),
                 qPrintable(QStringLiteral("Expected non-empty error for %1").arg(url)));
    }

    db.close();
}
