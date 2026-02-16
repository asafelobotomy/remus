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
    
    /// Fuzzy/similarity match (medium-low confidence)
    static constexpr const char* FUZZY = "fuzzy";
    
    /// User manually assigned (perfect confidence)
    static constexpr const char* MANUAL = "manual";
    
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
        if (method == HASH) return HASH_DISPLAY;
        if (method == NAME) return NAME_DISPLAY;
        if (method == FUZZY) return FUZZY_DISPLAY;
        if (method == MANUAL) return MANUAL_DISPLAY;
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
        if (method == HASH) return HASH_SHORT;
        if (method == NAME) return NAME_SHORT;
        if (method == FUZZY) return FUZZY_SHORT;
        if (method == MANUAL) return MANUAL_SHORT;
        return NONE_SHORT;
    }
    
    /**
     * @brief Check if method identifier is valid
     * @param method String to check
     * @return True if valid method identifier
     */
    static bool isValid(const QString &method) {
        return method == HASH || method == NAME || 
               method == FUZZY || method == MANUAL || method == NONE;
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
        if (method == HASH || method == MANUAL) return 100;
        if (method == NAME) return 90;
        if (method == FUZZY) return 70;
        return 0;
    }
    
    /**
     * @brief Get description for match method
     * @param method Method identifier
     * @return Human-readable description
     */
    static QString description(const QString &method) {
        if (method == HASH) {
            return "Matched by file hash against metadata database";
        }
        if (method == NAME) {
            return "Matched by exact filename against metadata database";
        }
        if (method == FUZZY) {
            return "Matched by similar filename using fuzzy search";
        }
        if (method == MANUAL) {
            return "Manually assigned by user";
        }
        return "No metadata match found";
    }
};

} // namespace Constants
} // namespace Remus
