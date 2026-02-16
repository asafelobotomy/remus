#pragma once

/**
 * @file engines.h
 * @brief Processing engines, status values, and pipeline constants
 * 
 * Centralizes constants for all processing engines (organize, verify, patch),
 * including status values, operation types, and validation constants.
 */

#include <QString>

namespace Remus {
namespace Constants {
namespace Engines {

// ============================================================================
// Processing Status Values
// ============================================================================

namespace ProcessingStatus {
    /// File has not been processed
    inline const QString UNPROCESSED = QStringLiteral("unprocessed");
    
    /// File processing is in progress
    inline const QString IN_PROGRESS = QStringLiteral("in_progress");
    
    /// File has been successfully processed
    inline const QString PROCESSED = QStringLiteral("processed");
    
    /// File processing failed
    inline const QString FAILED = QStringLiteral("failed");
    
    /// File skipped during processing (user choice or incompatible)
    inline const QString SKIPPED = QStringLiteral("skipped");
    
    /// File processing threw an error (recoverable)
    inline const QString ERROR = QStringLiteral("error");
}

// ============================================================================
// Organize Engine
// ============================================================================

namespace Organize {
    /// Operation type: Move file to organized location
    inline const QString OP_MOVE = QStringLiteral("move");
    
    /// Operation type: Copy file to organized location
    inline const QString OP_COPY = QStringLiteral("copy");
    
    /// Operation type: Create symbolic link
    inline const QString OP_SYMLINK = QStringLiteral("symlink");
    
    /// Collision handling: Skip duplicate file
    inline const QString COLLISION_SKIP = QStringLiteral("skip");
    
    /// Collision handling: Rename existing with suffix
    inline const QString COLLISION_RENAME = QStringLiteral("rename");
    
    /// Collision handling: Overwrite with new version
    inline const QString COLLISION_OVERWRITE = QStringLiteral("overwrite");
    
    /// Default collision handling mode
    inline const QString DEFAULT_COLLISION_MODE = COLLISION_RENAME;
}

// ============================================================================
// Verify Engine
// ============================================================================

namespace Verify {
    /// Verification status: Hash match found in database
    inline const QString VERIFIED = QStringLiteral("verified");
    
    /// Verification status: File marked as good in DAT
    inline const QString GOOD = QStringLiteral("good");
    
    /// Verification status: File is a known good dump
    inline const QString KNOWN_GOOD = QStringLiteral("known_good");
    
    /// Verification status: File hash does not match any known good dump
    inline const QString UNVERIFIED = QStringLiteral("unverified");
    
    /// Verification status: File appears to be corrupted or modified
    inline const QString CORRUPTED = QStringLiteral("corrupted");
    
    /// Verification status: File header doesn't match system signature
    inline const QString BAD_HEADER = QStringLiteral("bad_header");
    
    /// Verification status: File is a known bad dump
    inline const QString BAD = QStringLiteral("bad");
    
    /// Default verification check level (0=disabled, 1=hash only, 2=header+hash)
    inline constexpr int DEFAULT_CHECK_LEVEL = 1;
}

// ============================================================================
// Patch Engine
// ============================================================================

namespace Patch {
    /// Patch type: IPS (International Patching System)
    inline const QString TYPE_IPS = QStringLiteral("ips");
    
    /// Patch type: BPS (Binary Patching System)
    inline const QString TYPE_BPS = QStringLiteral("bps");
    
    /// Patch type: UPS (Universal Patching System)
    inline const QString TYPE_UPS = QStringLiteral("ups");
    
    /// Patch type: XDELTA
    inline const QString TYPE_XDELTA = QStringLiteral("xdelta");
    
    /// Patch status: Ready to apply
    inline const QString STATUS_READY = QStringLiteral("ready");
    
    /// Patch status: Applied successfully
    inline const QString STATUS_APPLIED = QStringLiteral("applied");
    
    /// Patch status: Application failed (incompatible ROM)
    inline const QString STATUS_FAILED = QStringLiteral("failed");
    
    /// Patch status: Source file not found
    inline const QString STATUS_MISSING_SOURCE = QStringLiteral("missing_source");
    
    /// Maximum supported patch file size (100 MB)
    inline constexpr int MAX_PATCH_SIZE_BYTES = 104857600;
    
    /// Backup directory for original files before patching
    inline constexpr const char* BACKUP_SUFFIX = ".backup";
}

// ============================================================================
// Hashing Engine
// ============================================================================

namespace Hashing {
    /// Default parallelization level (number of threads for parallel hashing)
    inline constexpr int DEFAULT_PARALLEL_THREADS = 4;
    
    /// Maximum number of parallel hashing threads
    inline constexpr int MAX_PARALLEL_THREADS = 8;
    
    /// Hash chunk size for streaming (1 MB)
    inline constexpr int HASH_CHUNK_SIZE = 1048576;
    
    /// Skip hashing files smaller than this (bytes)
    inline constexpr int MIN_HASH_FILE_SIZE = 0;
}

// ============================================================================
// Scanning Engine
// ============================================================================

namespace Scanning {
    /// Maximum file name length to scan (characters)
    inline constexpr int MAX_FILENAME_LENGTH = 255;
    
    /// Skip hidden files (files beginning with dot)
    inline constexpr bool SKIP_HIDDEN_FILES = true;
    
    /// Skip system/cache directories
    inline constexpr bool SKIP_SYSTEM_DIRS = true;
    
    /// Default scan follow symlinks behavior
    inline constexpr bool FOLLOW_SYMLINKS = false;
}

// ============================================================================
// Archive Extraction
// ============================================================================

namespace Archive {
    /// Archive type: ZIP
    inline const QString TYPE_ZIP = QStringLiteral("zip");
    
    /// Archive type: 7-Zip
    inline const QString TYPE_7Z = QStringLiteral("7z");
    
    /// Archive type: RAR
    inline const QString TYPE_RAR = QStringLiteral("rar");
    
    /// Archive type: TAR (and compressed variants)
    inline const QString TYPE_TAR = QStringLiteral("tar");
    
    /// Maximum archive extraction size (500 MB)
    inline constexpr int MAX_EXTRACT_SIZE_BYTES = 524288000;
    
    /// Maximum archive file count before warning
    inline constexpr int LARGE_ARCHIVE_FILE_COUNT = 1000;
}

// ============================================================================
// CHD Conversion
// ============================================================================

namespace CHD {
    /// CHD version 3
    inline constexpr int VERSION_3 = 3;
    
    /// CHD version 4
    inline constexpr int VERSION_4 = 4;
    
    /// CHD version 5 (latest)
    inline constexpr int VERSION_5 = 5;
    
    /// Default CHD compression (ZSTD)
    inline const QString COMPRESSION_ZSTD = QStringLiteral("zstd");
    
    /// Legacy CHD compression (FLAC)
    inline const QString COMPRESSION_FLAC = QStringLiteral("flac");
    
    /// CHD compression: No compression
    inline const QString COMPRESSION_NONE = QStringLiteral("none");
}

// ============================================================================
// Validation & Constraints
// ============================================================================

/// Minimum file size for validation (bytes)
inline constexpr int MIN_VALID_FILE_SIZE = 512;

/// Maximum file size for standard processing (8 GB)
inline constexpr long long MAX_PROCESSABLE_FILE_SIZE = 8589934592LL;

/// Timeout for individual file processing operations (milliseconds)
inline constexpr int FILE_OPERATION_TIMEOUT_MS = 120000;  // 2 minutes

}  // namespace Engines
}  // namespace Constants
}  // namespace Remus
