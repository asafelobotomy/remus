#include "database.h"
#include "patched_rom_parser.h"
#include "system_detector.h"
#include "constants/constants.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QFileInfo>
#include <QUuid>

namespace Remus {

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    close();
}

bool Database::initialize(const QString &dbPath, const QString &connectionName)
{
    m_dbPath = dbPath;
    m_connectionName = connectionName.isEmpty()
        ? QStringLiteral("remus-") + QUuid::createUuid().toString(QUuid::Id128)
        : connectionName;

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        logError(Constants::Errors::Database::FAILED_TO_OPEN);
        return false;
    }

    qInfo() << "Database opened:" << dbPath;

    // Check if schema exists
    QSqlQuery query(m_db);
    query.exec(QString("SELECT name FROM sqlite_master WHERE type='table' AND name='%1'")
        .arg(Constants::DatabaseSchema::Tables::SYSTEMS));
    
    bool isNewDatabase = !query.next();
    
    if (isNewDatabase) {
        // Schema doesn't exist, create it
        if (!createSchema()) {
            logError(Constants::Errors::Database::FAILED_TO_CREATE_SCHEMA);
            return false;
        }
        
        // Populate default systems
        if (!populateDefaultSystems()) {
            logError(Constants::Errors::Database::FAILED_TO_POPULATE_SYSTEMS);
            return false;
        }
    }
    
    // Run migrations for new columns
    runMigrations();

    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (!m_connectionName.isEmpty()) {
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

void Database::runMigrations()
{
    QSqlQuery query(m_db);
    
    // Check if is_processed column exists
    query.exec(QString("PRAGMA table_info(%1)").arg(Constants::DatabaseSchema::Tables::FILES));
    bool hasIsProcessed = false;
    bool hasProcessingStatus = false;
    bool hasIsCompressed = false;
    bool hasArchivePath = false;
    bool hasArchiveInternalPath = false;
    bool hasBaseTitle = false;
    bool hasFileType = false;
    bool hasIsPatched = false;
    bool hasPatchName = false;
    while (query.next()) {
        QString columnName = query.value(1).toString();
        if (columnName == Constants::DatabaseSchema::Columns::Files::IS_PROCESSED) hasIsProcessed = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::PROCESSING_STATUS) hasProcessingStatus = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::IS_COMPRESSED) hasIsCompressed = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::ARCHIVE_PATH) hasArchivePath = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::ARCHIVE_INTERNAL_PATH) hasArchiveInternalPath = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::BASE_TITLE) hasBaseTitle = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::FILE_TYPE) hasFileType = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::IS_PATCHED) hasIsPatched = true;
        if (columnName == Constants::DatabaseSchema::Columns::Files::PATCH_NAME) hasPatchName = true;
    }
    
    // Add is_processed column if missing
    if (!hasIsProcessed) {
        qInfo() << "Migration: Adding is_processed column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::IS_PROCESSED))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }
    
    // Add processing_status column if missing
    if (!hasProcessingStatus) {
        qInfo() << "Migration: Adding processing_status column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 TEXT DEFAULT '%3'")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::PROCESSING_STATUS,
                 Constants::Engines::ProcessingStatus::UNPROCESSED))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasIsCompressed) {
        qInfo() << "Migration: Adding is_compressed column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::IS_COMPRESSED))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasArchivePath) {
        qInfo() << "Migration: Adding archive_path column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::ARCHIVE_PATH))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasArchiveInternalPath) {
        qInfo() << "Migration: Adding archive_internal_path column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::ARCHIVE_INTERNAL_PATH))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasBaseTitle) {
        qInfo() << "Migration: Adding base_title column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::BASE_TITLE))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasFileType) {
        qInfo() << "Migration: Adding file_type column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 TEXT DEFAULT 'official'")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::FILE_TYPE))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasIsPatched) {
        qInfo() << "Migration: Adding is_patched column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::IS_PATCHED))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    if (!hasPatchName) {
        qInfo() << "Migration: Adding patch_name column to files table";
        if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
            .arg(Constants::DatabaseSchema::Tables::FILES,
                 Constants::DatabaseSchema::Columns::Files::PATCH_NAME))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    query.exec(QString(R"(
        CREATE TABLE IF NOT EXISTS %1 (
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
    )").arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES));
    query.exec(QString("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_sha1 ON %1(output_sha1)")
               .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES));
    query.exec(QString("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_md5 ON %1(output_md5)")
               .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES));
    query.exec(QString("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_crc32 ON %1(output_crc32)")
               .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES));

    SystemDetector detector;

    for (const QString &name : Constants::Systems::getSystemInternalNames()) {
        const SystemInfo info = detector.getSystemInfo(name);
        if (info.name.isEmpty()) {
            continue;
        }

        QSqlQuery selectCanonicalSlot(m_db);
        selectCanonicalSlot.prepare("SELECT name FROM systems WHERE id = ?");
        selectCanonicalSlot.addBindValue(info.id);
        if (selectCanonicalSlot.exec() && selectCanonicalSlot.next()) {
            const QString occupyingName = selectCanonicalSlot.value(0).toString();
            if (!occupyingName.isEmpty() && occupyingName != info.name) {
                const QString movedName = QStringLiteral("%1__legacy_slot_%2")
                    .arg(occupyingName)
                    .arg(info.id);

                QSqlQuery renameOccupyingRow(m_db);
                renameOccupyingRow.prepare("UPDATE systems SET id = id + 1000, name = ? WHERE id = ?");
                renameOccupyingRow.addBindValue(movedName);
                renameOccupyingRow.addBindValue(info.id);
                if (!renameOccupyingRow.exec()) {
                    logError("Failed to free canonical system slot: " + renameOccupyingRow.lastError().text());
                    continue;
                }
            }
        }

        int existingId = 0;
        QSqlQuery selectSystem(m_db);
        selectSystem.prepare("SELECT id FROM systems WHERE name = ?");
        selectSystem.addBindValue(info.name);
        if (selectSystem.exec() && selectSystem.next()) {
            existingId = selectSystem.value(0).toInt();
        }

        if (existingId > 0 && existingId != info.id) {
            const QString legacyName = QStringLiteral("%1__legacy_%2").arg(info.name).arg(existingId);

            QSqlQuery renameSystem(m_db);
            renameSystem.prepare("UPDATE systems SET name = ? WHERE id = ?");
            renameSystem.addBindValue(legacyName);
            renameSystem.addBindValue(existingId);
            if (!renameSystem.exec()) {
                logError("Failed to rename legacy system row: " + renameSystem.lastError().text());
                continue;
            }

            if (insertSystem(info) == 0) {
                logError("Failed to insert canonical system row for: " + info.name);
                continue;
            }

            QSqlQuery updateFiles(m_db);
            updateFiles.prepare("UPDATE files SET system_id = ? WHERE system_id = ?");
            updateFiles.addBindValue(info.id);
            updateFiles.addBindValue(existingId);
            if (!updateFiles.exec()) {
                logError("Failed to update file system IDs: " + updateFiles.lastError().text());
            }

            QSqlQuery updateGames(m_db);
            updateGames.prepare("UPDATE games SET system_id = ? WHERE system_id = ?");
            updateGames.addBindValue(info.id);
            updateGames.addBindValue(existingId);
            if (!updateGames.exec()) {
                logError("Failed to update game system IDs: " + updateGames.lastError().text());
            }

            QSqlQuery deleteLegacy(m_db);
            deleteLegacy.prepare("DELETE FROM systems WHERE id = ?");
            deleteLegacy.addBindValue(existingId);
            if (!deleteLegacy.exec()) {
                logError("Failed to delete legacy system row: " + deleteLegacy.lastError().text());
            }

            continue;
        }

        if (existingId == 0 && insertSystem(info) == 0) {
            logError("Failed to backfill missing system row for: " + info.name);
        }
    }

    QSqlQuery repairFiles(m_db);
    if (repairFiles.exec(R"(
        SELECT f.id, f.extension, f.current_path, f.is_compressed, f.archive_internal_path
        FROM files f
        LEFT JOIN systems s ON f.system_id = s.id
        WHERE f.system_id IS NULL OR s.id IS NULL
    )")) {
        while (repairFiles.next()) {
            const int fileId = repairFiles.value(0).toInt();
            const QString extension = repairFiles.value(1).toString();
            const QString currentPath = repairFiles.value(2).toString();
            const bool isCompressed = repairFiles.value(3).toBool();
            const QString archiveInternalPath = repairFiles.value(4).toString();

            const QString detectPath = isCompressed && !archiveInternalPath.isEmpty()
                ? archiveInternalPath
                : currentPath;
            const QString systemName = detector.detectSystem(extension, detectPath);
            const int systemId = getSystemId(systemName);
            if (systemId == 0) {
                continue;
            }

            QSqlQuery updateFile(m_db);
            updateFile.prepare("UPDATE files SET system_id = ? WHERE id = ?");
            updateFile.addBindValue(systemId);
            updateFile.addBindValue(fileId);
            if (!updateFile.exec()) {
                logError("Failed to repair file system ID: " + updateFile.lastError().text());
            }
        }
    }

    // ── Matches table migrations ──────────────────────────────────────────
    QSqlQuery matchesQuery(m_db);
    matchesQuery.exec(QString("PRAGMA table_info(%1)")
                      .arg(Constants::DatabaseSchema::Tables::MATCHES));
    bool hasNameMatchScore = false;
    while (matchesQuery.next()) {
        if (matchesQuery.value(1).toString() ==
            Constants::DatabaseSchema::Columns::Matches::NAME_MATCH_SCORE)
            hasNameMatchScore = true;
    }
    if (!hasNameMatchScore) {
        qInfo() << "Migration: Adding name_match_score column to matches table";
        if (!matchesQuery.exec(QString("ALTER TABLE %1 ADD COLUMN %2 REAL DEFAULT 0")
                               .arg(Constants::DatabaseSchema::Tables::MATCHES,
                                    Constants::DatabaseSchema::Columns::Matches::NAME_MATCH_SCORE))) {
            logError(Constants::Errors::Database::MIGRATION_FAILED);
        }
    }

    // ── mod_installations table ───────────────────────────────────────────────
    QSqlQuery modQuery(m_db);
    if (!modQuery.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS mod_installations (
            id                INTEGER PRIMARY KEY AUTOINCREMENT,
            base_file_id      INTEGER NOT NULL,
            patched_file_id   INTEGER,
            catalog_mod_id    TEXT NOT NULL,
            mod_title         TEXT NOT NULL,
            mod_author        TEXT,
            mod_version       TEXT,
            mod_type          TEXT DEFAULT 'hack',
            patch_format      TEXT,
            patch_url         TEXT,
            patch_sha1        TEXT,
            source_url        TEXT,
            installed_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (base_file_id)    REFERENCES files(id) ON DELETE CASCADE,
            FOREIGN KEY (patched_file_id) REFERENCES files(id) ON DELETE SET NULL
        )
    )"))) {
        logError("Migration: Failed to create mod_installations table: " + modQuery.lastError().text());
    }
    modQuery.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_mod_installations_base ON mod_installations(base_file_id)"));
    modQuery.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_mod_installations_catalog ON mod_installations(catalog_mod_id)"));

    // ── mod_catalog_cache table ───────────────────────────────────────────────
    QSqlQuery cacheQuery(m_db);
    if (!cacheQuery.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS mod_catalog_cache (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            source_url  TEXT NOT NULL UNIQUE,
            etag        TEXT,
            fetched_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            mod_count   INTEGER DEFAULT 0
        )
    )"))) {
        logError("Migration: Failed to create mod_catalog_cache table: " + cacheQuery.lastError().text());
    }
}

