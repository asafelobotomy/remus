/**
 * @file test_multi_signal_matching.cpp
 * @brief Tests for multi-signal ROM matching with confidence scoring
 */

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include "../src/metadata/local_database_provider.h"
#include "../src/core/hasher.h"

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
    
    // Try to load Genesis DAT
    QString datPath = "/home/solon/Documents/remus/data/databases/Nintendo - Game Boy Advance.dat";
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
    
    QString romPath = "/home/solon/Documents/remus/tests/rom_tests/Sonic The Hedgehog (USA, Europe)/Sonic The Hedgehog (USA, Europe).md";
    
    QFileInfo fileInfo(romPath);
    if (!fileInfo.exists()) {
        qWarning() << "✗ Test ROM not found:" << romPath;
        return false;
    }
    
    qInfo() << "Processing:" << fileInfo.fileName();
    qInfo() << "Size:" << fileInfo.size() << "bytes";
    
    // Calculate hashes
    QString md5 = calculateHash(romPath, "MD5");
    QString sha1 = calculateHash(romPath, "SHA1");
    
    qInfo() << "MD5:" << md5;
    qInfo() << "SHA1:" << sha1;
    
    if (!md5.isEmpty() && !sha1.isEmpty()) {
        qInfo() << "✓ Successfully calculated hashes from real ROM file";
        return true;
    }
    
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
    QString datPath = "/home/solon/Documents/remus/data/databases/Sega - Mega Drive - Genesis.dat";
    int entries = provider.loadDatabase(datPath);
    
    if (entries == 0) {
        qCritical() << "\n✗ Cannot continue: Genesis DAT not loaded";
        qInfo() << "\nPlease ensure DAT file exists at:" << datPath;
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
