/**
 * @file test_multi_signal_matching.cpp
 * @brief Tests for multi-signal ROM matching with confidence scoring
 */

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include "../src/metadata/local_database_provider.h"
#include "../src/core/hasher.h"
#include "test_local_database_provider_fixture.h"

using namespace Remus;

/**
 * @brief Calculate file hash
 */
QString calculateHash(const QString &filePath, const QString &algorithm) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file:" << filePath;
        return QString();
    }
    
    QCryptographicHash::Algorithm algo;
    if (algorithm == "CRC32") {
        // CRC32 not in Qt's QCryptographicHash, use placeholder
        return QString();
    } else if (algorithm == "MD5") {
        algo = QCryptographicHash::Md5;
    } else if (algorithm == "SHA1") {
        algo = QCryptographicHash::Sha1;
    } else {
        return QString();
    }
    
    QByteArray data = file.readAll();
    QByteArray hash = QCryptographicHash::hash(data, algo);
    return hash.toHex();
}

/**
 * @brief Test 1: DAT Loading
 */
bool testDatLoading() {
    qInfo() << "\n=== Test 1: DAT Loading ===";

    LocalDatabaseProvider provider;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning() << "✗ Failed to create temp directory for DAT fixture";
        return false;
    }

    const QString datPath = TestFixtures::writeGenesisDat(tempDir);
    if (datPath.isEmpty()) {
        qWarning() << "✗ Failed to create Genesis DAT fixture";
        return false;
    }

    int entries = provider.loadDatabase(datPath);

    if (entries > 0) {
        qInfo() << "✓ DAT loaded successfully:" << entries << "entries";
        return true;
    } else {
        qWarning() << "✗ Failed to load DAT file";
        return false;
    }
}

/**
 * @brief Test 2: Hash-Only Matching
 */
bool testHashMatching(LocalDatabaseProvider &provider) {
    qInfo() << "\n=== Test 2: Hash-Only Matching ===";
    
    // Test with known Genesis ROM hash (Sonic The Hedgehog USA)
    ROMSignals romSignals;
    romSignals.crc32 = "f9394e97"; // Correct CRC32 for Sonic 1 (USA, Europe)
    romSignals.filename = "Sonic The Hedgehog (USA, Europe).md";
    romSignals.fileSize = 524288; // 512KB
    
    QList<MultiSignalMatch> matches = provider.matchROM(romSignals);
    
    if (!matches.isEmpty()) {
        qInfo() << "✓ Found" << matches.size() << "match(es)";
        
        const MultiSignalMatch &best = matches.first();
        qInfo() << "  Best match:" << best.entry.gameName;
        qInfo() << "  ROM name:" << best.entry.romName;
        qInfo() << "  Confidence:" << best.confidencePercent() << "%";
        qInfo() << "  Score:" << best.confidenceScore << "/200";
        qInfo() << "  Signals matched:" << best.matchSignalCount;
        qInfo() << "    Hash:" << (best.hashMatch ? "✓" : "✗");
        qInfo() << "    Filename:" << (best.filenameMatch ? "✓" : "✗");
        qInfo() << "    Size:" << (best.sizeMatch ? "✓" : "✗");
        qInfo() << "    Serial:" << (best.serialMatch ? "✓" : "✗");
        
        return best.confidencePercent() >= 50;
    } else {
        qWarning() << "✗ No matches found";
        return false;
    }
}

/**
 * @brief Test 3: Multi-Signal Matching (Hash + Filename + Size)
 */
