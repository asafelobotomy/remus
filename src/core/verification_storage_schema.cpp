#include "verification_engine.h"
#include "patched_rom_parser.h"
#include "compendium_sql_pragmas.h"
#include "constants/systems.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace Remus {

void VerificationEngine::setCompendiumDb(const QString &compendiumDbPath) {
    // Switching catalog sources invalidates in-memory DAT caches.
    m_datCache.clear();
    m_patchDatCache.clear();
    m_datHashTypes.clear();

    const QString targetConnection = QString("compendium_verify_%1").arg(reinterpret_cast<quintptr>(this));

    // Allow explicit detach by passing an empty path.
    if (compendiumDbPath.trimmed().isEmpty()) {
        if (QSqlDatabase::contains(targetConnection)) {
            {
                QSqlDatabase existing = QSqlDatabase::database(targetConnection, false);
                if (existing.isValid() && existing.isOpen()) {
                    existing.close();
                }
            }
            QSqlDatabase::removeDatabase(targetConnection);
        }
        m_compendiumConnectionName.clear();
        return;
    }

    if (QSqlDatabase::contains(targetConnection)) {
        {
            QSqlDatabase existing = QSqlDatabase::database(targetConnection, false);
            if (existing.isValid() && existing.isOpen()) {
                existing.close();
            }
        }
        QSqlDatabase::removeDatabase(targetConnection);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), targetConnection);
    db.setDatabaseName(compendiumDbPath);
    if (!db.open()) {
        qWarning() << "VerificationEngine: failed to open compendium DB:" << compendiumDbPath;
        m_compendiumConnectionName.clear();
        return;
    }

    CompendiumSql::applyReadOnlyPragmas(db);

    m_compendiumConnectionName = targetConnection;
    qDebug() << "VerificationEngine: compendium DB attached:" << compendiumDbPath;
}

bool VerificationEngine::createVerificationSchema() {
    QSqlQuery query(m_database->database());

    // Create verification_dats table
    QString createDats = R"(
        CREATE TABLE IF NOT EXISTS verification_dats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            system_name TEXT NOT NULL,
            dat_name TEXT NOT NULL,
            dat_version TEXT,
            dat_source TEXT,
            dat_description TEXT,
            entry_count INTEGER DEFAULT 0,
            imported_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(system_name)
        )
    )";
    if (!query.exec(createDats)) {
        qWarning() << "Failed to create verification_dats table:" << query.lastError().text();
        return false;
    }

    // Create dat_entries table (stores parsed DAT entries)
    QString createEntries = R"(
        CREATE TABLE IF NOT EXISTS dat_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            dat_id INTEGER NOT NULL,
            game_name TEXT NOT NULL,
            rom_name TEXT NOT NULL,
            rom_size INTEGER,
            crc32 TEXT,
            md5 TEXT,
            sha1 TEXT,
            sha256 TEXT,
            description TEXT,
            status TEXT,
            FOREIGN KEY (dat_id) REFERENCES verification_dats(id) ON DELETE CASCADE
        )
    )";
    if (!query.exec(createEntries)) {
        qWarning() << "Failed to create dat_entries table:" << query.lastError().text();
        return false;
    }

    // Create indexes for fast hash lookup
    query.exec("CREATE INDEX IF NOT EXISTS idx_dat_entries_crc32 ON dat_entries(crc32)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_dat_entries_md5 ON dat_entries(md5)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_dat_entries_sha1 ON dat_entries(sha1)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_dat_entries_sha256 ON dat_entries(sha256)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_dat_entries_dat_id ON dat_entries(dat_id)");

    // Create verification_results table
    QString createResults = R"(
        CREATE TABLE IF NOT EXISTS verification_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id INTEGER NOT NULL,
            dat_id INTEGER,
            status TEXT NOT NULL,
            matched_entry_id INTEGER,
            hash_type TEXT,
            file_hash TEXT,
            dat_hash TEXT,
            header_stripped BOOLEAN DEFAULT 0,
            verified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            notes TEXT,
            FOREIGN KEY (file_id) REFERENCES files(id) ON DELETE CASCADE,
            FOREIGN KEY (dat_id) REFERENCES verification_dats(id) ON DELETE SET NULL,
            FOREIGN KEY (matched_entry_id) REFERENCES dat_entries(id) ON DELETE SET NULL
        )
    )";
    if (!query.exec(createResults)) {
        qWarning() << "Failed to create verification_results table:" << query.lastError().text();
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_verification_results_file ON verification_results(file_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_verification_results_status ON verification_results(status)");

    QString createPatchDats = R"(
        CREATE TABLE IF NOT EXISTS patch_verification_dats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            system_name TEXT NOT NULL,
            dat_name TEXT NOT NULL,
            dat_version TEXT,
            dat_source TEXT,
            dat_description TEXT,
            entry_count INTEGER DEFAULT 0,
            imported_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(system_name)
        )
    )";
    if (!query.exec(createPatchDats)) {
        qWarning() << "Failed to create patch_verification_dats table:" << query.lastError().text();
        return false;
    }

    QString createPatchEntries = R"(
        CREATE TABLE IF NOT EXISTS patch_dat_entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            dat_id INTEGER NOT NULL,
            game_name TEXT NOT NULL,
            rom_name TEXT NOT NULL,
            rom_size INTEGER,
            crc32 TEXT,
            md5 TEXT,
            sha1 TEXT,
            sha256 TEXT,
            description TEXT,
            status TEXT,
            base_title TEXT,
            patch_name TEXT,
            file_type TEXT,
            FOREIGN KEY (dat_id) REFERENCES patch_verification_dats(id) ON DELETE CASCADE
        )
    )";
    if (!query.exec(createPatchEntries)) {
        qWarning() << "Failed to create patch_dat_entries table:" << query.lastError().text();
        return false;
    }

    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_dat_entries_crc32 ON patch_dat_entries(crc32)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_dat_entries_md5 ON patch_dat_entries(md5)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_dat_entries_sha1 ON patch_dat_entries(sha1)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_dat_entries_sha256 ON patch_dat_entries(sha256)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_dat_entries_dat_id ON patch_dat_entries(dat_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_verification_dats_system ON patch_verification_dats(system_name)");

    return true;
}

} // namespace Remus
