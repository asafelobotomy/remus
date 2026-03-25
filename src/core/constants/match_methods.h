#pragma once

#include <QString>
#include <QStringList>

namespace Remus {
namespace Constants {

/**
 * @brief Match method constants for ROM identification
 * 
 * Provides unified constants for match method types used throughout
 * the metadata matching pipeline:
 * - Database storage (match_type column)
 * - UI display (match badges, filters)
 * - Provider orchestration (tracking match source)
 * - Confidence scoring (method affects confidence)
 */
class MatchMethods {
public:
    // ========================================================================
    // Method Identifiers (database storage)
    // ========================================================================
    
    /// Hash-based match (highest confidence)
    static constexpr const char* HASH = "hash";
    
    /// Name-based match (medium-high confidence)
    static constexpr const char* NAME = "name";

    /// Legacy exact-name alias still present in some persisted rows
    static constexpr const char* EXACT = "exact";

    /// Legacy exact-name alias emitted by older matching paths
    static constexpr const char* EXACT_NAME = "exact_name";
    
    /// Fuzzy/similarity match (medium-low confidence)
    static constexpr const char* FUZZY = "fuzzy";

    /// Legacy fuzzy alias emitted by older matching paths
    static constexpr const char* FUZZY_NAME = "fuzzy_name";

    /// Legacy exact-name alias emitted by older matching paths
    static constexpr const char* NAME_EXACT = "name-exact";

    /// Legacy fuzzy alias emitted by older matching paths
    static constexpr const char* NAME_FUZZY = "name-fuzzy";
    
    /// User manually assigned (perfect confidence)
    static constexpr const char* MANUAL = "manual";

    /// User-confirmed alias used by some UI flows
    static constexpr const char* USER_CONFIRMED = "user_confirmed";

    /// Placeholder used before a hash match is fully resolved
    static constexpr const char* HASH_PENDING = "hash_pending";
    
    /// No match found yet
    static constexpr const char* NONE = "none";
    
    // ========================================================================
    // Display Names (UI)
    // ========================================================================
    
    /// Hash match display name
    static constexpr const char* HASH_DISPLAY = "Hash Match";
    
    /// Name match display name
    static constexpr const char* NAME_DISPLAY = "Name Match";
    
    /// Fuzzy match display name
    static constexpr const char* FUZZY_DISPLAY = "Fuzzy Match";
    
    /// Manual match display name
    static constexpr const char* MANUAL_DISPLAY = "Manual";
    
    /// No match display name
    static constexpr const char* NONE_DISPLAY = "Not Matched";
    
    // ========================================================================
    // Short Display Names (badges, compact UI)
    // ========================================================================
    
    /// Hash match short name
    static constexpr const char* HASH_SHORT = "Hash";
    
    /// Name match short name
    static constexpr const char* NAME_SHORT = "Name";
    
    /// Fuzzy match short name
    static constexpr const char* FUZZY_SHORT = "Fuzzy";
    
    /// Manual match short name
    static constexpr const char* MANUAL_SHORT = "Manual";
    
    /// No match short name
    static constexpr const char* NONE_SHORT = "None";
    
    // ========================================================================
    // Utility Methods
    // ========================================================================
    
    /**
     * @brief Get display name from method identifier
     * @param method Method identifier ("hash", "name", "fuzzy", "manual", "none")
     * @return Full display name for UI
     * 
     * Example:
     *   displayName("hash") → "Hash Match"
     *   displayName("name") → "Name Match"
     */
    static QString displayName(const QString &method) {
        const QString normalized = canonicalize(method);
        if (normalized == HASH) return HASH_DISPLAY;
        if (normalized == NAME) return NAME_DISPLAY;
        if (normalized == FUZZY) return FUZZY_DISPLAY;
        if (normalized == MANUAL) return MANUAL_DISPLAY;
        return NONE_DISPLAY;
    }
    
    /**
     * @brief Get short display name from method identifier
     * @param method Method identifier
     * @return Short display name for compact UI elements
     * 
     * Example:
     *   shortName("hash") → "Hash"
     *   shortName("fuzzy") → "Fuzzy"
     */
    static QString shortName(const QString &method) {
        const QString normalized = canonicalize(method);
        if (normalized == HASH) return HASH_SHORT;
        if (normalized == NAME) return NAME_SHORT;
        if (normalized == FUZZY) return FUZZY_SHORT;
        if (normalized == MANUAL) return MANUAL_SHORT;
        return NONE_SHORT;
    }

    /**
     * @brief Normalize legacy or variant method identifiers to shared values.
     */
    static QString canonicalize(const QString &method) {
        const QString normalized = method.trimmed().toLower();

        if (normalized == HASH || normalized == HASH_PENDING) return QString::fromLatin1(HASH);
        if (normalized == NAME || normalized == EXACT || normalized == EXACT_NAME || normalized == NAME_EXACT) {
            return QString::fromLatin1(NAME);
        }
        if (normalized == FUZZY || normalized == FUZZY_NAME || normalized == NAME_FUZZY) {
            return QString::fromLatin1(FUZZY);
        }
        if (normalized == MANUAL || normalized == USER_CONFIRMED) return QString::fromLatin1(MANUAL);

        return normalized;
    }

    static bool isHashBased(const QString &method) {
        return canonicalize(method) == HASH;
    }

    static bool isNameBased(const QString &method) {
        const QString normalized = canonicalize(method);
        return normalized == NAME || normalized == FUZZY;
    }
    
    /**
     * @brief Check if method identifier is valid
     * @param method String to check
     * @return True if valid method identifier
     */
    static bool isValid(const QString &method) {
         const QString normalized = canonicalize(method);
         return normalized == HASH || normalized == NAME || 
             normalized == FUZZY || normalized == MANUAL || normalized == NONE;
    }
    
    /**
     * @brief Get all valid match methods (excluding NONE)
     * @return List of method identifiers
     */
    static QStringList allMethods() {
        return {HASH, NAME, FUZZY, MANUAL};
    }
    
    /**
     * @brief Get typical confidence for match method
     * @param method Method identifier
     * @return Typical confidence percentage (0-100)
     * 
     * Note: Actual confidence may vary based on fuzzy match similarity.
     * These are typical values:
     * - HASH/MANUAL: 100 (perfect)
     * - NAME: 90 (high)
     * - FUZZY: 70 (medium, varies by similarity)
     * - NONE: 0 (no match)
     */
    static int typicalConfidence(const QString &method) {
        const QString normalized = canonicalize(method);
        if (normalized == HASH || normalized == MANUAL) return 100;
        if (normalized == NAME) return 90;
        if (normalized == FUZZY) return 70;
        return 0;
    }
    
    /**
     * @brief Get description for match method
     * @param method Method identifier
     * @return Human-readable description
     */
    static QString description(const QString &method) {
        const QString normalized = canonicalize(method);
        if (normalized == HASH) {
            return "Matched by file hash against metadata database";
        }
        if (normalized == NAME) {
            return "Matched by exact filename against metadata database";
        }
        if (normalized == FUZZY) {
            return "Matched by similar filename using fuzzy search";
        }
        if (normalized == MANUAL) {
            return "Manually assigned by user";
        }
        return "No metadata match found";
    }
};

} // namespace Constants
} // namespace Remus