bool testMultiSignalMatching(LocalDatabaseProvider &provider) {
    qInfo() << "\n=== Test 3: Multi-Signal Matching (All Signals) ===";
    
    ROMSignals romSignals;
    romSignals.crc32 = "f9394e97";
    romSignals.filename = "Sonic The Hedgehog (USA, Europe).md";
    romSignals.fileSize = 524288;
    
    QList<MultiSignalMatch> matches = provider.matchROM(romSignals);
    
    if (!matches.isEmpty()) {
        const MultiSignalMatch &best = matches.first();
        qInfo() << "✓ Perfect match scenario:";
        qInfo() << "  Game:" << best.entry.gameName;
        qInfo() << "  Confidence:" << best.confidencePercent() << "%";
        qInfo() << "  Expected: ≥150/200 (75%)";
        qInfo() << "  Actual:" << best.confidenceScore << "/200";
        
        // Should have hash + filename + size = 180 points minimum
        return best.confidenceScore >= 150;
    }
    
    return false;
}

/**
 * @brief Test 4: Filename + Size Matching (No Hash)
 */
bool testFallbackMatching(LocalDatabaseProvider &provider) {
    qInfo() << "\n=== Test 4: Fallback Matching (No Hash) ===";
    
    ROMSignals romSignals;
    // No hash provided
    romSignals.filename = "Sonic The Hedgehog (USA, Europe).md";
    romSignals.fileSize = 524288;
    
    QList<MultiSignalMatch> matches = provider.matchROM(romSignals);
    
    if (!matches.isEmpty()) {
        const MultiSignalMatch &best = matches.first();
        qInfo() << "✓ Fallback match found:";
        qInfo() << "  Game:" << best.entry.gameName;
        qInfo() << "  Confidence:" << best.confidencePercent() << "% (expected 40%)";
        qInfo() << "  Hash matched:" << (best.hashMatch ? "YES" : "NO (expected)");
        qInfo() << "  Filename matched:" << (best.filenameMatch ? "YES" : "NO");
        qInfo() << "  Size matched:" << (best.sizeMatch ? "YES" : "NO");
        
        // Should have filename + size = 80 points
        return !best.hashMatch && best.confidenceScore == 80;
    }
    
    qWarning() << "✗ No fallback matches found";
    return false;
}

/**
 * @brief Test 5: Real ROM File Hashing
 */
bool testRealROMFile() {
    qInfo() << "\n=== Test 5: Real ROM File Processing ===";
    
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(true);
    if (!tempFile.open()) {
        qWarning() << "✗ Failed to create temp ROM file";
        return false;
    }

    const QByteArray romData("rom-test-data");
    if (tempFile.write(romData) != romData.size()) {
        qWarning() << "✗ Failed to write temp ROM file";
        return false;
    }
    if (!tempFile.flush()) {
        qWarning() << "✗ Failed to flush temp ROM file";
        return false;
    }

    QFileInfo fileInfo(tempFile.fileName());
    qInfo() << "Processing:" << fileInfo.fileName();
    qInfo() << "Size:" << fileInfo.size() << "bytes";

    // Calculate hashes
    QString md5 = calculateHash(tempFile.fileName(), "MD5");
    QString sha1 = calculateHash(tempFile.fileName(), "SHA1");

    qInfo() << "MD5:" << md5;
    qInfo() << "SHA1:" << sha1;

    const QString expectedMd5 = "f51e29e35344589ef6eceb3b5041eefa";
    const QString expectedSha1 = "03b13c10937543722ceb63ed792a1125640a9d23";

    if (md5 == expectedMd5 && sha1 == expectedSha1) {
        qInfo() << "✓ Successfully calculated hashes from temp ROM file";
        return true;
    }

    qWarning() << "✗ Hashes did not match expected values";
    return false;
}

/**
 * @brief Test 6: Confidence Score Distribution
 */
