#include "matching_engine.h"
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>

namespace Remus {

MatchingEngine::MatchingEngine(QObject *parent)
    : QObject(parent)
{
}

Match MatchingEngine::matchFile(const QString &filePath, const QString &hash,
                                const QString &fileName, const QString &system)
{
    emit matchingStarted(fileName);
    
    Match match;
    match.matchedAt = QDateTime::currentDateTime();
    
    // Step 1: Try hash-based matching (highest confidence)
    if (!hash.isEmpty()) {
        emit tryingHashMatch();
        qInfo() << "Attempting hash match for:" << fileName << "(" << hash << ")";
        
        // Hash match would be implemented by ProviderOrchestrator
        // For now, we mark the method and store the hash
        match.matchedHash = hash;
        match.matchMethod = "hash_pending";
        match.confidence = 0; // Will be set to 100 if hash matches
    }
    
    // Step 2: Extract and normalize filename
    QString normalizedName = normalizeFileName(fileName);
    QString gameTitle = extractGameTitle(fileName);
    
    qInfo() << "Normalized name:" << normalizedName;
    qInfo() << "Extracted title:" << gameTitle;
    
    match.matchedName = gameTitle;
    
    // Confidence will be calculated based on provider response
    // This is a placeholder structure that would be filled by the orchestrator
    
    return match;
}

int MatchingEngine::calculateConfidence(const QString &method, float nameMatchScore)
{
    if (method == "hash" || method == "manual") {
        return static_cast<int>(ConfidenceLevel::Perfect);
    } else if (method == "exact_name") {
        return static_cast<int>(ConfidenceLevel::High);
    } else if (method == "fuzzy_name") {
        // Scale fuzzy matches based on similarity score
        if (nameMatchScore >= 0.8f) {
            return static_cast<int>(ConfidenceLevel::Medium);
        } else if (nameMatchScore >= 0.6f) {
            return static_cast<int>(ConfidenceLevel::Low);
        } else {
            return 40; // Very low confidence
        }
    }
    
    return static_cast<int>(ConfidenceLevel::Unknown);
}

QString MatchingEngine::normalizeFileName(const QString &fileName)
{
    QString normalized = fileName;
    
    // Remove file extension
    QFileInfo info(normalized);
    normalized = info.completeBaseName();
    
    // Remove everything in parentheses (region, version, etc.)
    normalized.remove(QRegularExpression("\\([^)]*\\)"));
    
    // Remove everything in square brackets [tags]
    normalized.remove(QRegularExpression("\\[[^\\]]*\\]"));
    
    // Remove common separators and convert to lowercase
    normalized = normalized.replace('_', ' ')
                          .replace('-', ' ')
                          .replace('.', ' ')
                          .simplified()
                          .toLower();
    
    // Remove multiple spaces
    normalized = normalized.simplified();
    
    return normalized;
}

QString MatchingEngine::extractGameTitle(const QString &fileName)
{
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

int MatchingEngine::levenshteinDistance(const QString &s1, const QString &s2)
{
    const int len1 = s1.length();
    const int len2 = s2.length();
    
    // Create distance matrix
    QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));
    
    // Initialize first row and column
    for (int i = 0; i <= len1; ++i) {
        d[i][0] = i;
    }
    for (int j = 0; j <= len2; ++j) {
        d[0][j] = j;
    }
    
    // Calculate minimum edit distance
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            
            d[i][j] = std::min({
                d[i - 1][j] + 1,      // deletion
                d[i][j - 1] + 1,      // insertion
                d[i - 1][j - 1] + cost // substitution
            });
        }
    }
    
    return d[len1][len2];
}

float MatchingEngine::calculateNameSimilarity(const QString &s1, const QString &s2)
{
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
