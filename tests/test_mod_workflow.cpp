#include "test_mod_workflow_fixture.h"

// ── ModCatalogProvider tests ─────────────────────────────────────────────

void ModWorkflowTest::loadCatalog_validJson()
{
    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture test_mod_catalog.json not found");

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromFile(path));
    QCOMPARE(provider.allMods().size(), 3);
}

void ModWorkflowTest::loadCatalog_invalidJson()
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

void ModWorkflowTest::loadCatalog_missingFile()
{
    ModCatalogProvider provider;
    QVERIFY(!provider.loadFromFile("/nonexistent/catalog.json"));
}

void ModWorkflowTest::findModsForRom_hashMatch()
{
    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture not found");

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromFile(path));

    auto mods = provider.findModsForRom(
        QString(), QString(),
        "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    QCOMPARE(mods.size(), 1);
    QCOMPARE(mods.first().id, QStringLiteral("test-mod-alpha"));
}

void ModWorkflowTest::findModsForRom_crc32Match()
{
    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture not found");

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromFile(path));

    auto mods = provider.findModsForRom("AABBCCDD", QString(), QString());
    QVERIFY(mods.size() >= 1);

    bool foundAlpha = false;
    for (const auto &m : mods) {
        if (m.id == "test-mod-alpha") foundAlpha = true;
    }
    QVERIFY(foundAlpha);
}

void ModWorkflowTest::findModsForRom_noMatch()
{
    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture not found");

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromFile(path));

    auto mods = provider.findModsForRom("00000000", "00000000000000000000000000000000",
                                        "0000000000000000000000000000000000000000");
    QVERIFY(mods.isEmpty());
}

void ModWorkflowTest::findModsBySystem()
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

void ModWorkflowTest::getModById()
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

void ModWorkflowTest::dbModInstallation_insertAndQuery()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "mod_crud_test"));

    int libId = db.insertLibrary("/roms", "TestLib");
    QVERIFY(libId > 0);
    int sysId = db.getSystemId("NES");
    QVERIFY(sysId > 0);

    FileRecord baseFile;
    baseFile.libraryId    = libId;
    baseFile.filename     = "test_rom.sfc";
    baseFile.originalPath = "/roms/test_rom.sfc";
    baseFile.currentPath  = "/roms/test_rom.sfc";
    baseFile.extension    = "sfc";
    baseFile.systemId     = sysId;
    baseFile.fileSize     = 1024;
    int baseId = db.insertFile(baseFile);
    QVERIFY(baseId > 0);

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

    auto installs = db.getModInstallations(baseId);
    QCOMPARE(installs.size(), 1);
    QCOMPARE(installs.first().catalogModId, QStringLiteral("test-mod-alpha"));
    QCOMPARE(installs.first().modTitle, QStringLiteral("Test Mod Alpha"));

    QVERIFY(db.removeModInstallation(instId));
    QVERIFY(db.getModInstallations(baseId).isEmpty());

    db.close();
}

void ModWorkflowTest::dbModInstallation_removeNonexistent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "mod_remove_test"));

    QVERIFY(!db.removeModInstallation(999));

    db.close();
}

// ── ModWorkflowService install tests ─────────────────────────────────────

void ModWorkflowTest::install_patchVerificationFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

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
    QVERIFY(baseFile.id > 0);

    ModEntry mod;
    mod.id        = "test-sha1-fail";
    mod.title     = "SHA1 Fail Mod";
    mod.type      = "hack";
    mod.patchUrl  = patchPath;
    mod.patchSha1 = "0000000000000000000000000000000000000000";
    mod.format    = "ips";

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);

    ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");
    QVERIFY(!result.success);
    QVERIFY(result.error.contains("SHA1"));

    db.close();
}

void ModWorkflowTest::install_missingPatchFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "missing_patch_test"));

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
    mod.id       = "missing";
    mod.title    = "Missing Patch";
    mod.type     = "hack";
    mod.patchUrl = "/nonexistent/patch.ips";
    mod.format   = "ips";

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);
    ModInstallResult result = workflow.install(baseFile, mod, dir.path() + "/out");

    QVERIFY(!result.success);
    QVERIFY(result.error.contains("not found"));

    db.close();
}

