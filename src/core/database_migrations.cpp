#include "database.h"
#include "system_detector.h"
#include "constants/constants.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace Remus {

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

} // namespace Remus
