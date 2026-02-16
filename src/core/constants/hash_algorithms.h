#pragma once

#include <QString>

namespace Remus {
namespace Constants {

/**
 * @brief Hash algorithm constants and utilities
 * 
 * Provides unified constants for hash algorithm names, lengths, and validation.
 * Use these instead of hardcoded strings to ensure consistency across:
 * - Metadata provider APIs (lowercase: "md5", "sha1", "crc32")
 * - Database storage (uppercase: "MD5", "SHA1", "CRC32")
 * - UI display (uppercase with formatting)
 */
class HashAlgorithms {
public:
    // ========================================================================
    // Algorithm Identifiers (lowercase for APIs)
    // ========================================================================
    
    /// CRC32 algorithm identifier for APIs (8 character hex)
    static constexpr const char* CRC32 = "crc32";
    
    /// MD5 algorithm identifier for APIs (32 character hex)
    static constexpr const char* MD5 = "md5";
    
    /// SHA1 algorithm identifier for APIs (40 character hex)
    static constexpr const char* SHA1 = "sha1";
    
    // ========================================================================
    // Display Names (uppercase for UI/Database)
    // ========================================================================
    
    /// CRC32 display name for UI and database storage
    static constexpr const char* CRC32_DISPLAY = "CRC32";
    
    /// MD5 display name for UI and database storage
    static constexpr const char* MD5_DISPLAY = "MD5";
    
    /// SHA1 display name for UI and database storage
    static constexpr const char* SHA1_DISPLAY = "SHA1";
    
    // ========================================================================
    // Hash String Lengths
    // ========================================================================
    
    /// CRC32 hash string length (8 hex characters)
    static constexpr int CRC32_LENGTH = 8;
    
    /// MD5 hash string length (32 hex characters)
    static constexpr int MD5_LENGTH = 32;
    
    /// SHA1 hash string length (40 hex characters)
    static constexpr int SHA1_LENGTH = 40;
    
    // ========================================================================
    // Utility Methods
    // ========================================================================
    
    /**
     * @brief Detect hash algorithm from hash string length
     * @param hashLength Length of hash string (8, 32, or 40)
     * @return Algorithm identifier (lowercase) or empty string if unknown
     * 
     * Example:
     *   detectFromLength(32) → "md5"
     *   detectFromLength(40) → "sha1"
     *   detectFromLength(8) → "crc32"
     */
    static QString detectFromLength(int hashLength) {
        switch (hashLength) {
            case CRC32_LENGTH: return CRC32;
            case MD5_LENGTH: return MD5;
            case SHA1_LENGTH: return SHA1;
            default: return QString();
        }
    }
    
    /**
     * @brief Validate hash string for given algorithm
     * @param hash Hash string to validate
     * @param algorithm Algorithm identifier (lowercase)
     * @return True if hash length matches algorithm
     * 
     * Example:
     *   isValidHash("1bc674be034e43c96b86487ac69d9293", "md5") → true
     *   isValidHash("abc123", "md5") → false (wrong length)
     */
    static bool isValidHash(const QString &hash, const QString &algorithm) {
        if (algorithm == CRC32) return hash.length() == CRC32_LENGTH;
        if (algorithm == MD5) return hash.length() == MD5_LENGTH;
        if (algorithm == SHA1) return hash.length() == SHA1_LENGTH;
        return false;
    }
    
    /**
     * @brief Get display name from algorithm identifier
     * @param algorithm Algorithm identifier (lowercase: "md5", "sha1", "crc32")
     * @return Display name (uppercase: "MD5", "SHA1", "CRC32")
     * 
     * Example:
     *   displayName("md5") → "MD5"
     *   displayName("sha1") → "SHA1"
     */
    static QString displayName(const QString &algorithm) {
        if (algorithm == CRC32) return CRC32_DISPLAY;
        if (algorithm == MD5) return MD5_DISPLAY;
        if (algorithm == SHA1) return SHA1_DISPLAY;
        return algorithm.toUpper();
    }
    
    /**
     * @brief Convert display name to algorithm identifier
     * @param displayName Display name (uppercase: "MD5", "SHA1", "CRC32")
     * @return Algorithm identifier (lowercase: "md5", "sha1", "crc32")
     * 
     * Example:
     *   toAlgorithmId("MD5") → "md5"
     *   toAlgorithmId("SHA1") → "sha1"
     */
    static QString toAlgorithmId(const QString &displayName) {
        QString upper = displayName.toUpper();
        if (upper == CRC32_DISPLAY) return CRC32;
        if (upper == MD5_DISPLAY) return MD5;
        if (upper == SHA1_DISPLAY) return SHA1;
        return displayName.toLower();
    }
    
    /**
     * @brief Check if string is a valid algorithm identifier
     * @param algorithm String to check
     * @return True if valid algorithm identifier
     */
    static bool isValidAlgorithm(const QString &algorithm) {
        return algorithm == CRC32 || algorithm == MD5 || algorithm == SHA1;
    }
    
    /**
     * @brief Get all valid algorithm identifiers
     * @return List of algorithm identifiers ["crc32", "md5", "sha1"]
     */
    static QStringList allAlgorithms() {
        return {CRC32, MD5, SHA1};
    }
};

} // namespace Constants
} // namespace Remus
