#pragma once

/**
 * @file database_schema.h
 * @brief Database schema constants and table/column definitions
 * 
 * Centralizes all database table names, column names, and schema-related
 * constants to avoid hardcoding database structure throughout the codebase.
 */

#include <QString>

namespace Remus {
namespace Constants {
namespace DatabaseSchema {

// ============================================================================
// Database Files & Locations
// ============================================================================

/// Default database filename
inline constexpr const char* DATABASE_FILENAME = "remus.db";

/// Database connection name for QSqlDatabase
inline constexpr const char* CONNECTION_NAME = "RemusDB";

/// SQLite driver name
inline constexpr const char* DRIVER_NAME = "QSQLITE";

// ============================================================================
// Systems Table
// ============================================================================

namespace Tables {
    /// Systems table name (gaming systems registry)
    inline constexpr const char* SYSTEMS = "systems";
    
    /// Libraries table name (user's ROM directories)
    inline constexpr const char* LIBRARIES = "libraries";
    
    /// Files table name (scanned ROM files)
    inline constexpr const char* FILES = "files";
    
    /// Games table name (matched game metadata)
    inline constexpr const char* GAMES = "games";
    
    /// Matches table name (file-to-game mappings)
    inline constexpr const char* MATCHES = "matches";
    
    /// Cache table name (metadata cache)
    inline constexpr const char* CACHE = "cache";
    
    /// Undo table name (organize operation history)
    inline constexpr const char* UNDO_HISTORY = "undo_history";
}

namespace Columns {
    // Shared columns
    inline constexpr const char* ID = "id";
    inline constexpr const char* CREATED_AT = "created_at";
    inline constexpr const char* UPDATED_AT = "updated_at";
    
    // Systems columns
    namespace Systems {
        inline constexpr const char* NAME = "name";
        inline constexpr const char* DISPLAY_NAME = "display_name";
        inline constexpr const char* MANUFACTURER = "manufacturer";
        inline constexpr const char* GENERATION = "generation";
        inline constexpr const char* EXTENSIONS = "extensions";
        inline constexpr const char* PREFERRED_HASH = "preferred_hash";
    }
    
    // Libraries columns
    namespace Libraries {
        inline constexpr const char* PATH = "path";
        inline constexpr const char* NAME = "name";
        inline constexpr const char* ENABLED = "enabled";
        inline constexpr const char* LAST_SCANNED = "last_scanned";
    }
    
    // Files columns
    namespace Files {
        inline constexpr const char* LIBRARY_ID = "library_id";
        inline constexpr const char* ORIGINAL_PATH = "original_path";
        inline constexpr const char* CURRENT_PATH = "current_path";
        inline constexpr const char* FILENAME = "filename";
        inline constexpr const char* EXTENSION = "extension";
        inline constexpr const char* FILE_SIZE = "file_size";
        inline constexpr const char* IS_COMPRESSED = "is_compressed";
        inline constexpr const char* ARCHIVE_PATH = "archive_path";
        inline constexpr const char* ARCHIVE_INTERNAL_PATH = "archive_internal_path";
        inline constexpr const char* SYSTEM_ID = "system_id";
        inline constexpr const char* CRC32 = "crc32";
        inline constexpr const char* MD5 = "md5";
        inline constexpr const char* SHA1 = "sha1";
        inline constexpr const char* HASH_CALCULATED = "hash_calculated";
        inline constexpr const char* IS_PRIMARY = "is_primary";
        inline constexpr const char* PARENT_FILE_ID = "parent_file_id";
        inline constexpr const char* IS_PROCESSED = "is_processed";
        inline constexpr const char* PROCESSING_STATUS = "processing_status";
        inline constexpr const char* LAST_MODIFIED = "last_modified";
        inline constexpr const char* SCANNED_AT = "scanned_at";
    }
    
    // Games columns
    namespace Games {
        inline constexpr const char* PROVIDER_ID = "provider_id";
        inline constexpr const char* PROVIDER_GAME_ID = "provider_game_id";
        inline constexpr const char* TITLE = "title";
        inline constexpr const char* DESCRIPTION = "description";
        inline constexpr const char* RELEASE_DATE = "release_date";
        inline constexpr const char* DEVELOPER = "developer";
        inline constexpr const char* PUBLISHER = "publisher";
        inline constexpr const char* GENRE = "genre";
        inline constexpr const char* RATING = "rating";
        inline constexpr const char* PLAYERS = "players";
    }
    
    // Matches columns
    namespace Matches {
        inline constexpr const char* FILE_ID = "file_id";
        inline constexpr const char* GAME_ID = "game_id";
        inline constexpr const char* MATCH_METHOD = "match_method";
        inline constexpr const char* CONFIDENCE = "confidence";
        inline constexpr const char* PROVIDER = "provider";
        inline constexpr const char* NAME_MATCH_SCORE = "name_match_score";
    }
    
    // Cache columns
    namespace Cache {
        inline constexpr const char* CACHE_KEY = "cache_key";
        inline constexpr const char* CACHE_VALUE = "cache_value";
        inline constexpr const char* EXPIRY = "expiry";
    }
    
    // Undo columns
    namespace UndoHistory {
        inline constexpr const char* OPERATION_TYPE = "operation_type";
        inline constexpr const char* SOURCE_PATH = "source_path";
        inline constexpr const char* DEST_PATH = "dest_path";
        inline constexpr const char* FILE_ID = "file_id";
        inline constexpr const char* UNDO_DATA = "undo_data";
        inline constexpr const char* TIMESTAMP = "timestamp";
    }
}

// ============================================================================
// Schema Pragmas & Configuration
// ============================================================================

/// Enable foreign key constraints
inline constexpr const char* PRAGMA_FOREIGN_KEYS = "PRAGMA foreign_keys = ON";

/// Check database integrity
inline constexpr const char* PRAGMA_INTEGRITY_CHECK = "PRAGMA integrity_check";

// ============================================================================
// Index Names
// ============================================================================

namespace Indexes {
    inline constexpr const char* FILES_CURRENT_PATH = "idx_files_current_path";
    inline constexpr const char* FILES_SYSTEM_ID = "idx_files_system_id";
    inline constexpr const char* FILES_HASHES = "idx_files_hashes";
    inline constexpr const char* FILES_ORIGINAL_PATH = "idx_files_original_path";
    inline constexpr const char* FILES_PROCESSED = "idx_files_processed";
    inline constexpr const char* MATCHES_FILE_ID = "idx_matches_file_id";
    inline constexpr const char* MATCHES_GAME_ID = "idx_matches_game_id";
    inline constexpr const char* CACHE_KEY = "idx_cache_key";
}

// ============================================================================
// Default Values & Constraints
// ============================================================================

/// Maximum filename length
inline constexpr int MAX_FILENAME_LENGTH = 255;

/// Maximum path length
inline constexpr int MAX_PATH_LENGTH = 4096;

/// Cache default TTL (24 hours in seconds)
inline constexpr int DEFAULT_CACHE_TTL_SECONDS = 86400;

/// Database query timeout (milliseconds)
inline constexpr int QUERY_TIMEOUT_MS = 30000;

}  // namespace DatabaseSchema
}  // namespace Constants
}  // namespace Remus
