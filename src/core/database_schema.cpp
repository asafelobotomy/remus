#include "database.h"
#include "system_detector.h"
#include "constants/constants.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace Remus {

bool Database::createSchema() {
    QSqlQuery query(m_db);

    // Enable foreign keys
    if (!query.exec(Constants::DatabaseSchema::PRAGMA_FOREIGN_KEYS)) {
        logError(Constants::Errors::Database::FAILED_TO_CREATE_SCHEMA);
        return false;
    }

    // Create systems table
    QString createSystems = R"(
        CREATE TABLE IF NOT EXISTS systems (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            display_name TEXT NOT NULL,
            manufacturer TEXT,
            generation INTEGER,
            extensions TEXT NOT NULL,
            preferred_hash TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (!query.exec(createSystems)) {
        logError("Failed to create systems table: " + query.lastError().text());
        return false;
    }

    // Create libraries table
    QString createLibraries = R"(
        CREATE TABLE IF NOT EXISTS libraries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT NOT NULL UNIQUE,
            name TEXT,
            enabled BOOLEAN DEFAULT 1,
            last_scanned TIMESTAMP,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (!query.exec(createLibraries)) {
        logError("Failed to create libraries table: " + query.lastError().text());
        return false;
    }

    // Create files table
    QString createFiles = R"(
        CREATE TABLE IF NOT EXISTS files (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            library_id INTEGER NOT NULL,
            original_path TEXT NOT NULL,
            current_path TEXT NOT NULL,
            filename TEXT NOT NULL,
            extension TEXT NOT NULL,
            file_size INTEGER NOT NULL,
            is_compressed BOOLEAN DEFAULT 0,
            archive_path TEXT,
            archive_internal_path TEXT,
            system_id INTEGER,
            crc32 TEXT,
            md5 TEXT,
            sha1 TEXT,
            ra_md5 TEXT,
            hash_calculated BOOLEAN DEFAULT 0,
            is_primary BOOLEAN DEFAULT 1,
            parent_file_id INTEGER,
            base_title TEXT,
            disc_set_key TEXT,
            disc_number INTEGER DEFAULT 0,
            file_type TEXT DEFAULT 'official',
            is_patched BOOLEAN DEFAULT 0,
            patch_name TEXT,
            is_processed BOOLEAN DEFAULT 0,
            processing_status TEXT DEFAULT 'unprocessed',
            is_converted BOOLEAN DEFAULT 0,
            is_bundled BOOLEAN DEFAULT 0,
            bundle_output_path TEXT,
            has_local_artwork INTEGER DEFAULT 0,
            last_modified TIMESTAMP,
            scanned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (library_id) REFERENCES libraries(id) ON DELETE CASCADE,
            FOREIGN KEY (system_id) REFERENCES systems(id),
            FOREIGN KEY (parent_file_id) REFERENCES files(id) ON DELETE CASCADE
        )
    )";
    if (!query.exec(createFiles)) {
        logError("Failed to create files table: " + query.lastError().text());
        return false;
    }

    // Idempotent column migrations (duplicate column name is expected on re-run).
    const auto execMigration = [&](const char *sql) {
        if (!query.exec(sql)) {
            const QString err = query.lastError().text();
            if (err.contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive)) {
                return;
            }
            qWarning() << "Migration DDL warning:" << sql << "-" << err;
        }
    };
    execMigration("ALTER TABLE files ADD COLUMN is_processed BOOLEAN DEFAULT 0");
    execMigration("ALTER TABLE files ADD COLUMN processing_status TEXT DEFAULT 'unprocessed'");
    execMigration("ALTER TABLE files ADD COLUMN base_title TEXT");
    execMigration("ALTER TABLE files ADD COLUMN file_type TEXT DEFAULT 'official'");
    execMigration("ALTER TABLE files ADD COLUMN is_patched BOOLEAN DEFAULT 0");
    execMigration("ALTER TABLE files ADD COLUMN patch_name TEXT");
    execMigration("ALTER TABLE files ADD COLUMN is_converted BOOLEAN DEFAULT 0");
    execMigration("ALTER TABLE files ADD COLUMN is_bundled BOOLEAN DEFAULT 0");
    execMigration("ALTER TABLE files ADD COLUMN bundle_output_path TEXT");
    execMigration("ALTER TABLE files ADD COLUMN has_local_artwork INTEGER DEFAULT 0");
    execMigration("ALTER TABLE files ADD COLUMN chd_sha1 TEXT");
    execMigration("ALTER TABLE files ADD COLUMN rvz_sha1 TEXT");

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_files_processed ON files(is_processed)")) {
        qWarning() << "Failed to create idx_files_processed:" << query.lastError().text();
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_files_current_path ON files(current_path)")) {
        qWarning() << "Failed to create idx_files_current_path:" << query.lastError().text();
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_files_system_id ON files(system_id)")) {
        qWarning() << "Failed to create idx_files_system_id:" << query.lastError().text();
    }
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_files_hashes ON files(crc32, md5, sha1)")) {
        qWarning() << "Failed to create idx_files_hashes:" << query.lastError().text();
    }
    query.exec("DROP INDEX IF EXISTS idx_files_original_path");
    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_files_original_path "
                    "ON files(original_path, filename, COALESCE(archive_internal_path, ''))")) {
        qWarning() << "Failed to create idx_files_original_path:" << query.lastError().text();
    }

    // Create cache table for metadata
    QString createAppliedPatches = R"(
        CREATE TABLE IF NOT EXISTS applied_patches (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            base_path TEXT NOT NULL,
            output_path TEXT NOT NULL,
            patch_path TEXT NOT NULL,
            patch_format TEXT,
            base_title TEXT,
            patch_name TEXT,
            file_type TEXT DEFAULT 'hack',
            source_checksum TEXT,
            target_checksum TEXT,
            patch_checksum TEXT,
            base_crc32 TEXT,
            base_md5 TEXT,
            base_sha1 TEXT,
            output_crc32 TEXT,
            output_md5 TEXT,
            output_sha1 TEXT,
            applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    if (!query.exec(createAppliedPatches)) {
        logError("Failed to create applied_patches table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_sha1 ON applied_patches(output_sha1)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_md5 ON applied_patches(output_md5)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_crc32 ON applied_patches(output_crc32)");

    // Create cache table for metadata
    QString createCache = R"(
        CREATE TABLE IF NOT EXISTS cache (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            cache_key TEXT NOT NULL UNIQUE,
            cache_value BLOB,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            expiry TIMESTAMP
        )
    )";
    if (!query.exec(createCache)) {
        logError("Failed to create cache table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_cache_key ON cache(cache_key)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_cache_expiry ON cache(expiry)");

    // Create undo_queue table for file operation rollback (M4)
    QString createUndoQueue = R"(
        CREATE TABLE IF NOT EXISTS undo_queue (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            operation_type TEXT NOT NULL,
            old_path TEXT NOT NULL,
            new_path TEXT NOT NULL,
            file_id INTEGER,
            executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            undone BOOLEAN DEFAULT 0,
            undone_at TIMESTAMP,
            FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE SET NULL
        )
    )";
    if (!query.exec(createUndoQueue)) {
        logError("Failed to create undo_queue table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_undo_queue_file_id ON undo_queue(file_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_undo_queue_undone ON undo_queue(undone)");

    // Create games table for metadata
    QString createGames = R"(
        CREATE TABLE IF NOT EXISTS games (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            system_id INTEGER,
            region TEXT,
            publisher TEXT,
            developer TEXT,
            release_date TEXT,
            description TEXT,
            genres TEXT,
            players TEXT,
            rating REAL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (system_id) REFERENCES systems(id)
        )
    )";
    if (!query.exec(createGames)) {
        logError("Failed to create games table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_games_title ON games(title)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_games_system ON games(system_id)");
    // Composite indexes for hot query paths
    query.exec("CREATE INDEX IF NOT EXISTS idx_games_title_system ON games(title, system_id)");

    // Create matches table for file-to-game matching
    QString createMatches = R"(
        CREATE TABLE IF NOT EXISTS matches (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id INTEGER NOT NULL,
            game_id INTEGER NOT NULL,
            match_method TEXT NOT NULL,
            confidence REAL NOT NULL,
            is_confirmed BOOLEAN DEFAULT 0,
            is_rejected BOOLEAN DEFAULT 0,
            matched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
            FOREIGN KEY (game_id) REFERENCES games(id) ON DELETE CASCADE,
            UNIQUE(file_id, game_id)
        )
    )";
    if (!query.exec(createMatches)) {
        logError("Failed to create matches table: " + query.lastError().text());
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_matches_file ON matches(file_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_matches_game ON matches(game_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_matches_confidence ON matches(confidence)");
    // Composite indexes for hot query paths
    query.exec("CREATE INDEX IF NOT EXISTS idx_files_hash_primary ON files(hash_calculated, is_primary)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_files_primary_processed ON files(is_primary, is_processed)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_matches_file_status ON matches(file_id, is_confirmed, is_rejected)");

    qInfo() << "Database schema created successfully";
    return true;
}

bool Database::populateDefaultSystems() {
    // Use SystemDetector to get all default systems
    SystemDetector detector;

    // Get all system names from the constants
    using namespace Constants::Systems;
    int insertedCount = 0;

    for (auto it = SYSTEMS.begin(); it != SYSTEMS.end(); ++it) {
        const auto &def = it.value();
        SystemInfo system;
        system.name = def.internalName;
        system.displayName = def.displayName;
        system.manufacturer = def.manufacturer;
        system.generation = def.generation;
        system.extensions = def.extensions;
        system.preferredHash = def.preferredHash;

        if (insertSystem(system) > 0) {
            insertedCount++;
        }
    }

    qInfo() << "Populated" << insertedCount << "default systems";
    return insertedCount > 0;
}

} // namespace Remus