bool testConfidenceScoring(LocalDatabaseProvider &provider) {
    qInfo() << "\n=== Test 6: Confidence Score Distribution ===";
    
    struct TestCase {
        QString name;
        ROMSignals romSignals;
        int expectedMin;
        int expectedMax;
    };
    
    QList<TestCase> cases = {
        {
            "Perfect Match (All 4 signals)",
            {"f9394e97", "1bc674be034e43c96b86487ac69d9293", "6ddb7de1e17e7f6cdb88927bd906352030daa194", "Sonic The Hedgehog (USA, Europe).md", 524288, "00001009-00"},
            150, 200
        },
        {
            "Hash Only",
            {"f9394e97", "", "", "WrongName.md", 999999, ""},
            100, 100
        },
        {
            "Filename + Size (No Hash)",
            {"", "", "", "Sonic The Hedgehog (USA, Europe).md", 524288, ""},
            80, 80
        }
    };
    
    bool allPassed = true;
    
    for (const TestCase &testCase : cases) {
        qInfo() << "\n  Testing:" << testCase.name;
        
        QList<MultiSignalMatch> matches = provider.matchROM(testCase.romSignals);
        
        if (!matches.isEmpty()) {
            int score = matches.first().confidenceScore;
            bool passed = (score >= testCase.expectedMin && score <= testCase.expectedMax);
            
            qInfo() << "    Score:" << score << "/200";
            qInfo() << "    Expected range:" << testCase.expectedMin << "-" << testCase.expectedMax;
            qInfo() << "    Result:" << (passed ? "✓ PASS" : "✗ FAIL");
            
            allPassed = allPassed && passed;
        } else {
            qInfo() << "    ✗ No matches found";
            allPassed = false;
        }
    }
    
    return allPassed;
}

/**
 * @brief Test 7: Serial Normalization Matching
 * Verifies that disc serials with manufacturer prefixes (MK-, HDR-) and
 * region suffixes (-50) are normalised to match bare numeric serials in DATs.
 */
