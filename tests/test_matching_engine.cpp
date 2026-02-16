#include <QtTest/QtTest>
#include "../src/core/matching_engine.h"
#include "../src/core/constants/match_methods.h"

using namespace Remus;
using namespace Remus::Constants;

/**
 * @brief Unit tests for MatchingEngine
 * 
 * Tests matching logic including:
 * - Confidence calculation for different match types
 * - Levenshtein distance edge cases
 * - Name normalization and title extraction
 * - Name similarity scoring
 */
class MatchingEngineTest : public QObject {
    Q_OBJECT

private slots:
    // Confidence calculation tests
    void testConfidenceHashMatch();
    void testConfidenceExactNameMatch();
    void testConfidenceFuzzyMatchHigh();
    void testConfidenceFuzzyMatchMedium();
    void testConfidenceFuzzyMatchLow();
    void testConfidenceManualMatch();
    void testConfidenceUnknown();

    // Name normalization tests
    void testNormalizeFileNameBasic();
    void testNormalizeFileNameWithRegion();
    void testNormalizeFileNameWithTags();
    void testNormalizeFileNameWithUnderscores();
    void testNormalizeFileNameEmpty();
    void testNormalizeFileNameSpecialChars();

    // Title extraction tests
    void testExtractGameTitleBasic();
    void testExtractGameTitleWithRegion();
    void testExtractGameTitleWithVersion();
    void testExtractGameTitleEmpty();
    void testExtractGameTitleNoIntroFormat();

    // Levenshtein distance tests
    void testLevenshteinIdentical();
    void testLevenshteinEmpty();
    void testLevenshteinOneEmpty();
    void testLevenshteinCompletelyDifferent();
    void testLevenshteinSingleCharDifference();
    void testLevenshteinCaseInsensitive();

    // Name similarity tests
    void testNameSimilarityPerfectMatch();
    void testNameSimilarityCloseMatch();
    void testNameSimilarityPartialMatch();
    void testNameSimilarityNoMatch();
    void testNameSimilarityEmptyStrings();
};

// ============================================================================
// Confidence Calculation Tests
// ============================================================================

void MatchingEngineTest::testConfidenceHashMatch() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::HASH, 0.0f);
    QCOMPARE(confidence, static_cast<int>(ConfidenceLevel::Perfect));
    QCOMPARE(confidence, 100);
}

void MatchingEngineTest::testConfidenceExactNameMatch() {
    int confidence = MatchingEngine::calculateConfidence("exact_name", 1.0f);
    QCOMPARE(confidence, static_cast<int>(ConfidenceLevel::High));
    QCOMPARE(confidence, 90);
}

void MatchingEngineTest::testConfidenceFuzzyMatchHigh() {
    // 85% similarity should be Medium confidence (70%)
    int confidence = MatchingEngine::calculateConfidence("fuzzy_name", 0.85f);
    QCOMPARE(confidence, static_cast<int>(ConfidenceLevel::Medium));
    QCOMPARE(confidence, 70);
}

void MatchingEngineTest::testConfidenceFuzzyMatchMedium() {
    // 70% similarity should be Low confidence (50%)
    int confidence = MatchingEngine::calculateConfidence("fuzzy_name", 0.70f);
    QCOMPARE(confidence, static_cast<int>(ConfidenceLevel::Low));
    QCOMPARE(confidence, 50);
}

void MatchingEngineTest::testConfidenceFuzzyMatchLow() {
    // 50% similarity is below Low threshold, should return 40
    int confidence = MatchingEngine::calculateConfidence("fuzzy_name", 0.50f);
    QCOMPARE(confidence, 40);
}

void MatchingEngineTest::testConfidenceManualMatch() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::HASH, 0.0f);
    QCOMPARE(confidence, static_cast<int>(ConfidenceLevel::Perfect));
    QCOMPARE(confidence, 100);
}

void MatchingEngineTest::testConfidenceUnknown() {
    int confidence = MatchingEngine::calculateConfidence("unknown_method", 0.0f);
    QCOMPARE(confidence, 0);
}

// ============================================================================
// Name Normalization Tests
// ============================================================================

