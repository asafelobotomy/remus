#pragma once

#include <QString>
#include <QStringList>

namespace Remus {
namespace TitleSimilarity {

struct Scores {
    float tokenSet = 0.0f;
    float jaroWinkler = 0.0f;
    float combined = 0.0f;
};

namespace Thresholds {
    inline constexpr float HighTokenSet = 0.92f;
    inline constexpr float HighTokenSetWithJaro = 0.88f;
    inline constexpr float HighJaroWinkler = 0.90f;
    inline constexpr float ReviewTokenSetMin = 0.80f;
    inline constexpr float ReviewJaroWinklerMin = 0.82f;
    inline constexpr float AmbiguityTokenSetDelta = 0.03f;
} // namespace Thresholds

enum class MatchTier { Exact, HighConfidence, Review, Reject };

QStringList tokenizeForMatching(const QString &tokenString);
float tokenSetRatio(const QString &tokenStringA, const QString &tokenStringB);
float jaroWinkler(const QString &a, const QString &b);
Scores scorePair(const QString &tokenStringA, const QString &tokenStringB);
MatchTier classifyConservative(const Scores &scores, bool exactNormalizedMatch);

} // namespace TitleSimilarity
} // namespace Remus
