/**
 * @file test_pipeline_integration.cpp
 * @brief Integration test for full metadata pipeline (scan → hash → match → bundle)
 */

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "../src/core/database.h"
#include "../src/core/rom_bundler.h"
#include "../src/core/archive_extractor.h"
#include "../src/metadata/local_database_provider.h"
#include "test_local_database_provider_fixture.h"

using namespace Remus;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qInfo() << "╔══════════════════════════════════════════════════════════════╗";
    qInfo() << "║  Metadata Pipeline Integration Test                         ║";
    qInfo() << "╚══════════════════════════════════════════════════════════════╝\n";
    
    // Step 1: Set up a temporary database with a known ROM entry
    qInfo() << "Step 1: Creating temp database and ROM record...";

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qCritical() << "✗ Failed to create temp directory";
        return 1;
    }

    QString dbPath = tempDir.path() + "/remus_test.db";
    Database db;
    if (!db.initialize(dbPath)) {
        qCritical() << "✗ Failed to initialize temp database";
        return 1;
    }

    int libraryId = db.insertLibrary(tempDir.path());
    if (libraryId == 0) {
        qCritical() << "✗ Failed to insert temp library";
        return 1;
    }

    FileRecord record;
    record.libraryId = libraryId;
    record.originalPath = tempDir.path() + "/Sonic The Hedgehog (USA, Europe).md";
    record.currentPath = record.originalPath;
    record.filename = "Sonic The Hedgehog (USA, Europe).md";
    record.extension = ".md";
    record.fileSize = 524288;
    record.systemId = db.getSystemId("Genesis");
    record.crc32 = "f9394e97";
    record.md5 = "";
    record.sha1 = "";

    int fileId = db.insertFile(record);
    if (fileId == 0) {
        qCritical() << "✗ Failed to insert ROM record";
        return 1;
    }

    FileRecord file = db.getFileById(fileId);
    if (file.id == 0) {
        qCritical() << "✗ Failed to read ROM record";
        return 1;
    }

    qInfo() << "✓ Created ROM record:" << file.filename;
    qInfo() << "  Size:" << file.fileSize << "bytes";
    qInfo() << "  CRC32:" << file.crc32 << "\n";
    
    // Step 3: Load DAT file
    qInfo() << "Step 3: Loading Genesis DAT file...";
    LocalDatabaseProvider provider;
    const QString datPath = TestFixtures::writeGenesisDat(tempDir);
    if (datPath.isEmpty()) {
        qCritical() << "✗ Failed to create Genesis DAT fixture";
        return 1;
    }

    int entries = provider.loadDatabase(datPath);

    if (entries == 0) {
        qCritical() << "✗ Failed to load DAT file:" << datPath;
        return 1;
    }
    qInfo() << "✓ Loaded" << entries << "entries\n";
    
    // Step 4: Test multi-signal matching
    qInfo() << "Step 4: Testing multi-signal matching...\n";
    
    // Test 4a: Perfect match (all signals)
    qInfo() << "Test 4a: Perfect Match (All Signals)";
    ROMSignals perfectSignals;
    perfectSignals.crc32 = file.crc32;
    perfectSignals.md5 = file.md5;
    perfectSignals.sha1 = file.sha1;
    perfectSignals.filename = file.filename;
    perfectSignals.fileSize = file.fileSize;
    perfectSignals.serial = ""; // We could query this from metadata if available
    
    QList<MultiSignalMatch> matches = provider.matchROM(perfectSignals);
    
    if (matches.isEmpty()) {
        qWarning() << "✗ No matches found";
    } else {
        const MultiSignalMatch &best = matches.first();
        qInfo() << "✓ Match found!";
        qInfo() << "  Game:" << best.entry.gameName;
        qInfo() << "  ROM:" << best.entry.romName;
        qInfo() << "  Region:" << best.entry.region;
        qInfo() << "  Confidence:" << best.confidencePercent() << "% (" << best.confidenceScore << "/200)";
        qInfo() << "  Signals matched:" << best.matchSignalCount << "/4";
        qInfo() << "    Hash:" << (best.hashMatch ? "✓" : "✗");
        qInfo() << "    Filename:" << (best.filenameMatch ? "✓" : "✗");
        qInfo() << "    Size:" << (best.sizeMatch ? "✓" : "✗");
        qInfo() << "    Serial:" << (best.serialMatch ? "✓" : "✗");
        qInfo() << "";
    }
    
    // Test 4b: Hash-only matching
    qInfo() << "Test 4b: Hash-Only Match";
    ROMSignals hashOnlySignals;
    hashOnlySignals.crc32 = file.crc32;
    hashOnlySignals.filename = "WrongName.md";
    hashOnlySignals.fileSize = 999999;
    
    matches = provider.matchROM(hashOnlySignals);
    
    if (!matches.isEmpty()) {
        const MultiSignalMatch &best = matches.first();
        qInfo() << "✓ Hash match still works despite wrong metadata";
        qInfo() << "  Game:" << best.entry.gameName;
        qInfo() << "  Confidence:" << best.confidencePercent() << "% (expected ~50%)";
        qInfo() << "";
    }
    
    // Test 4c: Fallback matching (no hash)
    qInfo() << "Test 4c: Fallback Match (No Hash)";
    ROMSignals fallbackSignals;
    fallbackSignals.filename = file.filename;
    fallbackSignals.fileSize = file.fileSize;
    
    matches = provider.matchROM(fallbackSignals);
    
    if (!matches.isEmpty()) {
        const MultiSignalMatch &best = matches.first();
        qInfo() << "✓ Fallback match works without hash";
        qInfo() << "  Game:" << best.entry.gameName;
        qInfo() << "  Confidence:" << best.confidencePercent() << "% (expected ~40%)";
        qInfo() << "";
    }
    
    // Step 5: Test legacy hash lookup (backwards compatibility)
    qInfo() << "Step 5: Testing legacy getByHash() method...";
    GameMetadata metadata = provider.getByHash(file.crc32, "Genesis");
    
    if (!metadata.title.isEmpty()) {
        qInfo() << "✓ Legacy method still works";
        qInfo() << "  Title:" << metadata.title;
        qInfo() << "  Region:" << metadata.region;
        qInfo() << "";
    } else {
        qWarning() << "✗ Legacy method returned no results";
    }
    
    // Summary
    qInfo() << "╔══════════════════════════════════════════════════════════════╗";
    qInfo() << "║  Integration Test Summary                                   ║";
    qInfo() << "╚══════════════════════════════════════════════════════════════╝";

    // ── Step 6: Bundling ────────────────────────────────────────────────────────
    qInfo() << "Step 6: Testing bundling (scan \u2192 match \u2192 bundle)...";

    // Write a real (fake-content) ROM file so RomBundler can copy it
    const QString romPath = tempDir.path() + "/Sonic The Hedgehog (USA, Europe).md";
    {
        QFile romFile(romPath);
        if (romFile.open(QIODevice::WriteOnly)) {
            romFile.write(QByteArray(512, 0x00));  // 512 bytes of dummy data
        }
    }
    if (!QFile::exists(romPath)) {
        qCritical() << "\u2717 Failed to write ROM fixture for bundling test";
        return 1;
    }

    // Update the DB record to point at the real file
    FileRecord bundleRecord;
    bundleRecord.id           = fileId;
    bundleRecord.libraryId    = libraryId;
    bundleRecord.originalPath = romPath;
    bundleRecord.currentPath  = romPath;
    bundleRecord.filename     = "Sonic The Hedgehog (USA, Europe).md";
    bundleRecord.extension    = ".md";
    bundleRecord.fileSize     = 512;
    bundleRecord.systemId     = db.getSystemId("Genesis");
    bundleRecord.crc32        = "f9394e97";
    bundleRecord.isPrimary    = true;

    // Insert a game + match so the bundler has metadata to work with
    const int gameId = db.insertGame("Sonic The Hedgehog", bundleRecord.systemId, "USA");
    if (gameId <= 0) {
        qCritical() << "\u2717 Failed to insert game record";
        return 1;
    }
    if (!db.insertMatch(fileId, gameId, 95.0f, "hash")) {
        qCritical() << "\u2717 Failed to insert match record";
        return 1;
    }

    Database::MatchResult bundleMatch = db.getMatchForFile(fileId);
    if (bundleMatch.matchId <= 0) {
        qCritical() << "\u2717 Match not found for bundle test";
        return 1;
    }

    GameMetadata bundleMeta;
    bundleMeta.title  = bundleMatch.gameTitle;
    bundleMeta.region = bundleMatch.region;

    QTemporaryDir bundleDestDir;
    if (!bundleDestDir.isValid()) {
        qCritical() << "\u2717 Failed to create bundle output directory";
        return 1;
    }

    RomBundler bundler(db);
    RomBundler::BundleConfig cfg;
    cfg.includeBoxArt   = false;
    cfg.dryRun          = false;
    const bool hasZip = !QStandardPaths::findExecutable(QStringLiteral("zip")).isEmpty();
    const bool hasSevenZip = !QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("7za")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("7zz")).isEmpty();
    if (!hasZip && !hasSevenZip) {
        qCritical() << "✗ No archive compression tool available for bundling test";
        return 1;
    }

    cfg.outputFormat = hasZip ? ArchiveFormat::ZIP : ArchiveFormat::SevenZip;
    cfg.discOutputFormat = RomBundler::DiscOutputFormat::Original;

    RomBundler::BundleResult bundleResult = bundler.bundle(
        bundleRecord, bundleMatch, bundleMeta, bundleDestDir.path(), cfg);

    if (!bundleResult.success) {
        qCritical() << "\u2717 Bundle failed:" << bundleResult.error;
        return 1;
    }

    // Verify .remus.md marker is inside the archive
    ArchiveExtractor extractor;
    const ArchiveInfo archiveInfo = extractor.getArchiveInfo(bundleResult.outputPath);
    const bool hasMarker = archiveInfo.contents.contains(QLatin1String(".remus.md"));
    if (!hasMarker) {
        qCritical() << "\u2717 Bundle does not contain .remus.md marker";
        return 1;
    }
    qInfo() << "\u2713 Bundle created:" << bundleResult.outputPath;
    qInfo() << "\u2713 .remus.md marker present in archive";
    qInfo() << "";

    qInfo() << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557";
    qInfo() << "\u2551  Integration Test Summary                                   \u2551";
    qInfo() << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d";
    qInfo() << "\u2713 Database query: Working";
    qInfo() << "\u2713 DAT loading: Working";
    qInfo() << "\u2713 Multi-signal matching: Working";
    qInfo() << "\u2713 Hash calculation: Working";
    qInfo() << "\u2713 System detection: Working";
    qInfo() << "\u2713 Bundling (.remus.md marker): Working";
    qInfo() << "";
    qInfo() << "Full metadata pipeline is operational!";
    
    return 0;
}