bool Database::createSchema()
{
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
            hash_calculated BOOLEAN DEFAULT 0,
            is_primary BOOLEAN DEFAULT 1,
            parent_file_id INTEGER,
            base_title TEXT,
            file_type TEXT DEFAULT 'official',
            is_patched BOOLEAN DEFAULT 0,
            patch_name TEXT,
            is_processed BOOLEAN DEFAULT 0,
            processing_status TEXT DEFAULT 'unprocessed',
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

    // Migration: Add is_processed column if not exists (for existing databases)
    query.exec("ALTER TABLE files ADD COLUMN is_processed BOOLEAN DEFAULT 0");
    query.exec("ALTER TABLE files ADD COLUMN processing_status TEXT DEFAULT 'unprocessed'");
    query.exec("ALTER TABLE files ADD COLUMN base_title TEXT");
    query.exec("ALTER TABLE files ADD COLUMN file_type TEXT DEFAULT 'official'");
    query.exec("ALTER TABLE files ADD COLUMN is_patched BOOLEAN DEFAULT 0");
    query.exec("ALTER TABLE files ADD COLUMN patch_name TEXT");
    
    // Create index for processed status
    query.exec("CREATE INDEX IF NOT EXISTS idx_files_processed ON files(is_processed)");

    // Create indexes
    query.exec("CREATE INDEX IF NOT EXISTS idx_files_current_path ON files(current_path)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_files_system_id ON files(system_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_files_hashes ON files(crc32, md5, sha1)");
    query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_files_original_path ON files(original_path, filename)");

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

    qInfo() << "Database schema created successfully";
    return true;
}

