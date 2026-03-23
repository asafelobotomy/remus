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

bool Database::removeFile(int fileId)
{
    // Remove associated match record first to keep referential integrity
    QSqlQuery matchQuery(m_db);
    matchQuery.prepare("DELETE FROM matches WHERE file_id = ?");
    matchQuery.addBindValue(fileId);
    if (!matchQuery.exec()) {
        logError("Failed to delete match for file " + QString::number(fileId) + ": "
                 + matchQuery.lastError().text());
        // Continue — the file record removal is more important
    }

    QSqlQuery fileQuery(m_db);
    fileQuery.prepare("DELETE FROM files WHERE id = ?");
    fileQuery.addBindValue(fileId);
    if (!fileQuery.exec()) {
        logError("Failed to remove file record " + QString::number(fileId) + ": "
                 + fileQuery.lastError().text());
        return false;
    }

    return fileQuery.numRowsAffected() > 0;
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

int Database::insertFile(const FileRecord &record)
{
    const QString classificationName = !record.archiveInternalPath.isEmpty()
        ? record.archiveInternalPath
        : record.filename;
    const PatchedRomInfo patchedInfo = PatchedRomParser::parse(classificationName);
    const bool hasExplicitClassification = record.fileType != QStringLiteral("official") ||
        record.isPatched || !record.patchName.isEmpty();
    const QString baseTitle = record.baseTitle.isEmpty() ? patchedInfo.baseTitle : record.baseTitle;
    const QString fileType = hasExplicitClassification ? record.fileType : patchedInfo.fileType;
    const bool isPatched = record.isPatched || patchedInfo.isPatched;
    const QString patchName = record.patchName.isEmpty() ? patchedInfo.patchName : record.patchName;

    QSqlQuery query(m_db);
    // Use INSERT OR IGNORE to avoid duplicates based on original_path + filename
    query.prepare(R"(
        INSERT OR IGNORE INTO files 
        (library_id, original_path, current_path, filename, extension, 
         file_size, is_compressed, archive_path, archive_internal_path, 
         system_id, is_primary, parent_file_id, base_title, file_type, is_patched,
         patch_name, last_modified)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(record.libraryId);
    query.addBindValue(record.originalPath);
    query.addBindValue(record.currentPath);
    query.addBindValue(record.filename);
    query.addBindValue(record.extension);
    query.addBindValue(record.fileSize);
    query.addBindValue(record.isCompressed);
    query.addBindValue(record.archivePath.isEmpty() ? QVariant() : record.archivePath);
    query.addBindValue(record.archiveInternalPath.isEmpty() ? QVariant() : record.archiveInternalPath);
    query.addBindValue(record.systemId > 0 ? record.systemId : QVariant());
    query.addBindValue(record.isPrimary);
    query.addBindValue(record.parentFileId > 0 ? record.parentFileId : QVariant());
    query.addBindValue(baseTitle.isEmpty() ? QVariant() : baseTitle);
    query.addBindValue(fileType);
    query.addBindValue(isPatched);
    query.addBindValue(patchName.isEmpty() ? QVariant() : patchName);
    query.addBindValue(record.lastModified);

    if (!query.exec()) {
        logError("Failed to insert file: " + query.lastError().text());
        return 0;
    }

    return query.lastInsertId().toInt();
}

bool Database::updateFileHashes(int fileId, const QString &crc32,
                                 const QString &md5, const QString &sha1)
{
    QSqlQuery query(m_db);
    const AppliedPatchRecord lineage = findAppliedPatchByOutputHashes(crc32, md5, sha1);
    const bool hasLineage = lineage.id > 0;

    QString updateStatement = R"(
        UPDATE files 
        SET crc32 = ?, md5 = ?, sha1 = ?, hash_calculated = 1
    )";
    if (hasLineage) {
        updateStatement += R"(,
        base_title = COALESCE(?, base_title),
        file_type = COALESCE(?, file_type),
        is_patched = 1,
        patch_name = COALESCE(?, patch_name)
    )";
    }
    updateStatement += " WHERE id = ?";

    query.prepare(updateStatement);
    query.addBindValue(crc32);
    query.addBindValue(md5);
    query.addBindValue(sha1);
    if (hasLineage) {
        query.addBindValue(lineage.baseTitle.isEmpty() ? QVariant() : lineage.baseTitle);
        query.addBindValue(lineage.fileType.isEmpty() ? QVariant() : lineage.fileType);
        query.addBindValue(lineage.patchName.isEmpty() ? QVariant() : lineage.patchName);
    }
    query.addBindValue(fileId);

    if (!query.exec()) {
        logError("Failed to update file hashes: " + query.lastError().text());
        return false;
    }

    return true;
}

