#include <QtTest/QtTest>
#include "../src/core/scanner.h"
#include <QTemporaryDir>
#include <QFile>

using namespace Remus;

/**
 * @brief Unit tests for multi-file set detection
 * 
 * Tests Scanner's ability to detect and link:
 * - .cue/.bin file pairs
 * - .gdi track file sets
 * - .ccd/.img/.sub file sets
 * - .mds/.mdf file pairs
 */
class MultiFileDetectionTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();  // Cleanup before each test
    void cleanup(); // Cleanup after each test

    // CUE/BIN tests
    void testLinkCueBinPair();
    void testLinkCueMultipleBins();
    void testLinkCueBinMismatch();
    void testLinkCueImgFile();

    // GDI tests
    void testLinkGdiTracks();
    void testLinkGdiMissingTracks();
    void testLinkGdiEmptyFile();

    // CCD tests
    void testLinkCcdImgPair();
    void testLinkCcdWithSub();
    void testLinkCcdMismatch();

    // MDS tests
    void testLinkMdsMdfPair();
    void testLinkMdsMdfMismatch();

    // Edge cases
    void testMultipleFormatsInSameDir();
    void testNestedDirectories();
    void testNoPrimaryFiles();

private:
    QTemporaryDir *tempDir = nullptr;
    
    void createFile(const QString &relativePath, const QString &content = "");
    void createGdiFile(const QString &relativePath, const QStringList &tracks);
    QList<ScanResult> scanTestDirectory();
};

void MultiFileDetectionTest::initTestCase() {
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
}

void MultiFileDetectionTest::cleanupTestCase() {
    delete tempDir;
}

void MultiFileDetectionTest::init() {
    // Recreate temp directory before each test for complete isolation
    if (tempDir) {
        delete tempDir;
    }
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
}

void MultiFileDetectionTest::cleanup() {
    // Optional: could add per-test cleanup here if needed
}

void MultiFileDetectionTest::createFile(const QString &relativePath, const QString &content) {
    QString fullPath = tempDir->filePath(relativePath);
    QFileInfo info(fullPath);
    QDir().mkpath(info.absolutePath());
    
    QFile file(fullPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    if (!content.isEmpty()) {
        file.write(content.toUtf8());
    } else {
        file.write("dummy content");
    }
    file.close();
}

void MultiFileDetectionTest::createGdiFile(const QString &relativePath, const QStringList &tracks) {
    QString content = QString::number(tracks.size()) + "\n";
    for (int i = 0; i < tracks.size(); ++i) {
        content += QString("%1 0 4 2352 \"%2\" 0\n").arg(i + 1).arg(tracks[i]);
    }
    createFile(relativePath, content);
}

QList<ScanResult> MultiFileDetectionTest::scanTestDirectory() {
    Scanner scanner;
    scanner.setMultiFileDetection(true);
    return scanner.scan(tempDir->path());
}

// ============================================================================
// CUE/BIN Tests
// ============================================================================

void MultiFileDetectionTest::testLinkCueBinPair() {
    createFile("game.cue");
    createFile("game.bin");
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 2);
    
    // Find results
    ScanResult *cueFile = nullptr;
    ScanResult *binFile = nullptr;
    
    for (auto &result : results) {
        if (result.extension == ".cue") cueFile = &result;
        if (result.extension == ".bin") binFile = &result;
    }
    
    QVERIFY(cueFile != nullptr);
    QVERIFY(binFile != nullptr);
    
    // CUE should be primary, BIN should be secondary
    QVERIFY(cueFile->isPrimary);
    QVERIFY(!binFile->isPrimary);
    QCOMPARE(binFile->parentFilePath, cueFile->path);
}

void MultiFileDetectionTest::testLinkCueMultipleBins() {
    createFile("multi.cue");
    createFile("multi.bin"); // Matching base name will link
    createFile("another.bin"); // Different base name won't link
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 3);
    
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    // CUE and unlinked bin should be primary (2 total)
    QCOMPARE(primaryCount, 2);
}

void MultiFileDetectionTest::testLinkCueBinMismatch() {
    createFile("game1.cue");
    createFile("game2.bin"); // Different base name
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 2);
    
    // Both should be primary (no link)
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, 2);
}

void MultiFileDetectionTest::testLinkCueImgFile() {
    createFile("game.cue");
    createFile("game.img"); // IMG extension also supported
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 2);
    
    // Find IMG file
    ScanResult *imgFile = nullptr;
    for (auto &result : results) {
        if (result.extension == ".img") imgFile = &result;
    }
    
    QVERIFY(imgFile != nullptr);
    QVERIFY(!imgFile->isPrimary); // Should be linked
}

// ============================================================================
// GDI Tests
// ============================================================================

void MultiFileDetectionTest::testLinkGdiTracks() {
    QStringList tracks = {"track01.bin", "track02.raw", "track03.bin"};
    
    createGdiFile("game.gdi", tracks);
    for (const QString &track : tracks) {
        createFile(track);
    }
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 4); // 1 GDI + 3 tracks
    
    // GDI should be primary
    ScanResult *gdiFile = nullptr;
    int secondaryCount = 0;
    
    for (auto &result : results) {
        if (result.extension == ".gdi") {
            gdiFile = &result;
        } else if (!result.isPrimary) {
            secondaryCount++;
        }
    }
    
    QVERIFY(gdiFile != nullptr);
    QVERIFY(gdiFile->isPrimary);
    QCOMPARE(secondaryCount, 3); // All tracks should be secondary
}

