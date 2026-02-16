#include <QtTest/QtTest>
#include "../src/core/template_engine.h"

using namespace Remus;

/**
 * @brief Unit tests for TemplateEngine
 * 
 * Tests template processing including:
 * - Variable substitution
 * - Article movement in titles
 * - Disc number extraction
 * - Empty group cleanup
 * - Template validation
 */
class TemplateEngineTest : public QObject {
    Q_OBJECT

private slots:
    // Variable substitution tests
    void testSubstitutionBasic();
    void testSubstitutionMissingVariables();
    void testSubstitutionAllVariables();
    void testSubstitutionNoVariables();
    void testSubstitutionEmptyValue();

    // Article movement tests
    void testMoveArticleThe();
    void testMoveArticleA();
    void testMoveArticleAn();
    void testMoveArticleNone();
    void testMoveArticleCaseInsensitive();
    void testMoveArticleNotAtStart();

    // Disc number extraction tests
    void testExtractDiscNumberBasic();
    void testExtractDiscNumberPadded();
    void testExtractDiscNumberInParens();
    void testExtractDiscNumberCaseInsensitive();
    void testExtractDiscNumberNoDisc();
    void testExtractDiscNumberMultiple();

    // Title normalization tests
    void testNormalizeTitleBasic();
    void testNormalizeTitleWithArticle();
    void testNormalizeTitleSpecialChars();
    void testNormalizeTitleEmpty();

    // Empty group cleanup tests
    void testCleanupEmptyParens();
    void testCleanupEmptyBrackets();
    void testCleanupMultipleSpaces();
    void testCleanupSpaceBeforeExtension();
    void testCleanupMixed();

    // Template validation tests
    void testValidateTemplateValid();
    void testValidateTemplateUnbalancedBraces();
    void testValidateTemplateInvalidVariable();
    void testValidateTemplateNoVariables();

    // Full template application tests
    void testApplyNoIntroTemplate();
    void testApplyRedumpTemplate();
    void testApplyCustomTemplate();
};

// ============================================================================
// Variable Substitution Tests
// ============================================================================

void TemplateEngineTest::testSubstitutionBasic() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Super Mario Bros";
    metadata.region = "USA";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".nes";
    
    QString result = engine.applyTemplate("{title} ({region}){ext}", metadata, fileInfo);
    QVERIFY(result.contains("Super Mario Bros"));
    QVERIFY(result.contains("USA"));
    QVERIFY(result.contains(".nes"));
}

void TemplateEngineTest::testSubstitutionMissingVariables() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Test Game";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    // Region not provided, should result in empty parentheses
    QString result = engine.applyTemplate("{title} ({region}){ext}", metadata, fileInfo);
    QVERIFY(result.contains("Test Game"));
}

void TemplateEngineTest::testSubstitutionAllVariables() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Final Fantasy VII";
    metadata.region = "USA";
    metadata.releaseDate = "1997-09-07";
    metadata.publisher = "Square";
    metadata.system = "PlayStation";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".bin";
    fileInfo["disc"] = "1";
    
    QString result = engine.applyTemplate(
        "{title} ({region}) ({year}) - {publisher}{ext}", metadata, fileInfo);
    
    QVERIFY(result.contains("Final Fantasy VII"));
    QVERIFY(result.contains("USA"));
    QVERIFY(result.contains("1997"));
    QVERIFY(result.contains("Square"));
}

void TemplateEngineTest::testSubstitutionNoVariables() {
    TemplateEngine engine;
    GameMetadata metadata;
    
    QString result = engine.applyTemplate("static_filename.rom", metadata, {});
    QCOMPARE(result, QString("static_filename.rom"));
}

void TemplateEngineTest::testSubstitutionEmptyValue() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "";
    metadata.region = "USA";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    QString result = engine.applyTemplate("{title} ({region}){ext}", metadata, fileInfo);
    // Should handle empty title gracefully
    QVERIFY(result.contains("USA"));
}

// ============================================================================
// Article Movement Tests
// ============================================================================

void TemplateEngineTest::testMoveArticleThe() {
    QString result = TemplateEngine::moveArticleToEnd("The Legend of Zelda");
    QCOMPARE(result, QString("Legend of Zelda, The"));
}

