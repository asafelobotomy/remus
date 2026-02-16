#pragma once

#include "../metadata/metadata_provider.h"
#include <QString>
#include <QDateTime>

namespace Remus {

/**
 * @brief Match result with confidence scoring
 * 
 * Represents a match between a file and game metadata,
 * with confidence score based on matching method.
 */
struct Match {
    int fileId = 0;              // Foreign key to files table
    int gameId = 0;              // Foreign key to games table (if exists)
    
    QString providerName;         // Provider that found the match
    QString providerId;           // Provider-specific game ID
    
    // Confidence scoring (0-100%)
    int confidence = 0;
    QString matchMethod;         // "hash", "exact_name", "fuzzy_name", "manual"
    
    // Match details
    QString matchedHash;         // Hash that matched (if hash-based)
    QString matchedName;         // Name that matched
    float nameMatchScore = 0.0f; // Levenshtein distance score
    
    // Metadata from provider
    QString title;
    QString system;
    QString region;
    QString description;
    
    // Status
    bool reviewed = false;       // User has reviewed this match
    bool userConfirmed = false;  // User confirmed this is correct
    QDateTime matchedAt;
};

/**
 * @brief Confidence levels for matches
 */
enum class ConfidenceLevel {
    Perfect = 100,      // Hash match OR user confirmation
    High = 90,          // Exact filename match
    Medium = 70,        // Close fuzzy match (80%+ similarity)
    Low = 50,           // Distant fuzzy match (60-80% similarity)
    Unknown = 0         // No match
};

/**
 * @brief Matching engine that combines file hashing with metadata providers
 * 
 * Implements three-tier matching strategy:
 * 1. Hash-based matching (100% confidence)
 * 2. Exact name matching (90% confidence)
 * 3. Fuzzy name matching (50-80% confidence based on Levenshtein distance)
 */
class MatchingEngine : public QObject {
    Q_OBJECT

public:
    explicit MatchingEngine(QObject *parent = nullptr);
    ~MatchingEngine() override = default;

    /**
     * @brief Match a file against metadata providers
     * 
     * @param filePath Path to the file
     * @param hash File hash (CRC32/MD5/SHA1)
     * @param fileName Base filename without extension
     * @param system System identifier
     * @return Match result with confidence score
     */
    Match matchFile(const QString &filePath, const QString &hash, 
                   const QString &fileName, const QString &system);
    
    /**
     * @brief Calculate confidence score based on matching method
     * @param method "hash", "exact_name", "fuzzy_name", "manual"
     * @param nameMatchScore For fuzzy matches, the similarity score (0.0-1.0)
     * @return Confidence percentage (0-100)
     */
    static int calculateConfidence(const QString &method, float nameMatchScore = 0.0f);
    
    /**
     * @brief Calculate Levenshtein distance between two strings
     * @param s1 First string
     * @param s2 Second string
     * @return Similarity score (0.0 = completely different, 1.0 = identical)
     */
    static float calculateNameSimilarity(const QString &s1, const QString &s2);
    
    /**
     * @brief Normalize a filename for matching
     * 
     * Removes file extension, region tags, version tags, etc.
     * "Super Mario Bros. (USA).nes" → "super mario bros"
     * 
     * @param fileName Original filename
     * @return Normalized filename for comparison
     */
    static QString normalizeFileName(const QString &fileName);
    
    /**
     * @brief Extract game title from No-Intro/Redump formatted filename
     * @param fileName Formatted filename
     * @return Game title (without region, version, etc.)
     */
    static QString extractGameTitle(const QString &fileName);

signals:
    /**
     * @brief Emitted when matching process starts
     * @param fileName File being matched
     */
    void matchingStarted(const QString &fileName);
    
    /**
     * @brief Emitted when hash matching attempt begins
     */
    void tryingHashMatch();
    
    /**
     * @brief Emitted when name matching attempt begins
     */
    void tryingNameMatch();
    
    /**
     * @brief Emitted when matching completes
     * @param match Result with confidence score
     */
    void matchingCompleted(const Match &match);
    
    /**
     * @brief Emitted when no match found
     */
    void noMatchFound();

private:
    /**
     * @brief Calculate Levenshtein distance (edit distance) between strings
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance (lower = more similar)
     */
    static int levenshteinDistance(const QString &s1, const QString &s2);
};

} // namespace Remus
