#include <QtTest/QtTest>

#include "../src/core/title_similarity.h"

using namespace Remus::TitleSimilarity;

class TitleSimilarityTest : public QObject {
    Q_OBJECT

private slots:
    void tokenSetRatio_identicalTokens();
    void tokenSetRatio_partialOverlap();
    void tokenSetRatio_disjoint();
    void jaroWinkler_similarStrings();
    void classifyConservative_exactTier();
    void classifyConservative_highConfidence();
    void classifyConservative_reviewBand();
    void classifyConservative_reject();
};

void TitleSimilarityTest::tokenSetRatio_identicalTokens() {
    QCOMPARE(tokenSetRatio(QStringLiteral("super mario 64"), QStringLiteral("super mario 64")), 1.0f);
}

void TitleSimilarityTest::tokenSetRatio_partialOverlap() {
    const float score = tokenSetRatio(QStringLiteral("super mario 64"), QStringLiteral("mario 64"));
    QVERIFY(score >= 0.66f);
    QVERIFY(score < 1.0f);
}

void TitleSimilarityTest::tokenSetRatio_disjoint() {
    QCOMPARE(tokenSetRatio(QStringLiteral("zelda"), QStringLiteral("mario")), 0.0f);
}

void TitleSimilarityTest::jaroWinkler_similarStrings() {
    const float score = jaroWinkler(QStringLiteral("mario"), QStringLiteral("maro"));
    QVERIFY(score >= 0.90f);
}

void TitleSimilarityTest::classifyConservative_exactTier() {
    const Scores scores { 0.5f, 0.5f, 0.5f };
    QCOMPARE(classifyConservative(scores, true), MatchTier::Exact);
}

void TitleSimilarityTest::classifyConservative_highConfidence() {
    Scores scores;
    scores.tokenSet = 0.93f;
    scores.jaroWinkler = 0.70f;
    scores.combined = scores.tokenSet;
    QCOMPARE(classifyConservative(scores, false), MatchTier::HighConfidence);

    scores.tokenSet = 0.89f;
    scores.jaroWinkler = 0.91f;
    scores.combined = scores.jaroWinkler;
    QCOMPARE(classifyConservative(scores, false), MatchTier::HighConfidence);
}

void TitleSimilarityTest::classifyConservative_reviewBand() {
    Scores scores;
    scores.tokenSet = 0.85f;
    scores.jaroWinkler = 0.70f;
    scores.combined = scores.tokenSet;
    QCOMPARE(classifyConservative(scores, false), MatchTier::Review);
}

void TitleSimilarityTest::classifyConservative_reject() {
    Scores scores;
    scores.tokenSet = 0.50f;
    scores.jaroWinkler = 0.50f;
    scores.combined = 0.50f;
    QCOMPARE(classifyConservative(scores, false), MatchTier::Reject);
}

QTEST_MAIN(TitleSimilarityTest)

#include "test_title_similarity.moc"