bool Database::populateDefaultSystems()
{
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

int Database::insertLibrary(const QString &path, const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO libraries (path, name) VALUES (?, ?)");
    query.addBindValue(path);
    query.addBindValue(name.isEmpty() ? QFileInfo(path).fileName() : name);

    if (!query.exec()) {
        logError("Failed to insert library: " + query.lastError().text());
        return 0;
    }

    // Get library ID
    query.prepare("SELECT id FROM libraries WHERE path = ?");
    query.addBindValue(path);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool Database::deleteLibrary(int libraryId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM libraries WHERE id = ?");
    query.addBindValue(libraryId);

    if (!query.exec()) {
        logError("Failed to delete library: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

QString Database::getLibraryPath(int libraryId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT path FROM libraries WHERE id = ?");
    query.addBindValue(libraryId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

bool Database::deleteFilesForLibrary(int libraryId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM files WHERE library_id = ?");
    query.addBindValue(libraryId);

    if (!query.exec()) {
        logError("Failed to delete library files: " + query.lastError().text());
        return false;
    }

    return true;
}

int Database::insertSystem(const SystemInfo &system)
{
    if (system.name.isEmpty()) {
        logError("Cannot insert system with empty name");
        return 0;
    }
    if (system.extensions.isEmpty()) {
        logError("Cannot insert system '" + system.name + "' with empty extensions list");
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO systems 
        (id, name, display_name, manufacturer, generation, extensions, preferred_hash)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(name) DO UPDATE SET
            display_name = excluded.display_name,
            manufacturer = excluded.manufacturer,
            generation = excluded.generation,
            extensions = excluded.extensions,
            preferred_hash = excluded.preferred_hash
    )");
    query.addBindValue(system.id > 0 ? system.id : QVariant());
    query.addBindValue(system.name);
    query.addBindValue(system.displayName);
    query.addBindValue(system.manufacturer);
    query.addBindValue(system.generation);
    query.addBindValue(system.extensions.join(","));
    query.addBindValue(system.preferredHash);

    if (!query.exec()) {
        logError("Failed to insert system: " + query.lastError().text());
        return 0;
    }

    return getSystemId(system.name);
}

int Database::getSystemId(const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM systems WHERE name = ?");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

QString Database::getSystemDisplayName(int systemId)
{
    // Use SystemResolver for consistent name resolution across all layers
    return SystemResolver::displayName(systemId);
}

void Database::logError(const QString &message)
{
    qCritical() << message;
    emit databaseError(message);
}

} // namespace Remus