QList<FileRecord> Database::getFilesWithoutHashes()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, library_id, current_path, filename, extension,
             file_size, system_id, is_primary, is_compressed,
             archive_path, archive_internal_path, base_title, file_type,
             is_patched, patch_name
        FROM files 
        WHERE hash_calculated = 0 AND is_primary = 1
    )");

    if (!query.exec()) {
        logError("Failed to query files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        FileRecord record;
        record.id = query.value(0).toInt();
        record.libraryId = query.value(1).toInt();
        record.currentPath = query.value(2).toString();
        record.filename = query.value(3).toString();
        record.extension = query.value(4).toString();
        record.fileSize = query.value(5).toLongLong();
        record.systemId = query.value(6).toInt();
        record.isPrimary = query.value(7).toBool();
        record.isCompressed = query.value(8).toBool();
        record.archivePath = query.value(9).toString();
        record.archiveInternalPath = query.value(10).toString();
        record.baseTitle = query.value(11).toString();
        record.fileType = query.value(12).toString();
        record.isPatched = query.value(13).toBool();
        record.patchName = query.value(14).toString();
        files.append(record);
    }

    return files;
}

QMap<QString, int> Database::getFileCountBySystem()
{
    QMap<QString, int> counts;

    QSqlQuery query(m_db);
    query.exec(R"(
        SELECT s.name, COUNT(f.id) 
        FROM files f 
        LEFT JOIN systems s ON f.system_id = s.id 
        WHERE f.is_primary = 1
        GROUP BY s.name
    )");

    while (query.next()) {
        QString systemName = query.value(0).toString();
        int count = query.value(1).toInt();
        counts[systemName.isEmpty() ? "Unknown" : systemName] = count;
    }

    return counts;
}

FileRecord Database::getFileById(int fileId)
{
    FileRecord record;
    
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, library_id, original_path, current_path, filename, extension,
               file_size, is_compressed, archive_path, archive_internal_path,
               system_id, crc32, md5, sha1, hash_calculated, 
               is_primary, parent_file_id, base_title, file_type, is_patched,
             patch_name, is_processed, processing_status,
             last_modified, scanned_at
        FROM files 
        WHERE id = ?
    )");
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        record.id = query.value(0).toInt();
        record.libraryId = query.value(1).toInt();
        record.originalPath = query.value(2).toString();
        record.currentPath = query.value(3).toString();
        record.filename = query.value(4).toString();
        record.extension = query.value(5).toString();
        record.fileSize = query.value(6).toLongLong();
        record.isCompressed = query.value(7).toBool();
        record.archivePath = query.value(8).toString();
        record.archiveInternalPath = query.value(9).toString();
        record.systemId = query.value(10).toInt();
        record.crc32 = query.value(11).toString();
        record.md5 = query.value(12).toString();
        record.sha1 = query.value(13).toString();
        record.hashCalculated = query.value(14).toBool();
        record.isPrimary = query.value(15).toBool();
        record.parentFileId = query.value(16).toInt();
        record.baseTitle = query.value(17).toString();
        record.fileType = query.value(18).toString();
        record.isPatched = query.value(19).toBool();
        record.patchName = query.value(20).toString();
        record.isProcessed = query.value(21).toBool();
        record.processingStatus = query.value(22).toString();
        record.lastModified = query.value(23).toDateTime();
        record.scannedAt = query.value(24).toDateTime();
    }
    
    return record;
}

