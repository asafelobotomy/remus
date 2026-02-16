#pragma once

/**
 * @file errors.h
 * @brief Error messages and exception strings
 * 
 * Centralizes all error messages to ensure consistency and enable
 * easy localization/internationalization in the future.
 */

#include <QString>

namespace Remus {
namespace Constants {
namespace Errors {

// ============================================================================
// Database Errors
// ============================================================================

namespace Database {
    inline const QString FAILED_TO_OPEN = QStringLiteral("Failed to open database");
    inline const QString FAILED_TO_CREATE_SCHEMA = QStringLiteral("Failed to create database schema");
    inline const QString FAILED_TO_POPULATE_SYSTEMS = QStringLiteral("Failed to populate default systems");
    inline const QString FAILED_TO_CREATE_TABLE = QStringLiteral("Failed to create table");
    inline const QString FAILED_TO_INSERT_RECORD = QStringLiteral("Failed to insert record into database");
    inline const QString FAILED_TO_UPDATE_RECORD = QStringLiteral("Failed to update record in database");
    inline const QString FAILED_TO_DELETE_RECORD = QStringLiteral("Failed to delete record from database");
    inline const QString FAILED_TO_QUERY = QStringLiteral("Failed to execute database query");
    inline const QString QUERY_TIMEOUT = QStringLiteral("Database query timeout");
    inline const QString INVALID_DATABASE_PATH = QStringLiteral("Invalid database path");
    inline const QString FOREIGN_KEY_VIOLATION = QStringLiteral("Foreign key constraint violation");
    inline const QString UNIQUE_CONSTRAINT_VIOLATION = QStringLiteral("Unique constraint violation");
    inline const QString MIGRATION_FAILED = QStringLiteral("Database migration failed");
}

// ============================================================================
// File System Errors
// ============================================================================

namespace FileSystem {
    inline const QString FILE_NOT_FOUND = QStringLiteral("File not found");
    inline const QString FILE_NOT_READABLE = QStringLiteral("File is not readable");
    inline const QString FILE_NOT_WRITABLE = QStringLiteral("File is not writable");
    inline const QString DIRECTORY_NOT_FOUND = QStringLiteral("Directory not found");
    inline const QString DIRECTORY_NOT_ACCESSIBLE = QStringLiteral("Directory is not accessible");
    inline const QString PERMISSION_DENIED = QStringLiteral("Permission denied");
    inline const QString PATH_TOO_LONG = QStringLiteral("Path is too long");
    inline const QString FILENAME_INVALID = QStringLiteral("Filename contains invalid characters");
    inline const QString INSUFFICIENT_DISK_SPACE = QStringLiteral("Insufficient disk space");
    inline const QString FAILED_TO_CREATE_DIRECTORY = QStringLiteral("Failed to create directory");
    inline const QString FAILED_TO_DELETE_FILE = QStringLiteral("Failed to delete file");
    inline const QString FAILED_TO_MOVE_FILE = QStringLiteral("Failed to move file");
    inline const QString FAILED_TO_COPY_FILE = QStringLiteral("Failed to copy file");
    inline const QString FAILED_TO_CREATE_SYMLINK = QStringLiteral("Failed to create symbolic link");
}

// ============================================================================
// Hashing Errors
// ============================================================================

namespace Hashing {
    inline const QString HASH_CALCULATION_FAILED = QStringLiteral("Hash calculation failed");
    inline const QString UNSUPPORTED_HASH_ALGORITHM = QStringLiteral("Unsupported hash algorithm");
    inline const QString HASH_MISMATCH = QStringLiteral("Hash mismatch");
    inline const QString FILE_CHANGED_DURING_HASH = QStringLiteral("File changed during hash calculation");
}

// ============================================================================
// Metadata Provider Errors
// ============================================================================

namespace MetadataProvider {
    inline const QString PROVIDER_NOT_AVAILABLE = QStringLiteral("Metadata provider is not available");
    inline const QString PROVIDER_NOT_FOUND = QStringLiteral("Provider not found");
    inline const QString AUTHENTICATION_FAILED = QStringLiteral("Authentication with provider failed");
    inline const QString API_KEY_INVALID = QStringLiteral("Invalid API key for provider");
    inline const QString CREDENTIALS_MISSING = QStringLiteral("Provider credentials are missing or incomplete");
    inline const QString NETWORK_ERROR = QStringLiteral("Network error communicating with provider");
    inline const QString REQUEST_TIMEOUT = QStringLiteral("Request to provider timed out");
    inline const QString RATE_LIMIT_EXCEEDED = QStringLiteral("Rate limit exceeded for provider");
    inline const QString INVALID_RESPONSE = QStringLiteral("Invalid response from provider");
    inline const QString NO_RESULTS = QStringLiteral("No results found from provider");
    inline const QString PARSE_ERROR = QStringLiteral("Failed to parse provider response");
    inline const QString SERVICE_UNAVAILABLE = QStringLiteral("Provider service is temporarily unavailable");
}

// ============================================================================
// Matching Errors
// ============================================================================

namespace Matching {
    inline const QString NO_MATCH_FOUND = QStringLiteral("No matching game found");
    inline const QString SYSTEM_NOT_SUPPORTED = QStringLiteral("System is not supported");
    inline const QString INVALID_SYSTEM_ID = QStringLiteral("Invalid or unknown system ID");
    inline const QString GAME_NOT_FOUND_IN_DATABASE = QStringLiteral("Game not found in database");
    inline const QString AMBIGUOUS_MATCH = QStringLiteral("Multiple matches found, manual selection required");
}

// ============================================================================
// Organize Engine Errors
// ============================================================================

namespace Organize {
    inline const QString ORGANIZE_NOT_STARTED = QStringLiteral("Organization process not started");
    inline const QString ORGANIZE_FAILED = QStringLiteral("Organization process failed");
    inline const QString COLLISION_DETECTED = QStringLiteral("File collision detected at destination");
    inline const QString UNDO_NOT_AVAILABLE = QStringLiteral("Undo operation is not available");
    inline const QString UNDO_FAILED = QStringLiteral("Failed to undo organization operation");
    inline const QString DRY_RUN_ONLY = QStringLiteral("This operation is only available in dry-run mode for preview");
}

// ============================================================================
// Verify Engine Errors
// ============================================================================

namespace Verify {
    inline const QString DAT_FILE_NOT_FOUND = QStringLiteral("DAT file not found");
    inline const QString DAT_PARSE_ERROR = QStringLiteral("Failed to parse DAT file");
    inline const QString VERIFICATION_FAILED = QStringLiteral("Verification process failed");
    inline const QString NO_VERIFICATION_DATA = QStringLiteral("No verification data available for this file");
    inline const QString HEADER_CORRUPTED = QStringLiteral("File header is corrupted");
}

// ============================================================================
// Patch Engine Errors
// ============================================================================

namespace Patch {
    inline const QString PATCH_FILE_NOT_FOUND = QStringLiteral("Patch file not found");
    inline const QString PATCH_FORMAT_UNSUPPORTED = QStringLiteral("Unsupported patch format");
    inline const QString PATCH_PARSE_ERROR = QStringLiteral("Failed to parse patch file");
    inline const QString PATCH_APPLICATION_FAILED = QStringLiteral("Failed to apply patch");
    inline const QString PATCH_INCOMPATIBLE = QStringLiteral("Patch is incompatible with source file");
    inline const QString SOURCE_FILE_NOT_FOUND = QStringLiteral("Source file for patching not found");
    inline const QString BACKUP_CREATION_FAILED = QStringLiteral("Failed to create backup of original file");
}

// ============================================================================
// Archive Errors
// ============================================================================

namespace Archive {
    inline const QString ARCHIVE_NOT_FOUND = QStringLiteral("Archive file not found");
    inline const QString ARCHIVE_CORRUPTED = QStringLiteral("Archive file is corrupted");
    inline const QString ARCHIVE_FORMAT_UNSUPPORTED = QStringLiteral("Archive format is not supported");
    inline const QString EXTRACTION_FAILED = QStringLiteral("Failed to extract archive");
    inline const QString ARCHIVE_TOO_LARGE = QStringLiteral("Archive is too large to extract");
    inline const QString EXTRACTION_TIMEOUT = QStringLiteral("Archive extraction timed out");
}

// ============================================================================
// CHD Errors
// ============================================================================

namespace CHD {
    inline const QString CONVERSION_FAILED = QStringLiteral("CHD conversion failed");
    inline const QString UNSUPPORTED_VERSION = QStringLiteral("Unsupported CHD version");
    inline const QString COMPRESSION_UNSUPPORTED = QStringLiteral("Unsupported CHD compression method");
    inline const QString INVALID_CHD_FILE = QStringLiteral("Invalid or corrupted CHD file");
    inline const QString CHDMAN_NOT_FOUND = QStringLiteral("CHDMAN tool not found");
}

// ============================================================================
// Configuration Errors
// ============================================================================

namespace Configuration {
    inline const QString SETTINGS_NOT_SAVED = QStringLiteral("Failed to save settings");
    inline const QString SETTINGS_NOT_LOADED = QStringLiteral("Failed to load settings");
    inline const QString INVALID_SETTING_VALUE = QStringLiteral("Invalid setting value");
    inline const QString NAMING_TEMPLATE_INVALID = QStringLiteral("Invalid naming template");
}

// ============================================================================
// Generic/Critical Errors
// ============================================================================

namespace Generic {
    inline const QString OPERATION_CANCELLED = QStringLiteral("Operation was cancelled");
    inline const QString OPERATION_FAILED = QStringLiteral("Operation failed");
    inline const QString UNKNOWN_ERROR = QStringLiteral("An unknown error occurred");
    inline const QString NOT_IMPLEMENTED = QStringLiteral("This feature is not yet implemented");
    inline const QString INVALID_ARGUMENT = QStringLiteral("Invalid argument provided");
    inline const QString OUT_OF_MEMORY = QStringLiteral("Out of memory");
}

}  // namespace Errors
}  // namespace Constants
}  // namespace Remus