void MatchingEngineTest::testNormalizeFileNameBasic() {
    QString input = "Super Mario Bros.nes";
    QString expected = "super mario bros";
    QString result = MatchingEngine::normalizeFileName(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testNormalizeFileNameWithRegion() {
    QString input = "Super Mario Bros. (USA).nes";
    QString expected = "super mario bros";
    QString result = MatchingEngine::normalizeFileName(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testNormalizeFileNameWithTags() {
    QString input = "Super Mario Bros. (USA) [!].nes";
    QString expected = "super mario bros";
    QString result = MatchingEngine::normalizeFileName(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testNormalizeFileNameWithUnderscores() {
    QString input = "Super_Mario_Bros_3.nes";
    QString expected = "super mario bros 3";
    QString result = MatchingEngine::normalizeFileName(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testNormalizeFileNameEmpty() {
    QString input = "";
    QString expected = "";
    QString result = MatchingEngine::normalizeFileName(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testNormalizeFileNameSpecialChars() {
    QString input = "Mega-Man-X (USA) (Rev 1).sfc";
    QString expected = "mega man x";
    QString result = MatchingEngine::normalizeFileName(input);
    QCOMPARE(result, expected);
}

// ============================================================================
// Title Extraction Tests
// ============================================================================

void MatchingEngineTest::testExtractGameTitleBasic() {
    QString input = "Sonic the Hedgehog.md";
    QString expected = "Sonic the Hedgehog";
    QString result = MatchingEngine::extractGameTitle(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testExtractGameTitleWithRegion() {
    QString input = "Sonic the Hedgehog (USA, Europe).md";
    QString expected = "Sonic the Hedgehog";
    QString result = MatchingEngine::extractGameTitle(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testExtractGameTitleWithVersion() {
    QString input = "Street Fighter II (USA) (Rev A).sfc";
    QString expected = "Street Fighter II";
    QString result = MatchingEngine::extractGameTitle(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testExtractGameTitleEmpty() {
    QString input = "";
    QString expected = "";
    QString result = MatchingEngine::extractGameTitle(input);
    QCOMPARE(result, expected);
}

void MatchingEngineTest::testExtractGameTitleNoIntroFormat() {
    QString input = "Legend of Zelda, The - A Link to the Past (USA).sfc";
    QString expected = "Legend of Zelda, The - A Link to the Past";
    QString result = MatchingEngine::extractGameTitle(input);
    QCOMPARE(result, expected);
}

// ============================================================================
// Levenshtein Distance Tests
// ============================================================================

void MatchingEngineTest::testLevenshteinIdentical() {
    float similarity = MatchingEngine::calculateNameSimilarity("mario", "mario");
    QCOMPARE(similarity, 1.0f);
}

void MatchingEngineTest::testLevenshteinEmpty() {
    float similarity = MatchingEngine::calculateNameSimilarity("", "");
    QCOMPARE(similarity, 0.0f);
}

void MatchingEngineTest::testLevenshteinOneEmpty() {
    float similarity = MatchingEngine::calculateNameSimilarity("mario", "");
    QCOMPARE(similarity, 0.0f);
}

void MatchingEngineTest::testLevenshteinCompletelyDifferent() {
    float similarity = MatchingEngine::calculateNameSimilarity("abc", "xyz");
    QVERIFY(similarity < 0.5f); // Should be very low
}

void MatchingEngineTest::testLevenshteinSingleCharDifference() {
    float similarity = MatchingEngine::calculateNameSimilarity("mario", "marii");
    QVERIFY(similarity > 0.7f); // Should be quite similar (1 char different out of 5)
}

void MatchingEngineTest::testLevenshteinCaseInsensitive() {
    float similarity = MatchingEngine::calculateNameSimilarity("Mario", "MARIO");
    QCOMPARE(similarity, 1.0f); // Should be case-insensitive
}

// ============================================================================
// Name Similarity Tests
// ============================================================================

void MatchingEngineTest::testNameSimilarityPerfectMatch() {
    float similarity = MatchingEngine::calculateNameSimilarity(
        "Super Mario Bros", "Super Mario Bros");
    QCOMPARE(similarity, 1.0f);
}

void MatchingEngineTest::testNameSimilarityCloseMatch() {
    // "Super Mario Bros." vs "Super Mario Bros 3"
    float similarity = MatchingEngine::calculateNameSimilarity(
        "Super Mario Bros", "Super Mario Bros 3");
    QVERIFY(similarity > 0.8f); // Very similar, just extra character
}

void MatchingEngineTest::testNameSimilarityPartialMatch() {
    // "Super Mario" vs "Super Mario World"
    float similarity = MatchingEngine::calculateNameSimilarity(
        "Super Mario", "Super Mario World");
    QVERIFY(similarity > 0.6f && similarity < 0.9f);
}

void MatchingEngineTest::testNameSimilarityNoMatch() {
    float similarity = MatchingEngine::calculateNameSimilarity(
        "Zelda", "Metroid");
    QVERIFY(similarity < 0.3f);
}

void MatchingEngineTest::testNameSimilarityEmptyStrings() {
    float similarity1 = MatchingEngine::calculateNameSimilarity("", "Mario");
    float similarity2 = MatchingEngine::calculateNameSimilarity("Mario", "");
    float similarity3 = MatchingEngine::calculateNameSimilarity("", "");
    
    QCOMPARE(similarity1, 0.0f);
    QCOMPARE(similarity2, 0.0f);
    QCOMPARE(similarity3, 0.0f);
}

QTEST_MAIN(MatchingEngineTest)
#include "test_matching_engine.moc"