void TemplateEngineTest::testMoveArticleA() {
    QString result = TemplateEngine::moveArticleToEnd("A Link to the Past");
    QCOMPARE(result, QString("Link to the Past, A"));
}

void TemplateEngineTest::testMoveArticleAn() {
    QString result = TemplateEngine::moveArticleToEnd("An American Tail");
    QCOMPARE(result, QString("American Tail, An"));
}

void TemplateEngineTest::testMoveArticleNone() {
    QString result = TemplateEngine::moveArticleToEnd("Super Mario Bros");
    QCOMPARE(result, QString("Super Mario Bros"));
}

void TemplateEngineTest::testMoveArticleCaseInsensitive() {
    QString result1 = TemplateEngine::moveArticleToEnd("the Legend");
    QString result2 = TemplateEngine::moveArticleToEnd("THE LEGEND");
    
    // Function normalizes articles to proper case ("The", not "the" or "THE")
    QCOMPARE(result1, QString("Legend, The"));
    QCOMPARE(result2, QString("LEGEND, The"));
}

void TemplateEngineTest::testMoveArticleNotAtStart() {
    QString result = TemplateEngine::moveArticleToEnd("Legend of The Dragons");
    QCOMPARE(result, QString("Legend of The Dragons")); // "The" not at start
}

// ============================================================================
// Disc Number Extraction Tests
// ============================================================================

void TemplateEngineTest::testExtractDiscNumberBasic() {
    int disc = TemplateEngine::extractDiscNumber("Final Fantasy VII (USA) (Disc 1).bin");
    QCOMPARE(disc, 1);
}

void TemplateEngineTest::testExtractDiscNumberPadded() {
    int disc = TemplateEngine::extractDiscNumber("Game (Disc 02).iso");
    QCOMPARE(disc, 2);
}

void TemplateEngineTest::testExtractDiscNumberInParens() {
    int disc = TemplateEngine::extractDiscNumber("Game (USA) (Disc 3) (Rev 1).cue");
    QCOMPARE(disc, 3);
}

void TemplateEngineTest::testExtractDiscNumberCaseInsensitive() {
    int disc1 = TemplateEngine::extractDiscNumber("Game (disc 5).bin");
    int disc2 = TemplateEngine::extractDiscNumber("Game (DISC 5).bin");
    
    QCOMPARE(disc1, 5);
    QCOMPARE(disc2, 5);
}

void TemplateEngineTest::testExtractDiscNumberNoDisc() {
    int disc = TemplateEngine::extractDiscNumber("Single Disc Game.iso");
    QCOMPARE(disc, 0);
}

void TemplateEngineTest::testExtractDiscNumberMultiple() {
    // Should extract first occurrence
    int disc = TemplateEngine::extractDiscNumber("Disc 2 of Disc 4");
    QCOMPARE(disc, 2);
}

// ============================================================================
// Title Normalization Tests
// ============================================================================

void TemplateEngineTest::testNormalizeTitleBasic() {
    QString result = TemplateEngine::normalizeTitle("Super Mario Bros");
    QCOMPARE(result, QString("Super Mario Bros"));
}

void TemplateEngineTest::testNormalizeTitleWithArticle() {
    QString result = TemplateEngine::normalizeTitle("The Legend of Zelda");
    QCOMPARE(result, QString("Legend of Zelda, The"));
}

void TemplateEngineTest::testNormalizeTitleSpecialChars() {
    QString result = TemplateEngine::normalizeTitle("Pokémon™ Red");
    QVERIFY(!result.contains("™")); // Trademark should be removed
}

void TemplateEngineTest::testNormalizeTitleEmpty() {
    QString result = TemplateEngine::normalizeTitle("");
    QCOMPARE(result, QString(""));
}

// ============================================================================
// Empty Group Cleanup Tests (tested indirectly through applyTemplate)
// ============================================================================

void TemplateEngineTest::testCleanupEmptyParens() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Game Name";
    metadata.region = "USA";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    // Empty languages variable will create empty parens
    QString result = engine.applyTemplate("{title} ({languages}) ({region}){ext}", metadata, fileInfo);
    QVERIFY(!result.contains("()"));
    QVERIFY(result.contains("(USA)"));
}

