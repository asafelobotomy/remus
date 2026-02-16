#pragma once

#include <QString>

namespace Remus {
namespace Constants {
namespace Confidence {

// ============================================================================
// Confidence Thresholds
// ============================================================================

namespace Thresholds {
    /// Perfect match: Hash match OR user confirmation
    inline constexpr float PERFECT = 100.0f;
    
    /// Hash match: Exact hash database match
    inline constexpr float HASH_MATCH = 100.0f;
    
    /// User confirmed: Manual user confirmation
    inline constexpr float USER_CONFIRMED = 100.0f;
    
    /// Exact name match: Filename matches database exactly
    inline constexpr float EXACT_NAME = 90.0f;
    
    /// High confidence threshold: >= 90%
    inline constexpr float HIGH = 90.0f;
    
    /// Medium confidence threshold: >= 60%
    inline constexpr float MEDIUM = 60.0f;
    
    /// Low confidence threshold: < 60%
    inline constexpr float LOW = 0.0f;
    
    /// Fuzzy match minimum: Minimum similarity for fuzzy matching
    inline constexpr float FUZZY_MIN = 60.0f;
    
    /// Fuzzy match maximum: Maximum similarity for fuzzy matching
    inline constexpr float FUZZY_MAX = 80.0f;
    
    /// Default minimum for filtering: Default filter threshold
    inline constexpr float DEFAULT_MINIMUM = 60.0f;
}

// ============================================================================
// Confidence Categories
// ============================================================================

/**
 * @brief Confidence level categories for match quality
 */
enum class Category {
    Perfect,        ///< 100% - hash match or user confirmed
    High,           ///< >= 90% - exact name match
    Medium,         ///< 60-89% - close fuzzy match
    Low,            ///< < 60% - distant fuzzy match
    Unmatched       ///< 0% - no match found
};

/**
 * @brief Match method types
 */
enum class MatchMethod {
    Hash,           ///< Hash database match (100%)
    Exact,          ///< Exact filename match (90%)
    Fuzzy,          ///< Fuzzy string matching (60-80%)
    UserConfirmed,  ///< User manual confirmation (100%)
    Unmatched       ///< No match found (0%)
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get confidence category from percentage
 * @param confidence Confidence value (0-100)
 * @return Category enum value
 */
inline Category getCategory(float confidence) {
    if (confidence >= Thresholds::PERFECT) return Category::Perfect;
    if (confidence >= Thresholds::HIGH) return Category::High;
    if (confidence >= Thresholds::MEDIUM) return Category::Medium;
    if (confidence > 0.0f) return Category::Low;
    return Category::Unmatched;
}

/**
 * @brief Get category label for display
 * @param cat Category enum value
 * @return Human-readable label
 */
inline QString getCategoryLabel(Category cat) {
    switch (cat) {
        case Category::Perfect: return QStringLiteral("Perfect");
        case Category::High: return QStringLiteral("High");
        case Category::Medium: return QStringLiteral("Medium");
        case Category::Low: return QStringLiteral("Low");
        case Category::Unmatched: return QStringLiteral("Unmatched");
    }
    return QStringLiteral("Unknown");
}

/**
 * @brief Get short category label for badges
 * @param cat Category enum value
 * @return Short label (e.g., "HIGH", "MED")
 */
inline QString getShortLabel(Category cat) {
    switch (cat) {
        case Category::Perfect: return QStringLiteral("PERFECT");
        case Category::High: return QStringLiteral("HIGH");
        case Category::Medium: return QStringLiteral("MED");
        case Category::Low: return QStringLiteral("LOW");
        case Category::Unmatched: return QStringLiteral("NONE");
    }
    return QStringLiteral("?");
}

/**
 * @brief Get match method label for display
 * @param method MatchMethod enum value
 * @return Human-readable label
 */
inline QString getMethodLabel(MatchMethod method) {
    switch (method) {
        case MatchMethod::Hash: return QStringLiteral("Hash Match");
        case MatchMethod::Exact: return QStringLiteral("Exact Match");
        case MatchMethod::Fuzzy: return QStringLiteral("Fuzzy Match");
        case MatchMethod::UserConfirmed: return QStringLiteral("User Confirmed");
        case MatchMethod::Unmatched: return QStringLiteral("No Match");
    }
    return QStringLiteral("Unknown");
}

/**
 * @brief Get confidence value for match method
 * @param method MatchMethod enum value
 * @return Confidence percentage
 */
inline float getConfidenceForMethod(MatchMethod method) {
    switch (method) {
        case MatchMethod::Hash: return Thresholds::HASH_MATCH;
        case MatchMethod::Exact: return Thresholds::EXACT_NAME;
        case MatchMethod::Fuzzy: return Thresholds::FUZZY_MAX;  // Use max fuzzy
        case MatchMethod::UserConfirmed: return Thresholds::USER_CONFIRMED;
        case MatchMethod::Unmatched: return 0.0f;
    }
    return 0.0f;
}

/**
 * @brief Check if confidence meets minimum threshold
 * @param confidence Confidence value (0-100)
 * @param threshold Minimum threshold
 * @return True if confidence >= threshold
 */
inline bool meetsThreshold(float confidence, float threshold = Thresholds::MEDIUM) {
    return confidence >= threshold;
}

/**
 * @brief Check if confidence is considered reliable
 * Reliable means >= MEDIUM threshold (60%)
 * @param confidence Confidence value (0-100)
 * @return True if reliable
 */
inline bool isReliable(float confidence) {
    return confidence >= Thresholds::MEDIUM;
}

/**
 * @brief Check if confidence is high quality
 * High quality means >= HIGH threshold (90%)
 * @param confidence Confidence value (0-100)
 * @return True if high quality
 */
inline bool isHighQuality(float confidence) {
    return confidence >= Thresholds::HIGH;
}

/**
 * @brief Check if confidence is perfect
 * Perfect means exactly 100% (hash or user confirmed)
 * @param confidence Confidence value (0-100)
 * @return True if perfect
 */
inline bool isPerfect(float confidence) {
    return confidence >= Thresholds::PERFECT;
}

} // Confidence
} // Constants
} // Remus
