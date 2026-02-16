#include "verification_engine.h"
#include "header_detector.h"
#include "hasher.h"
#include "constants/systems.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QDebug>

namespace Remus {

VerificationEngine::VerificationEngine(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    createVerificationSchema();
}

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

    return true;
}

int VerificationEngine::importDat(const QString &datFilePath, const QString &systemName)
{
    DatParser parser;
    DatParseResult parseResult = parser.parse(datFilePath);

    if (!parseResult.success) {
        emit error(QString("Failed to parse DAT file: %1").arg(parseResult.error));
        return -1;
    }

    QSqlQuery query(m_database->database());

    // Delete existing DAT for this system
    query.prepare("DELETE FROM verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);
    query.exec();

    // Insert new DAT record
    QString source = DatParser::detectSource(parseResult.header);
    query.prepare(R"(
        INSERT INTO verification_dats 
        (system_name, dat_name, dat_version, dat_source, dat_description, entry_count)
        VALUES (?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(systemName);
    query.addBindValue(parseResult.header.name);
    query.addBindValue(parseResult.header.version);
    query.addBindValue(source);
    query.addBindValue(parseResult.header.description);
    query.addBindValue(parseResult.entryCount);

    if (!query.exec()) {
        emit error(QString("Failed to insert DAT: %1").arg(query.lastError().text()));
        return -1;
    }

    int datId = query.lastInsertId().toInt();

    // Insert all entries
    m_database->database().transaction();

    int imported = 0;
    for (const DatRomEntry &entry : parseResult.entries) {
        query.prepare(R"(
            INSERT INTO dat_entries 
            (dat_id, game_name, rom_name, rom_size, crc32, md5, sha1, description, status)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");
        query.addBindValue(datId);
        query.addBindValue(entry.gameName);
        query.addBindValue(entry.romName);
        query.addBindValue(entry.size);
        query.addBindValue(entry.crc32);
        query.addBindValue(entry.md5);
        query.addBindValue(entry.sha1);
        query.addBindValue(entry.description);
        query.addBindValue(entry.status);

        if (query.exec()) {
            imported++;
        }

        if (imported % 100 == 0) {
            emit datImportProgress(imported, parseResult.entryCount);
        }
    }

    m_database->database().commit();

    // Clear cache for this system (will reload on next verify)
    m_datCache.remove(systemName);

    qInfo() << "Imported" << imported << "entries from DAT:" << parseResult.header.name;
    emit datImportProgress(imported, parseResult.entryCount);

    return imported;
}

QMap<QString, DatHeader> VerificationEngine::getImportedDats()
{
    QMap<QString, DatHeader> dats;
    QSqlQuery query(m_database->database());

    query.exec("SELECT system_name, dat_name, dat_version, dat_source, dat_description FROM verification_dats");
    
    while (query.next()) {
        DatHeader header;
        QString systemName = query.value(0).toString();
        header.name = query.value(1).toString();
        header.version = query.value(2).toString();
        header.category = query.value(3).toString();  // Using category for source
        header.description = query.value(4).toString();
        dats.insert(systemName, header);
    }

    return dats;
}

bool VerificationEngine::removeDat(const QString &systemName)
{
    QSqlQuery query(m_database->database());
    query.prepare("DELETE FROM verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);
    
    if (query.exec()) {
        m_datCache.remove(systemName);
        return true;
    }
    return false;
}

bool VerificationEngine::hasDat(const QString &systemName)
{
    QSqlQuery query(m_database->database());
    query.prepare("SELECT COUNT(*) FROM verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);
    
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
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

        // Index by preferred hash type
        QString hash;
        if (hashType == "sha1" && !entry.sha1.isEmpty()) {
            hash = entry.sha1;
        } else if (hashType == "md5" && !entry.md5.isEmpty()) {
            hash = entry.md5;
        } else if (!entry.crc32.isEmpty()) {
            hash = entry.crc32;
        }

        if (!hash.isEmpty()) {
            entries.insert(hash.toLower(), entry);
        }
    }

    m_datCache.insert(systemName, entries);
    m_datHashTypes.insert(systemName, hashType);

    qDebug() << "Loaded" << entries.size() << "DAT entries for" << systemName;
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

QList<VerificationResult> VerificationEngine::verifyLibrary(const QString &systemFilter)
{
    QList<VerificationResult> results;
    m_lastSummary = VerificationSummary();

    // Get files to verify
    QSqlQuery query(m_database->database());
    QString sql = R"(
        SELECT f.id, f.current_path, f.filename, s.name as system_name,
               f.crc32, f.md5, f.sha1, f.hash_calculated
        FROM files f
        LEFT JOIN systems s ON f.system_id = s.id
        WHERE f.is_primary = 1
    )";

    if (!systemFilter.isEmpty()) {
        sql += " AND s.name = ?";
    }

    query.prepare(sql);
    if (!systemFilter.isEmpty()) {
        query.addBindValue(systemFilter);
    }

    if (!query.exec()) {
        emit error("Failed to query files: " + query.lastError().text());
        return results;
    }

    // Collect file data first
    struct FileData {
        int id;
        QString path;
        QString filename;
        QString system;
        QString crc32;
        QString md5;
        QString sha1;
        bool hashCalculated;
    };
    QList<FileData> files;

    while (query.next()) {
        FileData fd;
        fd.id = query.value(0).toInt();
        fd.path = query.value(1).toString();
        fd.filename = query.value(2).toString();
        fd.system = query.value(3).toString();
        fd.crc32 = query.value(4).toString();
        fd.md5 = query.value(5).toString();
        fd.sha1 = query.value(6).toString();
        fd.hashCalculated = query.value(7).toBool();
        files.append(fd);
    }

    m_lastSummary.totalFiles = files.size();

    // Verify each file
    int current = 0;
    for (const FileData &fd : files) {
        current++;
        emit verificationProgress(current, files.size(), fd.filename);

        // Ensure DAT is loaded for this system
        if (!fd.system.isEmpty() && hasDat(fd.system)) {
            loadDatCache(fd.system);
        }

        VerificationResult result;
        result.fileId = fd.id;
        result.filePath = fd.path;
        result.filename = fd.filename;
        result.system = fd.system;

        // Check if hash exists
        if (!fd.hashCalculated) {
            result.status = VerificationStatus::HashMissing;
            result.notes = "Hash not calculated";
            m_lastSummary.noHash++;
            results.append(result);
            continue;
        }

        // Check if we have a DAT for this system
        if (fd.system.isEmpty() || !m_datCache.contains(fd.system)) {
            result.status = VerificationStatus::NotInDat;
            result.notes = "No DAT file for system";
            m_lastSummary.notInDat++;
            results.append(result);
            continue;
        }

        // Get preferred hash type and look up
        QString hashType = m_datHashTypes.value(fd.system, "crc32");
        QString fileHash;
        if (hashType == "sha1") {
            fileHash = fd.sha1.toLower();
        } else if (hashType == "md5") {
            fileHash = fd.md5.toLower();
        } else {
            fileHash = fd.crc32.toLower();
        }

        result.hashType = hashType;
        result.fileHash = fileHash;

        // Look up in DAT
        const auto &datEntries = m_datCache.value(fd.system);
        if (datEntries.contains(fileHash)) {
            const DatRomEntry &entry = datEntries.value(fileHash);
            result.status = VerificationStatus::Verified;
            result.datName = entry.gameName;
            result.datRomName = entry.romName;
            result.datDescription = entry.description;
            result.datHash = fileHash;
            m_lastSummary.verified++;
        } else {
            result.status = VerificationStatus::NotInDat;
            result.notes = "Hash not found in DAT";
            m_lastSummary.notInDat++;
        }

        results.append(result);
    }

    // Set summary info
    if (!systemFilter.isEmpty() && hasDat(systemFilter)) {
        auto dats = getImportedDats();
        if (dats.contains(systemFilter)) {
            m_lastSummary.datName = dats.value(systemFilter).name;
            m_lastSummary.datVersion = dats.value(systemFilter).version;
            m_lastSummary.datSource = dats.value(systemFilter).category;
        }
    }

    emit verificationComplete(m_lastSummary);
    return results;
}

QList<VerificationResult> VerificationEngine::verifyFiles(const QList<int> &fileIds)
{
    QList<VerificationResult> results;
    
    for (int fileId : fileIds) {
        results.append(verifyFile(fileId));
    }

    return results;
}

VerificationResult VerificationEngine::verifyFile(int fileId)
{
    VerificationResult result;
    result.fileId = fileId;

    // Get file info from database
    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT f.current_path, f.filename, s.name, f.crc32, f.md5, f.sha1, f.hash_calculated
        FROM files f
        LEFT JOIN systems s ON f.system_id = s.id
        WHERE f.id = ?
    )");
    query.addBindValue(fileId);

    if (!query.exec() || !query.next()) {
        result.status = VerificationStatus::Unknown;
        result.notes = "File not found in database";
        return result;
    }

    result.filePath = query.value(0).toString();
    result.filename = query.value(1).toString();
    result.system = query.value(2).toString();
    QString crc32 = query.value(3).toString();
    QString md5 = query.value(4).toString();
    QString sha1 = query.value(5).toString();
    bool hashCalculated = query.value(6).toBool();

    if (!hashCalculated) {
        result.status = VerificationStatus::HashMissing;
        return result;
    }

    if (!hasDat(result.system)) {
        result.status = VerificationStatus::NotInDat;
        result.notes = "No DAT file for " + result.system;
        return result;
    }

    loadDatCache(result.system);

    QString hashType = m_datHashTypes.value(result.system, "crc32");
    QString fileHash;
    if (hashType == "sha1") {
        fileHash = sha1.toLower();
    } else if (hashType == "md5") {
        fileHash = md5.toLower();
    } else {
        fileHash = crc32.toLower();
    }

    result.hashType = hashType;
    result.fileHash = fileHash;

    const auto &datEntries = m_datCache.value(result.system);
    if (datEntries.contains(fileHash)) {
        const DatRomEntry &entry = datEntries.value(fileHash);
        result.status = VerificationStatus::Verified;
        result.datName = entry.gameName;
        result.datRomName = entry.romName;
        result.datDescription = entry.description;
        result.datHash = fileHash;
    } else {
        result.status = VerificationStatus::NotInDat;
    }

    return result;
}

QList<DatRomEntry> VerificationEngine::getMissingGames(const QString &systemName)
{
    QList<DatRomEntry> missing;

    if (!hasDat(systemName)) {
        return missing;
    }

    loadDatCache(systemName);
    const auto &datEntries = m_datCache.value(systemName);

    // Get all verified hashes for this system
    QSet<QString> verifiedHashes;
    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT LOWER(f.crc32), LOWER(f.md5), LOWER(f.sha1)
        FROM files f
        JOIN systems s ON f.system_id = s.id
        WHERE s.name = ? AND f.hash_calculated = 1
    )");
    query.addBindValue(systemName);

    if (query.exec()) {
        while (query.next()) {
            verifiedHashes.insert(query.value(0).toString());
            verifiedHashes.insert(query.value(1).toString());
            verifiedHashes.insert(query.value(2).toString());
        }
    }

    // Find entries not in library
    for (auto it = datEntries.begin(); it != datEntries.end(); ++it) {
        const DatRomEntry &entry = it.value();
        bool found = verifiedHashes.contains(entry.crc32.toLower()) ||
                     verifiedHashes.contains(entry.md5.toLower()) ||
                     verifiedHashes.contains(entry.sha1.toLower());
        
        if (!found) {
            missing.append(entry);
        }
    }

    return missing;
}

bool VerificationEngine::exportReport(const QList<VerificationResult> &results,
                                       const QString &outputPath,
                                       const QString &format)
{
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit error("Failed to create report file: " + outputPath);
        return false;
    }

    if (format == "json") {
        QJsonArray jsonResults;
        for (const VerificationResult &r : results) {
            QJsonObject obj;
            obj["fileId"] = r.fileId;
            obj["filePath"] = r.filePath;
            obj["filename"] = r.filename;
            obj["system"] = r.system;
            
            QString statusStr;
            switch (r.status) {
                case VerificationStatus::Verified: statusStr = "verified"; break;
                case VerificationStatus::Mismatch: statusStr = "mismatch"; break;
                case VerificationStatus::NotInDat: statusStr = "not_in_dat"; break;
                case VerificationStatus::HashMissing: statusStr = "hash_missing"; break;
                case VerificationStatus::Corrupt: statusStr = "corrupt"; break;
                default: statusStr = "unknown"; break;
            }
            obj["status"] = statusStr;
            obj["datName"] = r.datName;
            obj["datRomName"] = r.datRomName;
            obj["hashType"] = r.hashType;
            obj["fileHash"] = r.fileHash;
            obj["datHash"] = r.datHash;
            obj["notes"] = r.notes;

            jsonResults.append(obj);
        }

        QJsonDocument doc(jsonResults);
        file.write(doc.toJson(QJsonDocument::Indented));
    } else {
        // CSV format
        QTextStream out(&file);
        out << "File ID,Filename,System,Status,DAT Name,Hash Type,File Hash,DAT Hash,Notes\n";

        for (const VerificationResult &r : results) {
            QString statusStr;
            switch (r.status) {
                case VerificationStatus::Verified: statusStr = "Verified"; break;
                case VerificationStatus::Mismatch: statusStr = "Mismatch"; break;
                case VerificationStatus::NotInDat: statusStr = "Not in DAT"; break;
                case VerificationStatus::HashMissing: statusStr = "Hash Missing"; break;
                case VerificationStatus::Corrupt: statusStr = "Corrupt"; break;
                default: statusStr = "Unknown"; break;
            }

            // Escape CSV fields
            auto escape = [](QString s) {
                if (s.contains(',') || s.contains('"') || s.contains('\n')) {
                    s.replace("\"", "\"\"");
                    return "\"" + s + "\"";
                }
                return s;
            };

            out << r.fileId << ","
                << escape(r.filename) << ","
                << escape(r.system) << ","
                << statusStr << ","
                << escape(r.datName) << ","
                << r.hashType << ","
                << r.fileHash << ","
                << r.datHash << ","
                << escape(r.notes) << "\n";
        }
    }

    file.close();
    return true;
}

} // namespace Remus