void MultiFileDetectionTest::testLinkGdiMissingTracks() {
    QStringList tracks = {"track01.bin", "track02.raw"};
    
    createGdiFile("game.gdi", tracks);
    createFile("track01.bin"); // Only create first track
    
    QList<ScanResult> results = scanTestDirectory();
    
    // GDI should be primary
    bool foundGdi = false;
    for (const auto &result : results) {
        if (result.extension == ".gdi" && result.isPrimary) {
            foundGdi = true;
        }
    }
    
    QVERIFY(foundGdi);
}

void MultiFileDetectionTest::testLinkGdiEmptyFile() {
    createFile("empty.gdi", "0\n"); // Zero tracks
    
    QList<ScanResult> results = scanTestDirectory();
    
    // Should find GDI but not link anything
    QCOMPARE(results.size(), 1);
    QVERIFY(results[0].isPrimary);
}

// ============================================================================
// CCD Tests
// ============================================================================

void MultiFileDetectionTest::testLinkCcdImgPair() {
    createFile("game.ccd");
    createFile("game.img");
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 2);
    
    ScanResult *ccdFile = nullptr;
    ScanResult *imgFile = nullptr;
    
    for (auto &result : results) {
        if (result.extension == ".ccd") ccdFile = &result;
        if (result.extension == ".img") imgFile = &result;
    }
    
    QVERIFY(ccdFile != nullptr);
    QVERIFY(imgFile != nullptr);
    QVERIFY(ccdFile->isPrimary);
    QVERIFY(!imgFile->isPrimary);
    QCOMPARE(imgFile->parentFilePath, ccdFile->path);
}

void MultiFileDetectionTest::testLinkCcdWithSub() {
    createFile("game.ccd");
    createFile("game.img");
    createFile("game.sub");
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 3);
    
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, 1); // Only CCD is primary
}

void MultiFileDetectionTest::testLinkCcdMismatch() {
    createFile("game1.ccd");
    createFile("game2.img");
    
    QList<ScanResult> results = scanTestDirectory();
    
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, 2); // No link, both primary
}

// ============================================================================
// MDS Tests
// ============================================================================

void MultiFileDetectionTest::testLinkMdsMdfPair() {
    createFile("game.mds");
    createFile("game.mdf");
    
    QList<ScanResult> results = scanTestDirectory();
    
    QCOMPARE(results.size(), 2);
    
    ScanResult *mdsFile = nullptr;
    ScanResult *mdfFile = nullptr;
    
    for (auto &result : results) {
        if (result.extension == ".mds") mdsFile = &result;
        if (result.extension == ".mdf") mdfFile = &result;
    }
    
    QVERIFY(mdsFile != nullptr);
    QVERIFY(mdfFile != nullptr);
    QVERIFY(mdsFile->isPrimary);
    QVERIFY(!mdfFile->isPrimary);
    QCOMPARE(mdfFile->parentFilePath, mdsFile->path);
}

void MultiFileDetectionTest::testLinkMdsMdfMismatch() {
    createFile("game1.mds");
    createFile("game2.mdf");
    
    QList<ScanResult> results = scanTestDirectory();
    
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, 2); // No link
}

// ============================================================================
// Edge Case Tests
// ============================================================================

void MultiFileDetectionTest::testMultipleFormatsInSameDir() {
    // Mix of different formats
    createFile("psx_game.cue");
    createFile("psx_game.bin");
    createFile("saturn_game.ccd");
    createFile("saturn_game.img");
    createFile("dreamcast_game.gdi", "1\n1 0 4 2352 \"track.bin\" 0\n");
    createFile("track.bin");
    
    QList<ScanResult> results = scanTestDirectory();
    
    // Count primary files (should be 3: cue, ccd, gdi)
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, 3);
}

void MultiFileDetectionTest::testNestedDirectories() {
    createFile("dir1/game.cue");
    createFile("dir1/game.bin");
    createFile("dir2/other.cue");
    createFile("dir2/other.bin");
    
    QList<ScanResult> results = scanTestDirectory();
    
    // Should find 4 files, 2 primary (both cues)
    QCOMPARE(results.size(), 4);
    
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, 2);
}

void MultiFileDetectionTest::testNoPrimaryFiles() {
    // Only secondary files without their primaries (use unique names)
    createFile("standalone1.bin");  // Would link to standalone1.cue if it existed
    createFile("standalone2.img");  // Would link to standalone2.cue/ccd if it existed
    createFile("standalone3.mdf");  // Would link to standalone3.mds if it existed
    
    QList<ScanResult> results = scanTestDirectory();
    
    // All should be primary since no matching cue/ccd/mds exists
    int primaryCount = 0;
    for (const auto &result : results) {
        if (result.isPrimary) primaryCount++;
    }
    
    QCOMPARE(primaryCount, results.size());
}

QTEST_MAIN(MultiFileDetectionTest)
#include "test_multi_file_detection.moc"
