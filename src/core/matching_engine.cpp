#include "matching_engine.h"
#include "constants/confidence.h"
#include "constants/match_methods.h"
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>

namespace Remus {

MatchingEngine::MatchingEngine(QObject *parent)
    : QObject(parent) { }

int MatchingEngine::calculateConfidence(const QString &method, float nameMatchScore) {
    const QString canonicalMethod = Constants::MatchMethods::canonicalize(method);

    if (canonicalMethod == Constants::MatchMethods::HASH || canonicalMethod == Constants::MatchMethods::MANUAL) {
        return static_cast<int>(ConfidenceLevel::Perfect);
    } else if (canonicalMethod == Constants::MatchMethods::NAME) {
        return static_cast<int>(ConfidenceLevel::High);
    } else if (canonicalMethod == Constants::MatchMethods::FUZZY) {
        // Scale fuzzy matches based on similarity score
        if (nameMatchScore >= Constants::Confidence::FuzzyThresholds::MEDIUM_SIMILARITY) {
            return static_cast<int>(ConfidenceLevel::Medium);
        } else if (nameMatchScore >= Constants::Confidence::FuzzyThresholds::LOW_SIMILARITY) {
            return static_cast<int>(ConfidenceLevel::Low);
        } else {
            return static_cast<int>(Constants::Confidence::Thresholds::VERY_LOW);
        }
    }

    return static_cast<int>(ConfidenceLevel::Unknown);
}

QString MatchingEngine::normalizeFileName(const QString &fileName) {
    QString normalized = fileName;

    // Remove file extension
    QFileInfo info(normalized);
    normalized = info.completeBaseName();

    // Remove everything in parentheses (region, version, etc.)
    normalized.remove(QRegularExpression("\\([^)]*\\)"));

    // Remove everything in square brackets [tags]
    normalized.remove(QRegularExpression("\\[[^\\]]*\\]"));

    // Remove common separators and convert to lowercase
    normalized = normalized.replace('_', ' ').replace('-', ' ').replace('.', ' ').simplified().toLower();

    // Remove multiple spaces
    normalized = normalized.simplified();

    return normalized;
}

QString MatchingEngine::extractGameTitle(const QString &fileName) {
    QString title = fileName;

    // Remove file extension
    QFileInfo info(title);
    title = info.completeBaseName();

    // Extract text before first parenthesis (region marker)
    QRegularExpression regionPattern("^([^(]+)");
    QRegularExpressionMatch match = regionPattern.match(title);

    if (match.hasMatch()) {
        title = match.captured(1).trimmed();
    }

    // Clean up common separators
    title = title.replace('_', ' ').simplified();

    return title;
}

int MatchingEngine::levenshteinDistance(const QString &s1, const QString &s2) {
    const int len1 = s1.length();
    const int len2 = s2.length();

    QVector<int> prev(len2 + 1), curr(len2 + 1);
    for (int j = 0; j <= len2; ++j)
        prev[j] = j;

    for (int i = 1; i <= len1; ++i) {
        curr[0] = i;
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr[j] = std::min({ prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost });
        }
        std::swap(prev, curr);
    }

    return prev[len2];
}

float MatchingEngine::calculateNameSimilarity(const QString &s1, const QString &s2) {
    if (s1.isEmpty() || s2.isEmpty()) {
        return 0.0f;
    }

    // Normalize both strings for comparison
    QString norm1 = s1.toLower().simplified();
    QString norm2 = s2.toLower().simplified();

    // Exact match
    if (norm1 == norm2) {
        return 1.0f;
    }

    // Calculate Levenshtein distance
    int distance = levenshteinDistance(norm1, norm2);
    int maxLen = std::max(norm1.length(), norm2.length());

    // Convert distance to similarity score (0.0 to 1.0)
    float similarity = 1.0f - (static_cast<float>(distance) / maxLen);

    return std::max(0.0f, similarity);
}

} // namespace Remus
