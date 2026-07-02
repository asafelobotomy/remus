#include "title_similarity.h"

#include <QChar>
#include <QHash>
#include <QSet>
#include <algorithm>

namespace Remus {
namespace TitleSimilarity {

namespace {

    bool charMatch(QChar a, QChar b) {
        return a.toLower() == b.toLower();
    }

    int commonPrefixLength(const QString &a, const QString &b, int maxPrefix) {
        const int lenA = static_cast<int>(a.size());
        const int lenB = static_cast<int>(b.size());
        const int limit = std::min(std::min(lenA, lenB), maxPrefix);
        int prefix = 0;
        for (int i = 0; i < limit; ++i) {
            if (!charMatch(a.at(i), b.at(i)))
                break;
            ++prefix;
        }
        return prefix;
    }

    float jaroSimilarity(const QString &s1, const QString &s2) {
        if (s1.isEmpty() && s2.isEmpty())
            return 1.0f;
        if (s1.isEmpty() || s2.isEmpty())
            return 0.0f;
        if (s1 == s2)
            return 1.0f;

        const int len1 = s1.size();
        const int len2 = s2.size();
        const int matchDistance = std::max(len1, len2) / 2 - 1;

        QVector<bool> s1Matches(len1, false);
        QVector<bool> s2Matches(len2, false);

        int matches = 0;
        for (int i = 0; i < len1; ++i) {
            const int start = std::max(0, i - matchDistance);
            const int end = std::min(i + matchDistance + 1, len2);
            for (int j = start; j < end; ++j) {
                if (s2Matches.at(j) || !charMatch(s1.at(i), s2.at(j)))
                    continue;
                s1Matches[i] = true;
                s2Matches[j] = true;
                ++matches;
                break;
            }
        }

        if (matches == 0)
            return 0.0f;

        int transpositions = 0;
        int k = 0;
        for (int i = 0; i < len1; ++i) {
            if (!s1Matches.at(i))
                continue;
            while (!s2Matches.at(k))
                ++k;
            if (!charMatch(s1.at(i), s2.at(k)))
                ++transpositions;
            ++k;
        }

        const float m = static_cast<float>(matches);
        const float t = static_cast<float>(transpositions) / 2.0f;
        return (m / static_cast<float>(len1) + m / static_cast<float>(len2)
            + (m - t) / m)
            / 3.0f;
    }

} // namespace

QStringList tokenizeForMatching(const QString &tokenString) {
    QStringList tokens;
    QString current;
    current.reserve(16);
    for (const QChar &ch : tokenString) {
        if (ch.isLetterOrNumber()) {
            current.append(ch.toLower());
        } else if (!current.isEmpty()) {
            tokens.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

float tokenSetRatio(const QString &tokenStringA, const QString &tokenStringB) {
    const QStringList tokensA = tokenizeForMatching(tokenStringA);
    const QStringList tokensB = tokenizeForMatching(tokenStringB);
    if (tokensA.isEmpty() && tokensB.isEmpty())
        return 1.0f;
    if (tokensA.isEmpty() || tokensB.isEmpty())
        return 0.0f;

    QSet<QString> setA(tokensA.cbegin(), tokensA.cend());
    QSet<QString> setB(tokensB.cbegin(), tokensB.cend());

    int intersection = 0;
    for (const QString &token : setA) {
        if (setB.contains(token))
            ++intersection;
    }

    return (2.0f * static_cast<float>(intersection))
        / static_cast<float>(setA.size() + setB.size());
}

float jaroWinkler(const QString &a, const QString &b) {
    const float jaro = jaroSimilarity(a, b);
    if (jaro <= 0.0f)
        return 0.0f;

    constexpr int kPrefixScale = 4;
    constexpr float kScalingFactor = 0.1f;
    const int prefix = commonPrefixLength(a, b, kPrefixScale);
    return jaro + static_cast<float>(prefix) * kScalingFactor * (1.0f - jaro);
}

Scores scorePair(const QString &tokenStringA, const QString &tokenStringB) {
    Scores scores;
    scores.tokenSet = tokenSetRatio(tokenStringA, tokenStringB);
    scores.jaroWinkler = jaroWinkler(tokenStringA, tokenStringB);
    scores.combined = std::max(scores.tokenSet, scores.jaroWinkler);
    return scores;
}

MatchTier classifyConservative(const Scores &scores, bool exactNormalizedMatch) {
    if (exactNormalizedMatch)
        return MatchTier::Exact;

    if (scores.tokenSet >= Thresholds::HighTokenSet
        || (scores.tokenSet >= Thresholds::HighTokenSetWithJaro
            && scores.jaroWinkler >= Thresholds::HighJaroWinkler)) {
        return MatchTier::HighConfidence;
    }

    if (scores.tokenSet >= Thresholds::ReviewTokenSetMin
        || scores.jaroWinkler >= Thresholds::ReviewJaroWinklerMin) {
        return MatchTier::Review;
    }

    return MatchTier::Reject;
}

} // namespace TitleSimilarity
} // namespace Remus
