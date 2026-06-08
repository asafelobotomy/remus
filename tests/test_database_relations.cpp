#include "test_database_fixture.h"

void DatabaseTest::testInsertAndFindAppliedPatch() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    Database::AppliedPatchRecord record;
    record.basePath = "/roms/base.sfc";
    record.outputPath = "/roms/base [English v2.0].sfc";
    record.patchPath = "/patches/english_v2.bps";
    record.patchFormat = "BPS";
    record.baseTitle = "Base Game";
    record.patchName = "English v2.0";
    record.fileType = "translation";
    record.sourceChecksum = "11111111";
    record.targetChecksum = "22222222";
    record.patchChecksum = "33333333";
    record.baseCrc32 = "AAAA1111";
    record.baseMd5 = "base-md5";
    record.baseSha1 = "base-sha1";
    record.outputCrc32 = "BBBB2222";
    record.outputMd5 = "output-md5";
    record.outputSha1 = "output-sha1";

    QVERIFY(db.insertAppliedPatch(record));

    const Database::AppliedPatchRecord found
        = db.findAppliedPatchByOutputHashes("BBBB2222", "output-md5", "output-sha1");
    QVERIFY(found.id > 0);
    QCOMPARE(found.baseTitle, QStringLiteral("Base Game"));
    QCOMPARE(found.patchName, QStringLiteral("English v2.0"));
    QCOMPARE(found.fileType, QStringLiteral("translation"));
}

void DatabaseTest::testGetFilesByParent() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("PlayStation");

    FileRecord cue = makeRecord(libId, sysId, "game.cue");
    cue.isPrimary = true;
    int parentId = db.insertFile(cue);
    QVERIFY(parentId > 0);

    FileRecord bin1 = makeRecord(libId, sysId, "game (Track 1).bin");
    bin1.isPrimary = false;
    bin1.parentFileId = parentId;
    int child1 = db.insertFile(bin1);
    QVERIFY(child1 > 0);

    FileRecord bin2 = makeRecord(libId, sysId, "game (Track 2).bin");
    bin2.isPrimary = false;
    bin2.parentFileId = parentId;
    int child2 = db.insertFile(bin2);
    QVERIFY(child2 > 0);

    QList<FileRecord> children = db.getFilesByParent(parentId);
    QCOMPARE(children.size(), 2);
}

void DatabaseTest::testGetAllMatches() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));

    int gid1 = db.insertGame("Super Mario Bros.", sysId);
    int gid2 = db.insertGame("Legend of Zelda", sysId);

    db.insertMatch(fid1, gid1, 100.0f, "hash");
    db.insertMatch(fid2, gid2, 85.0f, "fuzzy");

    QMap<int, Database::MatchResult> matches = db.getAllMatches();
    QCOMPARE(matches.size(), 2);
    QVERIFY(matches.contains(fid1));
    QVERIFY(matches.contains(fid2));
    QCOMPARE(matches[fid1].gameTitle, QStringLiteral("Super Mario Bros."));
    QCOMPARE(matches[fid2].matchMethod, QStringLiteral("fuzzy"));
}

void DatabaseTest::testInsertAndGetModInstallation() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int baseId = db.insertFile(makeRecord(libId, sysId, "base.nes"));
    int patchedId = db.insertFile(makeRecord(libId, sysId, "patched.nes"));

    Database::ModInstallationRecord mod;
    mod.baseFileId = baseId;
    mod.patchedFileId = patchedId;
    mod.catalogModId = "mod-123";
    mod.modTitle = "Translation Patch";
    mod.modAuthor = "translator";
    mod.modVersion = "1.0";
    mod.modType = "translation";
    mod.patchFormat = "BPS";
    int modId = db.insertModInstallation(mod);
    QVERIFY(modId > 0);

    QList<Database::ModInstallationRecord> mods = db.getModInstallations(baseId);
    QCOMPARE(mods.size(), 1);
    QCOMPARE(mods.first().modTitle, QStringLiteral("Translation Patch"));
    QCOMPARE(mods.first().catalogModId, QStringLiteral("mod-123"));
}

void DatabaseTest::testRemoveModInstallation() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int baseId = db.insertFile(makeRecord(libId, sysId, "base.nes"));
    int patchedId = db.insertFile(makeRecord(libId, sysId, "patched.nes"));

    Database::ModInstallationRecord mod;
    mod.baseFileId = baseId;
    mod.patchedFileId = patchedId;
    mod.catalogModId = "mod-456";
    mod.modTitle = "Hack";
    int modId = db.insertModInstallation(mod);
    QVERIFY(modId > 0);

    QVERIFY(db.removeModInstallation(modId));
    QCOMPARE(db.getModInstallations(baseId).size(), 0);
}

void DatabaseTest::testUpsertAndGetCatalogCache() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    Database::ModCatalogCacheRecord cache;
    cache.sourceUrl = "https://example.com/catalog.json";
    cache.etag = "abc123";
    cache.modCount = 42;
    int cacheId = db.upsertCatalogCache(cache);
    QVERIFY(cacheId > 0);

    Database::ModCatalogCacheRecord got = db.getCatalogCache("https://example.com/catalog.json");
    QVERIFY(got.id > 0);
    QCOMPARE(got.etag, QStringLiteral("abc123"));
    QCOMPARE(got.modCount, 42);

    cache.etag = "def456";
    cache.modCount = 50;
    int updatedId = db.upsertCatalogCache(cache);
    QVERIFY(updatedId > 0);

    Database::ModCatalogCacheRecord updated = db.getCatalogCache("https://example.com/catalog.json");
    QCOMPARE(updated.etag, QStringLiteral("def456"));
    QCOMPARE(updated.modCount, 50);

    Database::ModCatalogCacheRecord missing = db.getCatalogCache("https://nonexistent.example.com");
    QCOMPARE(missing.id, 0);
}

void DatabaseTest::testDeleteLibraryCascadesFiles() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms/cascade", "Cascade Test");
    int sysId = db.getSystemId("NES");

    int fid1 = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int fid2 = db.insertFile(makeRecord(libId, sysId, "zelda.nes"));
    QVERIFY(fid1 > 0);
    QVERIFY(fid2 > 0);

    QCOMPARE(db.getFilesBySystem("NES").size(), 2);

    QVERIFY(db.deleteLibrary(libId));
    QVERIFY(db.getLibraryPath(libId).isEmpty());
    QCOMPARE(db.getFileById(fid1).id, 0);
    QCOMPARE(db.getFileById(fid2).id, 0);
}

void DatabaseTest::testInsertMatchWithNameMatchScore() {
    Database db;
    QVERIFY(db.initialize(":memory:"));

    int libId = db.insertLibrary("/roms", "Test");
    int sysId = db.getSystemId("NES");
    int fileId = db.insertFile(makeRecord(libId, sysId, "mario.nes"));
    int gameId = db.insertGame("Super Mario Bros.", sysId);

    QVERIFY(db.insertMatch(fileId, gameId, 85.0f, "fuzzy", 0.92f));

    Database::MatchResult m = db.getMatchForFile(fileId);
    QCOMPARE(m.matchMethod, QStringLiteral("fuzzy"));
    QVERIFY(m.nameMatchScore >= 0.91f && m.nameMatchScore <= 0.93f);
}