bool testSerialNormalization() {
    qInfo() << "\n=== Test 7: Serial Normalization (Dreamcast) ===";

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        qWarning() << "✗ Cannot create temp directory";
        return false;
    }

    const QString datPath = TestFixtures::writeDreamcastDat(tempDir);
    if (datPath.isEmpty()) {
        qWarning() << "✗ Cannot write Dreamcast DAT fixture";
        return false;
    }

    LocalDatabaseProvider provider;
    int entries = provider.loadDatabase(datPath);
    qInfo() << "  Loaded" << entries << "Dreamcast DAT entries";
    if (entries == 0) {
        qWarning() << "✗ Failed to load Dreamcast DAT";
        return false;
    }

    bool allPassed = true;

    // Case 1: IP.BIN serial "MK-51000" should match DAT serial "51000" (USA)
    {
        ROMSignals romInput;
        romInput.serial = QStringLiteral("MK-51000");
        romInput.filename = QStringLiteral("Sonic Adventure (US).cdi");
        romInput.fileSize = 1185760800;

        QList<MultiSignalMatch> matches = provider.matchROM(romInput);
        bool found = false;
        for (const auto &m : matches) {
            if (m.serialMatch) {
                found = true;
                qInfo() << "  ✓ MK-51000 matched" << m.entry.gameName
                        << "(serial:" << m.entry.serial << ")";
                break;
            }
        }
        if (!found) {
            qWarning() << "  ✗ MK-51000 did NOT serial-match any entry";
            allPassed = false;
        }
    }

    // Case 2: "MK-51000-50" (full EU format) should also match "51000" via normalization
    {
        ROMSignals romInput;
        romInput.serial = QStringLiteral("MK-51000-50");
        romInput.filename = QStringLiteral("Sonic Adventure (Europe).cdi");
        romInput.fileSize = 1185760800;

        QList<MultiSignalMatch> matches = provider.matchROM(romInput);
        bool found = false;
        for (const auto &m : matches) {
            if (m.serialMatch) {
                found = true;
                qInfo() << "  ✓ MK-51000-50 matched" << m.entry.gameName
                        << "(serial:" << m.entry.serial << ")";
                break;
            }
        }
        if (!found) {
            qWarning() << "  ✗ MK-51000-50 did NOT serial-match any entry";
            allPassed = false;
        }
    }

    // Case 3: Exact match "51000" should still work
    {
        ROMSignals romInput;
        romInput.serial = QStringLiteral("51000");
        romInput.filename = QStringLiteral("Sonic Adventure (USA).bin");
        romInput.fileSize = 1185760800;

        QList<MultiSignalMatch> matches = provider.matchROM(romInput);
        bool found = false;
        for (const auto &m : matches) {
            if (m.serialMatch) {
                found = true;
                qInfo() << "  ✓ 51000 exact-matched" << m.entry.gameName
                        << "(serial:" << m.entry.serial << ")";
                break;
            }
        }
        if (!found) {
            qWarning() << "  ✗ 51000 did NOT serial-match any entry";
            allPassed = false;
        }
    }

    // Case 4: HDR-0001 should NOT match 51000 (different core numbers)
    {
        ROMSignals romInput;
        romInput.serial = QStringLiteral("HDR-0001");
        romInput.filename = QStringLiteral("Sonic Adventure (Japan).bin");
        romInput.fileSize = 1185760800;

        QList<MultiSignalMatch> matches = provider.matchROM(romInput);
        bool matchedUSA = false;
        for (const auto &m : matches) {
            if (m.serialMatch && m.entry.serial == QStringLiteral("51000")) {
                matchedUSA = true;
                break;
            }
        }
        if (matchedUSA) {
            qWarning() << "  ✗ HDR-0001 incorrectly matched USA entry (serial 51000)";
            allPassed = false;
        } else {
            qInfo() << "  ✓ HDR-0001 correctly did NOT match USA (serial 51000)";
        }
    }

    return allPassed;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qInfo() << "╔════════════════════════════════════════════════════════════╗";
    qInfo() << "║  Multi-Signal ROM Matching Test Suite                     ║";
    qInfo() << "╚════════════════════════════════════════════════════════════╝";

    int passed = 0;
    int total = 0;

    // Test 1: DAT Loading
    total++;
    if (testDatLoading()) {
        passed++;
    }
    
    // Create provider for remaining tests
    LocalDatabaseProvider provider;
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            qCritical() << "\n✗ Cannot continue: temp directory could not be created";
            return 1;
        }

        const QString datPath = TestFixtures::writeGenesisDat(tempDir);
    if (datPath.isEmpty()) {
            qCritical() << "\n✗ Cannot continue: Genesis DAT fixture could not be created";
        return 1;
    }

    int entries = provider.loadDatabase(datPath);
    
    if (entries == 0) {
        qCritical() << "\n✗ Cannot continue: Genesis DAT not loaded";
        qInfo() << "\nFixture path:" << datPath;
        return 1;
    }

    qInfo() << "\nGenesis DAT loaded:" << entries << "entries";

    // Test 2: Hash-Only Matching
    total++;
    if (testHashMatching(provider)) {
        passed++;
    }
    
    // Test 3: Multi-Signal Matching
    total++;
    if (testMultiSignalMatching(provider)) {
        passed++;
    }
    
    // Test 4: Fallback Matching
    total++;
    if (testFallbackMatching(provider)) {
        passed++;
    }
    
    // Test 5: Real ROM File
    total++;
    if (testRealROMFile()) {
        passed++;
    }
    
    // Test 6: Confidence Scoring
    total++;
    if (testConfidenceScoring(provider)) {
        passed++;
    }
    
    // Test 7: Serial Normalization (Dreamcast)
    total++;
    if (testSerialNormalization()) {
        passed++;
    }
    
    // Summary
    qInfo() << "\n╔════════════════════════════════════════════════════════════╗";
    qInfo() << "║  Test Results                                              ║";
    qInfo() << "╚════════════════════════════════════════════════════════════╝";
    qInfo() << "Passed:" << passed << "/" << total;
    qInfo() << "Success rate:" << (passed * 100 / total) << "%";
    
    if (passed == total) {
        qInfo() << "\n✓ All tests passed! Multi-signal matching is working correctly.";
        return 0;
    } else {
        qWarning() << "\n✗ Some tests failed. Please review the output above.";
        return 1;
    }
}
