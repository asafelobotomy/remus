#include "verification_engine.h"
#include "patched_rom_parser.h"
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

        VerificationResult result = verifyFile(fd.id);
        switch (result.status) {
        case VerificationStatus::Verified:
            m_lastSummary.verified++;
            break;
        case VerificationStatus::HashMissing:
            m_lastSummary.noHash++;
            break;
        case VerificationStatus::Corrupt:
            m_lastSummary.corrupt++;
            break;
        case VerificationStatus::Mismatch:
            m_lastSummary.mismatched++;
            break;
        case VerificationStatus::NotInDat:
            m_lastSummary.notInDat++;
            break;
        default:
            break;
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

    const bool hasOfficialDat = hasDat(result.system);
    const bool hasPatchCatalog = hasPatchDat(result.system);
    if (!hasOfficialDat && !hasPatchCatalog) {
        result.status = VerificationStatus::NotInDat;
        result.notes = "No verification catalog for " + result.system;
        return result;
    }

    if (hasOfficialDat) {
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
            result.notes = "Verified against official DAT";
            return result;
        }
    }

    DatRomEntry patchEntry;
    QString matchedHash;
    QString matchedHashType;
    if (hasPatchCatalog && findPatchCatalogMatch(result.system, crc32, md5, sha1,
                                                 patchEntry, matchedHash, matchedHashType)) {
        result.status = VerificationStatus::Verified;
        result.datName = patchEntry.gameName;
        result.datRomName = patchEntry.romName;
        result.datDescription = patchEntry.description;
        result.hashType = matchedHashType;
        result.fileHash = matchedHash;
        result.datHash = matchedHash;
        result.notes = patchEntry.patchName.isEmpty()
            ? QStringLiteral("Verified against patch catalog")
            : QStringLiteral("Verified against patch catalog: %1").arg(patchEntry.patchName);
        promotePatchMetadata(fileId, patchEntry);
    } else {
        result.status = VerificationStatus::NotInDat;
        result.notes = "Hash not found in verification catalogs";
    }

    return result;
}

bool VerificationEngine::findPatchCatalogMatch(const QString &systemName,
                                               const QString &crc32,
                                               const QString &md5,
                                               const QString &sha1,
                                               DatRomEntry &matchedEntry,
                                               QString &matchedHash,
                                               QString &matchedHashType)
{
    if (!hasPatchDat(systemName)) {
        return false;
    }

    loadPatchDatCache(systemName);
    const auto &patchEntries = m_patchDatCache.value(systemName);

    struct CandidateHash {
        QString type;
        QString value;
    };
    const QList<CandidateHash> candidates = {
        {QStringLiteral("sha1"), sha1.toLower()},
        {QStringLiteral("md5"), md5.toLower()},
        {QStringLiteral("crc32"), crc32.toLower()}
    };

    for (const CandidateHash &candidate : candidates) {
        if (!candidate.value.isEmpty() && patchEntries.contains(candidate.value)) {
            matchedEntry = patchEntries.value(candidate.value);
            matchedHash = candidate.value;
            matchedHashType = candidate.type;
            return true;
        }
    }

    return false;
}

void VerificationEngine::promotePatchMetadata(int fileId, const DatRomEntry &entry)
{
    DatRomEntry metadataEntry = entry;
    if (metadataEntry.baseTitle.isEmpty() || metadataEntry.patchName.isEmpty() || metadataEntry.fileType.isEmpty()) {
        const PatchedRomInfo parsed = PatchedRomParser::parse(metadataEntry.gameName);
        if (metadataEntry.baseTitle.isEmpty()) {
            metadataEntry.baseTitle = parsed.baseTitle;
        }
        if (metadataEntry.patchName.isEmpty()) {
            metadataEntry.patchName = parsed.patchName;
        }
        if (metadataEntry.fileType.isEmpty() || metadataEntry.fileType == QStringLiteral("official")) {
            metadataEntry.fileType = parsed.fileType;
        }
    }

    const bool markPatched = !metadataEntry.patchName.isEmpty() ||
        metadataEntry.fileType == QStringLiteral("translation") ||
        metadataEntry.fileType == QStringLiteral("hack");

    QSqlQuery query(m_database->database());
    query.prepare(R"(
        UPDATE files
        SET base_title = COALESCE(NULLIF(?, ''), base_title),
            file_type = COALESCE(NULLIF(?, ''), file_type),
            is_patched = CASE WHEN ? THEN 1 ELSE is_patched END,
            patch_name = COALESCE(NULLIF(?, ''), patch_name)
        WHERE id = ?
    )");
    query.addBindValue(metadataEntry.baseTitle);
    query.addBindValue(metadataEntry.fileType);
    query.addBindValue(markPatched);
    query.addBindValue(metadataEntry.patchName);
    query.addBindValue(fileId);

    if (!query.exec()) {
        qWarning() << "Failed to promote patch metadata:" << query.lastError().text();
    }
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
