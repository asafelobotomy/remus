#include "verification_engine.h"
#include "patched_rom_parser.h"
#include "constants/systems.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace Remus {

bool VerificationEngine::createVerificationSchema()
{
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
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_dat_entries_dat_id ON patch_dat_entries(dat_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_patch_verification_dats_system ON patch_verification_dats(system_name)");

    return true;
}

void VerificationEngine::loadDatCache(const QString &systemName)
{
    if (m_datCache.contains(systemName)) {
        return;  // Already loaded
    }

    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT e.game_name, e.rom_name, e.rom_size, e.crc32, e.md5, e.sha1, e.description, e.status
        FROM dat_entries e
        JOIN verification_dats d ON e.dat_id = d.id
        WHERE d.system_name = ?
    )");
    query.addBindValue(systemName);

    if (!query.exec()) {
        qWarning() << "Failed to load DAT cache:" << query.lastError().text();
        return;
    }

    QMap<QString, DatRomEntry> entries;
    QString hashType = getPreferredHashType(systemName);

    while (query.next()) {
        DatRomEntry entry;
        entry.gameName = query.value(0).toString();
        entry.romName = query.value(1).toString();
        entry.size = query.value(2).toLongLong();
        entry.crc32 = query.value(3).toString();
        entry.md5 = query.value(4).toString();
        entry.sha1 = query.value(5).toString();
        entry.description = query.value(6).toString();
        entry.status = query.value(7).toString();

        if (!entry.sha1.isEmpty()) {
            entries.insert(entry.sha1.toLower(), entry);
        }
        if (!entry.md5.isEmpty()) {
            entries.insert(entry.md5.toLower(), entry);
        }
        if (!entry.crc32.isEmpty()) {
            entries.insert(entry.crc32.toLower(), entry);
        }
    }

    m_datCache.insert(systemName, entries);
    m_datHashTypes.insert(systemName, hashType);

    qDebug() << "Loaded" << entries.size() << "DAT entries for" << systemName;
}

void VerificationEngine::loadPatchDatCache(const QString &systemName)
{
    if (m_patchDatCache.contains(systemName)) {
        return;
    }

    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT e.game_name, e.rom_name, e.rom_size, e.crc32, e.md5, e.sha1,
               e.description, e.status, e.base_title, e.patch_name, e.file_type
        FROM patch_dat_entries e
        JOIN patch_verification_dats d ON e.dat_id = d.id
        WHERE d.system_name = ?
    )");
    query.addBindValue(systemName);

    if (!query.exec()) {
        qWarning() << "Failed to load patch DAT cache:" << query.lastError().text();
        return;
    }

    QMap<QString, DatRomEntry> entries;
    while (query.next()) {
        DatRomEntry entry;
        entry.gameName = query.value(0).toString();
        entry.romName = query.value(1).toString();
        entry.size = query.value(2).toLongLong();
        entry.crc32 = query.value(3).toString();
        entry.md5 = query.value(4).toString();
        entry.sha1 = query.value(5).toString();
        entry.description = query.value(6).toString();
        entry.status = query.value(7).toString();
        entry.baseTitle = query.value(8).toString();
        entry.patchName = query.value(9).toString();
        entry.fileType = query.value(10).toString();

        if (!entry.sha1.isEmpty()) {
            entries.insert(entry.sha1.toLower(), entry);
        }
        if (!entry.md5.isEmpty()) {
            entries.insert(entry.md5.toLower(), entry);
        }
        if (!entry.crc32.isEmpty()) {
            entries.insert(entry.crc32.toLower(), entry);
        }
    }

    m_patchDatCache.insert(systemName, entries);
    qDebug() << "Loaded" << entries.size() << "patch DAT entries for" << systemName;
}

QString VerificationEngine::getPreferredHashType(const QString &systemName)
{
    QSqlQuery query(m_database->database());
    query.prepare("SELECT preferred_hash FROM systems WHERE name = ?");
    query.addBindValue(systemName);

    if (query.exec() && query.next()) {
        return query.value(0).toString().toLower();
    }

    // Fallback to Constants::Systems registry if database query fails
    const Constants::Systems::SystemDef* systemDef = Constants::Systems::getSystemByName(systemName);
    if (systemDef) {
        return systemDef->preferredHash.toLower();
    }

    // Ultimate fallback
    return "crc32";
}

} // namespace Remus
