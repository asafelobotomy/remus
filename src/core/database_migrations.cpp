#include "database.h"
#include "system_detector.h"
#include "constants/constants.h"
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <functional>
#include <QSet>

namespace {

bool loadTableColumns(QSqlDatabase &db, const QString &tableName, QSet<QString> &columns, QString &error) {
    static const QSet<QString> kAllowedTables
        = { QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::SYSTEMS),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::LIBRARIES),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::FILES),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::GAMES),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::MATCHES),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::APPLIED_PATCHES),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::CACHE),
              QString::fromLatin1(Remus::Constants::DatabaseSchema::Tables::UNDO_HISTORY) };

    if (!kAllowedTables.contains(tableName)) {
        error = QStringLiteral("Unsupported table name for schema inspection: %1").arg(tableName);
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(tableName))) {
        error = query.lastError().text();
        return false;
    }

    while (query.next()) {
        columns.insert(query.value(1).toString());
    }

    return true;
}

} // namespace

namespace Remus {

bool migrateCanonicalSystems(
    QSqlDatabase &db, Database &database, const std::function<bool(const QString &)> &rollbackAndFail);

bool Database::runMigrations() {
    const bool useTransaction = m_db.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction && !m_db.transaction()) {
        logError("Migration: Failed to start transaction: " + m_db.lastError().text());
        return false;
    }

    auto rollbackAndFail = [&](const QString &message) {
        logError(message);
        if (useTransaction && !m_db.rollback()) {
            logError("Migration: Failed to roll back transaction: " + m_db.lastError().text());
        }
        return false;
    };

    auto execChecked = [&](QSqlQuery &query, const QString &sql, const QString &context) {
        if (!query.exec(sql)) {
            return rollbackAndFail(context + ": " + query.lastError().text());
        }
        return true;
    };

    QSqlQuery query(m_db);

    QSet<QString> fileColumns;
    QString columnError;
    if (!loadTableColumns(m_db, Constants::DatabaseSchema::Tables::FILES, fileColumns, columnError)) {
        return rollbackAndFail("Migration: Failed to inspect files table: " + columnError);
    }