void ModWorkflowTest::install_rollsBackWhenRecordingFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString basePath = dir.path() + "/rom.sfc";
    QFile baseDiskFile(basePath);
    QVERIFY(baseDiskFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(baseDiskFile, QByteArrayLiteral("base rom data")));
    baseDiskFile.close();

    const QString patchPath = dir.path() + "/patch.ips";
    QFile patchFile(patchPath);
    QVERIFY(patchFile.open(QIODevice::WriteOnly));
    QVERIFY(writeAll(patchFile, QByteArrayLiteral("PATCHEOF")));
    patchFile.close();

    Database db;
    QVERIFY(db.initialize(dir.path() + "/test.db", "install_rollback_test"));

    const int libId = db.insertLibrary(dir.path(), "TestLib");
    QVERIFY(libId > 0);
    const int sysId = db.getSystemId("SNES");
    QVERIFY(sysId > 0);

    FileRecord baseFile;
    baseFile.libraryId = libId;
    baseFile.systemId = sysId;
    baseFile.filename = QStringLiteral("rom.sfc");
    baseFile.originalPath = basePath;
    baseFile.currentPath = basePath;
    baseFile.extension = QStringLiteral("sfc");
    baseFile.fileSize = QFileInfo(basePath).size();
    baseFile.id = db.insertFile(baseFile);
    QVERIFY(baseFile.id > 0);

    QSqlQuery trigger(db.database());
    QVERIFY(trigger.exec(QStringLiteral(R"(
        CREATE TRIGGER fail_mod_installation
        BEFORE INSERT ON mod_installations
        BEGIN
            SELECT RAISE(ABORT, 'forced mod installation failure');
        END
    )")));

    ModEntry mod;
    mod.id = QStringLiteral("rollback-mod");
    mod.title = QStringLiteral("Rollback Mod");
    mod.type = QStringLiteral("hack");
    mod.patchUrl = patchPath;
    mod.format = QStringLiteral("ips");

    PatchService patchSvc;
    ModWorkflowService workflow(db, patchSvc);

    const QString outDir = dir.path() + "/out";
    const ModInstallResult result = workflow.install(baseFile, mod, outDir);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("failed to record mod installation"), Qt::CaseInsensitive));
    QCOMPARE(result.patchedFileId, 0);
    QVERIFY(!QFile::exists(outDir + "/rom [Rollback Mod].sfc"));
    QCOMPARE(db.getAllFiles().size(), 1);
    QVERIFY(db.getModInstallations(baseFile.id).isEmpty());

    db.close();
}

// ── Phase 2: URL loading + cache tests ───────────────────────────────────

void ModWorkflowTest::loadFromUrl_fetchesAndCaches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture not found");

    const QString tempCatalog = dir.path() + "/mods.json";
    QVERIFY(QFile::copy(path, tempCatalog));

    const QUrl url = QUrl::fromLocalFile(tempCatalog);

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromUrl(url, true));
    QCOMPARE(provider.allMods().size(), 3);

    const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
    QVERIFY(QFile::exists(cacheFile));

    QVERIFY(removeIfExists(cacheFile));
}

void ModWorkflowTest::loadFromUrl_usesCacheWhenFresh()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture not found");

    const QUrl url(QStringLiteral("https://example.com/catalog_cache_test.json"));
    const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
    QVERIFY(QDir().mkpath(ModCatalogProvider::cacheDir()));

    QVERIFY(QFile::copy(path, cacheFile));

    QFile cf(cacheFile);
    QVERIFY(cf.open(QIODevice::ReadWrite));
    cf.close();

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromUrl(url, false));
    QCOMPARE(provider.allMods().size(), 3);

    QVERIFY(removeIfExists(cacheFile));
}

void ModWorkflowTest::loadFromUrl_networkErrorFallsBackToCache()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = catalogPath();
    QVERIFY2(!path.isEmpty(), "Fixture not found");

    const QUrl url(QStringLiteral("https://unreachable.invalid/catalog.json"));
    const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
    QVERIFY(QDir().mkpath(ModCatalogProvider::cacheDir()));
    QVERIFY(QFile::copy(path, cacheFile));

    ModCatalogProvider provider;
    QVERIFY(provider.loadFromUrl(url, true));
    QCOMPARE(provider.allMods().size(), 3);

    QVERIFY(removeIfExists(cacheFile));
}

void ModWorkflowTest::loadFromUrl_networkErrorNoCache()
{
    const QUrl url(QStringLiteral("https://unreachable.invalid/no_cache.json"));
    const QString cacheFile = ModCatalogProvider::cacheFileForUrl(url);
    QVERIFY(removeIfExists(cacheFile));

    ModCatalogProvider provider;
    QVERIFY(!provider.loadFromUrl(url, true));
    QVERIFY(!provider.lastError().isEmpty());
}

QTEST_MAIN(ModWorkflowTest)