void TemplateEngineTest::testCleanupEmptyBrackets() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Game";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    // Empty tags will create empty brackets
    QString result = engine.applyTemplate("{title} [{tags}]{ext}", metadata, fileInfo);
    QVERIFY(!result.contains("[]"));
}

void TemplateEngineTest::testCleanupMultipleSpaces() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Game    Name"; // Multiple spaces in title
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    QString result = engine.applyTemplate("{title}{ext}", metadata, fileInfo);
    // Should normalize multiple spaces
    QVERIFY(!result.contains("    "));
}

void TemplateEngineTest::testCleanupSpaceBeforeExtension() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Game Name";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    QString result = engine.applyTemplate("{title} {ext}", metadata, fileInfo);
    // Should remove space before extension
    QVERIFY(!result.contains(" .rom"));
    QVERIFY(result.endsWith(".rom"));
}

void TemplateEngineTest::testCleanupMixed() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Game";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".rom";
    
    QString result = engine.applyTemplate("{title} ({languages}) [{tags}] {ext}", metadata, fileInfo);
    // Should clean up all empty groups and spaces
    QVERIFY(!result.contains("()"));
    QVERIFY(!result.contains("[]"));
    QVERIFY(!result.contains("  "));
}

// ============================================================================
// Template Validation Tests
// ============================================================================

void TemplateEngineTest::testValidateTemplateValid() {
    TemplateEngine engine;
    bool valid = engine.validateTemplate("{title} ({region}){ext}");
    QVERIFY(valid);
}

void TemplateEngineTest::testValidateTemplateUnbalancedBraces() {
    TemplateEngine engine;
    bool valid1 = engine.validateTemplate("{title ({region}){ext}");
    bool valid2 = engine.validateTemplate("{title} {region}}{ext}");
    
    QVERIFY(!valid1);
    QVERIFY(!valid2);
}

void TemplateEngineTest::testValidateTemplateInvalidVariable() {
    TemplateEngine engine;
    bool valid = engine.validateTemplate("{title} ({invalid_var}){ext}");
    QVERIFY(!valid);
}

void TemplateEngineTest::testValidateTemplateNoVariables() {
    TemplateEngine engine;
    bool valid = engine.validateTemplate("static_name.rom");
    QVERIFY(valid);
}

// ============================================================================
// Full Template Application Tests
// ============================================================================

void TemplateEngineTest::testApplyNoIntroTemplate() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Super Mario Bros";
    metadata.region = "USA";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".nes";
    
    QString tmpl = TemplateEngine::getNoIntroTemplate();
    QString result = engine.applyTemplate(tmpl, metadata, fileInfo);
    
    QVERIFY(result.contains("Super Mario Bros"));
    QVERIFY(result.contains(".nes"));
}

void TemplateEngineTest::testApplyRedumpTemplate() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Final Fantasy VII";
    metadata.region = "USA";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".bin";
    fileInfo["disc"] = "1";
    
    QString tmpl = TemplateEngine::getRedumpTemplate();
    QString result = engine.applyTemplate(tmpl, metadata, fileInfo);
    
    QVERIFY(result.contains("Final Fantasy VII"));
    QVERIFY(result.contains(".bin"));
}

void TemplateEngineTest::testApplyCustomTemplate() {
    TemplateEngine engine;
    GameMetadata metadata;
    metadata.title = "Sonic";
    metadata.system = "Genesis";
    metadata.releaseDate = "1991-06-23";
    
    QMap<QString, QString> fileInfo;
    fileInfo["ext"] = ".md";
    
    QString tmpl = "{system} - {title} ({year}){ext}";
    QString result = engine.applyTemplate(tmpl, metadata, fileInfo);
    
    QVERIFY(result.contains("Genesis"));
    QVERIFY(result.contains("Sonic"));
    QVERIFY(result.contains("1991"));
    QVERIFY(result.contains(".md"));
}

QTEST_MAIN(TemplateEngineTest)
#include "test_template_engine.moc"