QList<FileRecord> Database::getAllFiles()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files")) {
        logError("Failed to get all files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }

    return files;
}

QList<FileRecord> Database::getExistingFiles()
{
    QList<FileRecord> files;

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files")) {
        logError("Failed to get existing files: " + query.lastError().text());
        return files;
    }

    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            QFileInfo pathInfo(record.currentPath);
            if (pathInfo.exists()) {
                files.append(record);
            }
        }
    }

    return files;
}

QList<FileRecord> Database::getFilesBySystem(const QString &systemName)
{
    QList<FileRecord> files;
    
    // First get system ID
    int systemId = getSystemId(systemName);
    if (systemId == 0) {
        logError("System not found: " + systemName);
        return files;
    }
    
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM files WHERE system_id = ? AND is_primary = 1");
    query.addBindValue(systemId);
    
    if (!query.exec()) {
        logError("Failed to get files by system: " + query.lastError().text());
        return files;
    }
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

QList<FileRecord> Database::getFilesByParent(int parentId)
{
    QList<FileRecord> files;
    
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM files WHERE parent_file_id = ?");
    query.addBindValue(parentId);
    
    if (!query.exec()) {
        logError("Failed to get files by parent: " + query.lastError().text());
        return files;
    }
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

bool Database::updateFilePath(int fileId, const QString &newPath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET current_path = ? WHERE id = ?");
    query.addBindValue(newPath);
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to update file path: " + query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

bool Database::updateFileOriginalPath(int fileId, const QString &newOriginalPath)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET original_path = ?, current_path = ? WHERE id = ?");
    query.addBindValue(newOriginalPath);
    query.addBindValue(newOriginalPath);
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to update file original path: " + query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

QMap<int, Database::MatchResult> Database::getAllMatches()
{
    QMap<int, MatchResult> results;
    QSqlQuery query(m_db);
    
    // Join matches with games to get full info including metadata
    // SELECT ONLY THE BEST MATCH per file using a CTE:
    // 1. Prefer confirmed matches (is_confirmed = 1)
    // 2. Prefer matches with higher confidence
    // 3. Prefer manual matches over automatic
    // 4. Use highest ID (most recent) as tiebreaker
    query.prepare(R"(
        WITH best_matches AS (
            SELECT file_id, MAX(
                is_confirmed * 1000000 + 
                confidence * 1000 + 
                CASE match_method 
                    WHEN 'manual' THEN 300
                    WHEN 'hash' THEN 200
                    WHEN 'filename' THEN 100
                    ELSE 0
                END +
                (id * 0.001)
            ) as score
            FROM matches
            GROUP BY file_id
        )
        SELECT m.id, m.file_id, m.game_id, m.match_method, m.confidence, 
               m.is_confirmed, m.is_rejected,
               g.title, g.publisher, g.release_date, g.developer, g.description,
               g.genres, g.players, g.region, g.rating, m.name_match_score
        FROM matches m
        LEFT JOIN games g ON m.game_id = g.id
        INNER JOIN best_matches bm ON m.file_id = bm.file_id
        WHERE (
            m.is_confirmed * 1000000 + 
            m.confidence * 1000 + 
            CASE m.match_method 
                WHEN 'manual' THEN 300
                WHEN 'hash' THEN 200
                WHEN 'filename' THEN 100
                ELSE 0
            END +
            (m.id * 0.001)
        ) = bm.score
    )");
    
    if (!query.exec()) {
        logError("Failed to get all matches: " + query.lastError().text());
        return results;
    }
    
    while (query.next()) {
        MatchResult result;
        result.matchId = query.value(0).toInt();
        result.fileId = query.value(1).toInt();
        result.gameId = query.value(2).toInt();
        result.matchMethod = query.value(3).toString();
        result.confidence = query.value(4).toFloat();
        result.isConfirmed = query.value(5).toBool();
        result.isRejected = query.value(6).toBool();
        result.gameTitle = query.value(7).toString();
        result.publisher = query.value(8).toString();
        
        // Parse year from release_date (ISO format: YYYY-MM-DD)
        QString releaseDate = query.value(9).toString();
        if (!releaseDate.isEmpty()) {
            result.releaseYear = releaseDate.left(4).toInt();
        }
        
        // Populate remaining metadata fields
        result.developer = query.value(10).toString();
        result.description = query.value(11).toString();
        result.genre = query.value(12).toString();
        result.players = query.value(13).toString();
        result.region = query.value(14).toString();
        result.rating = query.value(15).toFloat();
        result.nameMatchScore = query.value(16).toFloat();
        
        results[result.fileId] = result;
    }
    
    qDebug() << "Database::getAllMatches() loaded" << results.count() << "matches";
    return results;
}

Database::MatchResult Database::getMatchForFile(int fileId)
{
    MatchResult result;
    QSqlQuery query(m_db);
    
    query.prepare(R"(
        SELECT m.id, m.file_id, m.game_id, m.match_method, m.confidence, 
               m.is_confirmed, m.is_rejected,
               g.title, g.publisher, g.developer, g.release_date,
               g.description, g.genres, g.players, g.region, g.rating,
               m.name_match_score
        FROM matches m
        LEFT JOIN games g ON m.game_id = g.id
        WHERE m.file_id = ?
        ORDER BY m.confidence DESC
        LIMIT 1
    )");
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to get match for file: " + query.lastError().text());
        return result;
    }
    
    if (query.next()) {
        result.matchId = query.value(0).toInt();
        result.fileId = query.value(1).toInt();
        result.gameId = query.value(2).toInt();
        result.matchMethod = query.value(3).toString();
        result.confidence = query.value(4).toFloat();
        result.isConfirmed = query.value(5).toBool();
        result.isRejected = query.value(6).toBool();
        result.gameTitle = query.value(7).toString();
        result.publisher = query.value(8).toString();
        result.developer = query.value(9).toString();
        
        QString releaseDate = query.value(10).toString();
        if (!releaseDate.isEmpty()) {
            result.releaseYear = releaseDate.left(4).toInt();
        }
        
        result.description = query.value(11).toString();
        result.genre = query.value(12).toString();
        result.players = query.value(13).toString();
        result.region = query.value(14).toString();
        result.rating = query.value(15).toFloat();
        result.nameMatchScore = query.value(16).toFloat();
    }
    
    return result;
}

int Database::insertGame(const QString &title, int systemId, const QString &region,
                         const QString &publisher, const QString &developer,
                         const QString &releaseDate, const QString &description,
                         const QString &genres, const QString &players, float rating)
{
    QSqlQuery query(m_db);
    
    // Check if game already exists
    query.prepare("SELECT id FROM games WHERE title = ? AND system_id = ? AND region = ?");
    query.addBindValue(title);
    query.addBindValue(systemId);
    query.addBindValue(region);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    
    // Insert new game
    query.prepare("INSERT INTO games (title, system_id, region, publisher, developer, release_date, "
                  "description, genres, players, rating) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(title);
    query.addBindValue(systemId);
    query.addBindValue(region);
    query.addBindValue(publisher);
    query.addBindValue(developer);
    query.addBindValue(releaseDate);
    query.addBindValue(description);
    query.addBindValue(genres);
    query.addBindValue(players);
    query.addBindValue(rating);
    
    if (!query.exec()) {
        logError("Failed to insert game: " + query.lastError().text());
        return 0;
    }
    
    return query.lastInsertId().toInt();
}

bool Database::updateGame(int gameId,
                          const QString &publisher, const QString &developer,
                          const QString &releaseDate, const QString &description,
                          const QString &genres, const QString &players,
                          float rating)
{
    QStringList setClauses;
    QVariantList values;

    auto addIfSet = [&](const QString &col, const QString &val) {
        if (!val.isEmpty()) {
            setClauses << col + " = ?";
            values << val;
        }
    };

    addIfSet("publisher",    publisher);
    addIfSet("developer",    developer);
    addIfSet("release_date", releaseDate);
    addIfSet("description",  description);
    addIfSet("genres",       genres);
    addIfSet("players",      players);

    if (rating >= 0.0f) {
        setClauses << "rating = ?";
        values << static_cast<double>(rating);
    }

    if (setClauses.isEmpty()) return true; // nothing to update

    setClauses << "updated_at = CURRENT_TIMESTAMP";

    QSqlQuery query(m_db);
    query.prepare("UPDATE games SET " + setClauses.join(", ") + " WHERE id = ?");
    for (const auto &v : values) query.addBindValue(v);
    query.addBindValue(gameId);

    if (!query.exec()) {
        logError("Failed to update game: " + query.lastError().text());
        return false;
    }
    return true;
}

bool Database::insertMatch(int fileId, int gameId, float confidence, const QString &matchMethod,
                           float nameMatchScore)
{
    QSqlQuery query(m_db);
    
    // First check if match already exists
    query.prepare("SELECT id FROM matches WHERE file_id = ? AND game_id = ?");
    query.addBindValue(fileId);
    query.addBindValue(gameId);
    
    if (!query.exec()) {
        logError("Failed to check existing match: " + query.lastError().text());
        return false;
    }
    
    if (query.next()) {
        // Update existing match
        int matchId = query.value(0).toInt();
        query.prepare("UPDATE matches SET confidence = ?, match_method = ?, name_match_score = ?, "
                      "matched_at = CURRENT_TIMESTAMP WHERE id = ?");
        query.addBindValue(confidence);
        query.addBindValue(matchMethod);
        query.addBindValue(nameMatchScore);
        query.addBindValue(matchId);
    } else {
        // Insert new match
        query.prepare("INSERT INTO matches (file_id, game_id, confidence, match_method, name_match_score, matched_at) "
                      "VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)");
        query.addBindValue(fileId);
        query.addBindValue(gameId);
        query.addBindValue(confidence);
        query.addBindValue(matchMethod);
        query.addBindValue(nameMatchScore);
    }
    
    if (!query.exec()) {
        logError("Failed to insert/update match: " + query.lastError().text());
        return false;
    }
    
    return true;
}

bool Database::confirmMatch(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE matches SET is_confirmed = 1, is_rejected = 0, confidence = 100 WHERE file_id = ?");
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to confirm match: " + query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

bool Database::rejectMatch(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE matches SET is_rejected = 1, is_confirmed = 0 WHERE file_id = ?");
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to reject match: " + query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

QString Database::getFilePath(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT current_path FROM files WHERE id = ?");
    query.addBindValue(fileId);
    
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    
    return QString();
}

bool Database::markFileProcessed(int fileId, const QString &status)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET is_processed = 1, processing_status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to mark file as processed: " + query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

bool Database::markFileUnprocessed(int fileId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE files SET is_processed = 0, processing_status = 'unprocessed' WHERE id = ?");
    query.addBindValue(fileId);
    
    if (!query.exec()) {
        logError("Failed to mark file as unprocessed: " + query.lastError().text());
        return false;
    }
    
    return query.numRowsAffected() > 0;
}

QList<FileRecord> Database::getProcessedFiles()
{
    QList<FileRecord> files;
    
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files WHERE is_primary = 1 AND is_processed = 1")) {
        logError("Failed to get processed files: " + query.lastError().text());
        return files;
    }
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

QList<FileRecord> Database::getUnprocessedFiles()
{
    QList<FileRecord> files;
    
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id FROM files WHERE is_primary = 1 AND is_processed = 0")) {
        logError("Failed to get unprocessed files: " + query.lastError().text());
        return files;
    }
    
    while (query.next()) {
        int fileId = query.value(0).toInt();
        FileRecord record = getFileById(fileId);
        if (record.id > 0) {
            files.append(record);
        }
    }
    
    return files;
}

    bool Database::insertAppliedPatch(const AppliedPatchRecord &record)
    {
        QSqlQuery query(m_db);
        query.prepare(R"(
            INSERT INTO applied_patches (
                base_path, output_path, patch_path, patch_format,
                base_title, patch_name, file_type,
                source_checksum, target_checksum, patch_checksum,
                base_crc32, base_md5, base_sha1,
                output_crc32, output_md5, output_sha1
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");
        query.addBindValue(record.basePath);
        query.addBindValue(record.outputPath);
        query.addBindValue(record.patchPath);
        query.addBindValue(record.patchFormat.isEmpty() ? QVariant() : record.patchFormat);
        query.addBindValue(record.baseTitle.isEmpty() ? QVariant() : record.baseTitle);
        query.addBindValue(record.patchName.isEmpty() ? QVariant() : record.patchName);
        query.addBindValue(record.fileType.isEmpty() ? QVariant(QStringLiteral("hack")) : record.fileType);
        query.addBindValue(record.sourceChecksum.isEmpty() ? QVariant() : record.sourceChecksum);
        query.addBindValue(record.targetChecksum.isEmpty() ? QVariant() : record.targetChecksum);
        query.addBindValue(record.patchChecksum.isEmpty() ? QVariant() : record.patchChecksum);
        query.addBindValue(record.baseCrc32.isEmpty() ? QVariant() : record.baseCrc32);
        query.addBindValue(record.baseMd5.isEmpty() ? QVariant() : record.baseMd5);
        query.addBindValue(record.baseSha1.isEmpty() ? QVariant() : record.baseSha1);
        query.addBindValue(record.outputCrc32.isEmpty() ? QVariant() : record.outputCrc32);
        query.addBindValue(record.outputMd5.isEmpty() ? QVariant() : record.outputMd5);
        query.addBindValue(record.outputSha1.isEmpty() ? QVariant() : record.outputSha1);

        if (!query.exec()) {
            logError("Failed to insert applied patch: " + query.lastError().text());
            return false;
        }

        return true;
    }

    Database::AppliedPatchRecord Database::findAppliedPatchByOutputHashes(
        const QString &crc32, const QString &md5, const QString &sha1)
    {
        AppliedPatchRecord record;
        QStringList conditions;
        QVariantList values;

        if (!sha1.isEmpty()) {
            conditions << "output_sha1 = ?";
            values << sha1;
        }
        if (!md5.isEmpty()) {
            conditions << "output_md5 = ?";
            values << md5;
        }
        if (!crc32.isEmpty()) {
            conditions << "output_crc32 = ?";
            values << crc32;
        }

        if (conditions.isEmpty()) {
            return record;
        }

        QSqlQuery query(m_db);
        query.prepare(QString(R"(
            SELECT id, base_path, output_path, patch_path, patch_format,
                   base_title, patch_name, file_type,
                   source_checksum, target_checksum, patch_checksum,
                   base_crc32, base_md5, base_sha1,
                   output_crc32, output_md5, output_sha1
            FROM applied_patches
            WHERE %1
            ORDER BY applied_at DESC
            LIMIT 1
        )").arg(conditions.join(" OR ")));

        for (const QVariant &value : values) {
            query.addBindValue(value);
        }

        if (!query.exec()) {
            logError("Failed to query applied patch lineage: " + query.lastError().text());
            return record;
        }

        if (!query.next()) {
            return record;
        }

        record.id = query.value(0).toInt();
        record.basePath = query.value(1).toString();
        record.outputPath = query.value(2).toString();
        record.patchPath = query.value(3).toString();
        record.patchFormat = query.value(4).toString();
        record.baseTitle = query.value(5).toString();
        record.patchName = query.value(6).toString();
        record.fileType = query.value(7).toString();
        record.sourceChecksum = query.value(8).toString();
        record.targetChecksum = query.value(9).toString();
        record.patchChecksum = query.value(10).toString();
        record.baseCrc32 = query.value(11).toString();
        record.baseMd5 = query.value(12).toString();
        record.baseSha1 = query.value(13).toString();
        record.outputCrc32 = query.value(14).toString();
        record.outputMd5 = query.value(15).toString();
        record.outputSha1 = query.value(16).toString();
        return record;
    }

void Database::logError(const QString &message)
{
    qCritical() << message;
    emit databaseError(message);
}

} // namespace Remus