    const bool hasIsProcessed = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::IS_PROCESSED);
    const bool hasProcessingStatus = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::PROCESSING_STATUS);
    const bool hasIsCompressed = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::IS_COMPRESSED);
    const bool hasArchivePath = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::ARCHIVE_PATH);
    const bool hasArchiveInternalPath
        = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::ARCHIVE_INTERNAL_PATH);
    const bool hasBaseTitle = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::BASE_TITLE);
    const bool hasFileType = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::FILE_TYPE);
    const bool hasIsPatched = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::IS_PATCHED);
    const bool hasPatchName = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::PATCH_NAME);
    const bool hasIsConverted = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::IS_CONVERTED);
    const bool hasIsBundled = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::IS_BUNDLED);
    const bool hasBundleOutputPath
        = fileColumns.contains(Constants::DatabaseSchema::Columns::Files::BUNDLE_OUTPUT_PATH);

    // Add is_processed column if missing
    if (!hasIsProcessed) {
        qInfo() << "Migration: Adding is_processed column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::IS_PROCESSED),
                "Migration: Failed to add is_processed column to files table")) {
            return false;
        }
    }

    // Add processing_status column if missing
    if (!hasProcessingStatus) {
        qInfo() << "Migration: Adding processing_status column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT DEFAULT '%3'")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::PROCESSING_STATUS,
                        Constants::Engines::ProcessingStatus::UNPROCESSED),
                "Migration: Failed to add processing_status column to files table")) {
            return false;
        }
    }

    if (!hasIsCompressed) {
        qInfo() << "Migration: Adding is_compressed column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::IS_COMPRESSED),
                "Migration: Failed to add is_compressed column to files table")) {
            return false;
        }
    }

    if (!hasArchivePath) {
        qInfo() << "Migration: Adding archive_path column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::ARCHIVE_PATH),
                "Migration: Failed to add archive_path column to files table")) {
            return false;
        }
    }

    if (!hasArchiveInternalPath) {
        qInfo() << "Migration: Adding archive_internal_path column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::ARCHIVE_INTERNAL_PATH),
                "Migration: Failed to add archive_internal_path column to files table")) {
            return false;
        }
    }

    if (!hasBaseTitle) {
        qInfo() << "Migration: Adding base_title column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::BASE_TITLE),
                "Migration: Failed to add base_title column to files table")) {
            return false;
        }
    }

    if (!hasFileType) {
        qInfo() << "Migration: Adding file_type column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT DEFAULT 'official'")
                    .arg(
                        Constants::DatabaseSchema::Tables::FILES, Constants::DatabaseSchema::Columns::Files::FILE_TYPE),
                "Migration: Failed to add file_type column to files table")) {
            return false;
        }
    }

    if (!hasIsPatched) {
        qInfo() << "Migration: Adding is_patched column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::IS_PATCHED),
                "Migration: Failed to add is_patched column to files table")) {
            return false;
        }
    }

    if (!hasPatchName) {
        qInfo() << "Migration: Adding patch_name column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::PATCH_NAME),
                "Migration: Failed to add patch_name column to files table")) {
            return false;
        }
    }

    if (!hasIsConverted) {
        qInfo() << "Migration: Adding is_converted column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::IS_CONVERTED),
                "Migration: Failed to add is_converted column to files table")) {
            return false;
        }
    }

    if (!hasIsBundled) {
        qInfo() << "Migration: Adding is_bundled column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 BOOLEAN DEFAULT 0")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::IS_BUNDLED),
                "Migration: Failed to add is_bundled column to files table")) {
            return false;
        }
    }

    if (!hasBundleOutputPath) {
        qInfo() << "Migration: Adding bundle_output_path column to files table";
        if (!execChecked(query,
                QString("ALTER TABLE %1 ADD COLUMN %2 TEXT")
                    .arg(Constants::DatabaseSchema::Tables::FILES,
                        Constants::DatabaseSchema::Columns::Files::BUNDLE_OUTPUT_PATH),
                "Migration: Failed to add bundle_output_path column to files table")) {
            return false;
        }
    }

    if (!execChecked(query,
            QString(R"(
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
    )")
                .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES),
            "Migration: Failed to create applied_patches table")) {
        return false;
    }
    if (!execChecked(query,
            QString("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_sha1 ON %1(output_sha1)")
                .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES),
            "Migration: Failed to create applied_patches SHA1 index")) {
        return false;
    }
    if (!execChecked(query,
            QString("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_md5 ON %1(output_md5)")
                .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES),
            "Migration: Failed to create applied_patches MD5 index")) {
        return false;
    }
    if (!execChecked(query,
            QString("CREATE INDEX IF NOT EXISTS idx_applied_patches_output_crc32 ON %1(output_crc32)")
                .arg(Constants::DatabaseSchema::Tables::APPLIED_PATCHES),
            "Migration: Failed to create applied_patches CRC32 index")) {
        return false;
    }

    if (!migrateCanonicalSystems(m_db, *this, rollbackAndFail)) {
        return false;
    }

    // ── Matches table migrations ──────────────────────────────────────────
    QSet<QString> matchColumns;
    if (!loadTableColumns(m_db, Constants::DatabaseSchema::Tables::MATCHES, matchColumns, columnError)) {
        return rollbackAndFail("Migration: Failed to inspect matches table: " + columnError);
    }
    const bool hasNameMatchScore = matchColumns.contains(Constants::DatabaseSchema::Columns::Matches::NAME_MATCH_SCORE);
    QSqlQuery matchesQuery(m_db);
    if (!hasNameMatchScore) {
        qInfo() << "Migration: Adding name_match_score column to matches table";
        if (!execChecked(matchesQuery,
                QString("ALTER TABLE %1 ADD COLUMN %2 REAL DEFAULT 0")
                    .arg(Constants::DatabaseSchema::Tables::MATCHES,
                        Constants::DatabaseSchema::Columns::Matches::NAME_MATCH_SCORE),
                "Migration: Failed to add name_match_score column to matches table")) {
            return false;
        }
    }

    // ── mod_installations table ───────────────────────────────────────────────
    QSqlQuery modQuery(m_db);
    if (!execChecked(modQuery, QStringLiteral(R"(
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
    )"),
            "Migration: Failed to create mod_installations table")) {
        return false;
    }
    if (!execChecked(modQuery,
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_mod_installations_base ON mod_installations(base_file_id)"),
            "Migration: Failed to create mod_installations base index")) {
        return false;
    }
    if (!execChecked(modQuery,
            QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_mod_installations_catalog ON mod_installations(catalog_mod_id)"),
            "Migration: Failed to create mod_installations catalog index")) {
        return false;
    }

    // ── mod_catalog_cache table ───────────────────────────────────────────────
    QSqlQuery cacheQuery(m_db);
    if (!execChecked(cacheQuery, QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS mod_catalog_cache (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            source_url  TEXT NOT NULL UNIQUE,
            etag        TEXT,
            fetched_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            mod_count   INTEGER DEFAULT 0
        )
    )"),
            "Migration: Failed to create mod_catalog_cache table")) {
        return false;
    }

    // Composite performance indexes — idempotent; safe to add for existing databases.
    query.exec(
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_hash_primary ON files(hash_calculated, is_primary)"));
    query.exec(
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_primary_processed ON files(is_primary, is_processed)"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_games_title_system ON games(title, system_id)"));
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_matches_file_status ON matches(file_id, is_confirmed, is_rejected)"));

    if (useTransaction && !m_db.commit()) {
        logError("Migration: Failed to commit transaction: " + m_db.lastError().text());
        if (!m_db.rollback()) {
            logError("Migration: Failed to roll back transaction after commit failure: " + m_db.lastError().text());
        }
        return false;
    }

    return true;
}

} // namespace Remus
