#include "verification_engine.h"
#include "verification_hash_matcher.h"
#include "compendium_disc_bridge.h"

#include "patched_rom_parser.h"
#include "constants/file_types.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>

namespace Remus {
using namespace VerificationHashMatcher;

namespace {

    void appendDiscCatalogNotes(const QString &compendiumConnectionName, VerificationResult &result,
        const QString &systemName, const QString &crc32, const QString &md5, const QString &sha1,
        int libraryDiscNumber) {
        if (compendiumConnectionName.isEmpty())
            return;

        QSqlDatabase compendiumDb = QSqlDatabase::database(compendiumConnectionName);
        if (!compendiumDb.isOpen())
            return;

        CompendiumFileDiscContext ctx;
        if (!lookupCompendiumDiscContextFromDb(compendiumDb, systemName, crc32, md5, sha1, ctx))
            return;

        if (libraryDiscNumber > 0 && ctx.discNumber > 0 && libraryDiscNumber != ctx.discNumber) {
            if (!result.notes.isEmpty())
                result.notes += QStringLiteral("; ");
            result.notes += QStringLiteral("Catalog disc %1 but library disc_number is %2")
                                .arg(ctx.discNumber)
                                .arg(libraryDiscNumber);
        }
    }

} // namespace

QList<VerificationResult> VerificationEngine::verifyLibrary(const QString &systemFilter) {
    QList<VerificationResult> results;
    m_lastSummary = VerificationSummary();

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

    struct FileData {
        int id;
        QString filename;
    };
    QList<FileData> files;
    while (query.next()) {
        files.append({
            query.value(0).toInt(),
            query.value(2).toString(),
        });
    }

    m_lastSummary.totalFiles = files.size();
    int current = 0;
    for (const FileData &fd : files) {
        current++;
        emit verificationProgress(current, files.size(), fd.filename);

        const VerificationResult result = verifyFile(fd.id);
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

    if (!systemFilter.isEmpty() && hasDat(systemFilter)) {
        const auto dats = getImportedDats();
        if (dats.contains(systemFilter)) {
            m_lastSummary.datName = dats.value(systemFilter).name;
            m_lastSummary.datVersion = dats.value(systemFilter).version;
            m_lastSummary.datSource = dats.value(systemFilter).category;
        }
    }

    emit verificationComplete(m_lastSummary);
    return results;
}

QList<VerificationResult> VerificationEngine::verifyFiles(const QList<int> &fileIds) {
    QList<VerificationResult> results;
    for (int fileId : fileIds) {
        results.append(verifyFile(fileId));
    }
    return results;
}

VerificationResult VerificationEngine::verifyFile(int fileId) {
    VerificationResult result;
    result.fileId = fileId;

    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT f.current_path, f.filename, s.name, f.crc32, f.md5, f.sha1, f.hash_calculated, f.disc_number
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
    const QString crc32 = query.value(3).toString();
    const QString md5 = query.value(4).toString();
    const QString sha1 = query.value(5).toString();
    const bool hashCalculated = query.value(6).toBool();
    const int libraryDiscNumber = query.value(7).toInt();

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
        const auto &datEntries = m_datCache.value(result.system);
        DatRomEntry matchedEntry;
        QString matchedHash;
        QString matchedHashType;
        if (findOfficialDatMatch(datEntries, m_datHashTypes.value(result.system, QStringLiteral("crc32")), crc32, md5,
                sha1, QStringLiteral(""), matchedEntry, matchedHash, matchedHashType)) {
            result.status = VerificationStatus::Verified;
            result.datName = matchedEntry.gameName;
            result.datRomName = matchedEntry.romName;
            result.datDescription = matchedEntry.description;
            result.hashType = matchedHashType;
            result.fileHash = matchedHash;
            result.datHash = matchedHash;
            result.notes = "Verified against official DAT";
            appendDiscCatalogNotes(m_compendiumConnectionName, result, result.system, crc32, md5, sha1,
                libraryDiscNumber);
            return result;
        }
    }

    DatRomEntry patchEntry;
    QString matchedHash;
    QString matchedHashType;
    if (hasPatchCatalog
        && findPatchCatalogMatch(
            result.system, crc32, md5, sha1, QStringLiteral(""), patchEntry, matchedHash, matchedHashType)) {
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
        appendDiscCatalogNotes(m_compendiumConnectionName, result, result.system, crc32, md5, sha1,
            libraryDiscNumber);
    } else {
        result.status = VerificationStatus::NotInDat;
        result.notes = "Hash not found in verification catalogs";
    }

    return result;
}

bool VerificationEngine::findPatchCatalogMatch(const QString &systemName, const QString &crc32, const QString &md5,
    const QString &sha1, const QString &sha256, DatRomEntry &matchedEntry, QString &matchedHash,
    QString &matchedHashType) {
    if (!hasPatchDat(systemName)) {
        return false;
    }

    loadPatchDatCache(systemName);
    const auto &patchEntries = m_patchDatCache.value(systemName);

    return findHashInDatEntries(patchEntries, m_datHashTypes.value(systemName, QStringLiteral("crc32")), crc32, md5,
        sha1, sha256, matchedEntry, matchedHash, matchedHashType);
}

void VerificationEngine::promotePatchMetadata(int fileId, const DatRomEntry &entry) {
    DatRomEntry metadataEntry = entry;
    if (metadataEntry.baseTitle.isEmpty() || metadataEntry.patchName.isEmpty() || metadataEntry.fileType.isEmpty()) {
        const PatchedRomInfo parsed = PatchedRomParser::parse(metadataEntry.gameName);
        if (metadataEntry.baseTitle.isEmpty()) {
            metadataEntry.baseTitle = parsed.baseTitle;
        }
        if (metadataEntry.patchName.isEmpty()) {
            metadataEntry.patchName = parsed.patchName;
        }
        if (metadataEntry.fileType.isEmpty() || Constants::FileTypes::isOfficial(metadataEntry.fileType)) {
            metadataEntry.fileType = parsed.fileType;
        }
    }

    const bool markPatched = !metadataEntry.patchName.isEmpty()
        || metadataEntry.fileType == Constants::FileTypes::TRANSLATION
        || metadataEntry.fileType == Constants::FileTypes::HACK;

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

QList<DatRomEntry> VerificationEngine::getMissingGames(const QString &systemName) {
    QList<DatRomEntry> missing;
    if (!hasDat(systemName)) {
        return missing;
    }

    // Collect hashes already present in the library for this system
    QSet<QString> verifiedHashes;
    {
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
                for (int col = 0; col < 3; ++col) {
                    const QString h = query.value(col).toString();
                    if (!h.isEmpty())
                        verifiedHashes.insert(h);
                }
            }
        }
    }

    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);
        q.prepare(R"(
            SELECT g.canonical_title,
                   MAX(CASE WHEN gs.hash_type='crc32' THEN gs.hash_value ELSE NULL END) AS crc32,
                   MAX(CASE WHEN gs.hash_type='md5'   THEN gs.hash_value ELSE NULL END) AS md5,
                   MAX(CASE WHEN gs.hash_type='sha1'  THEN gs.hash_value ELSE NULL END) AS sha1
            FROM games g
            JOIN systems s ON g.system_id = s.system_id
            JOIN game_signatures gs ON gs.game_id = g.game_id
            WHERE s.internal_name = ?
            GROUP BY g.game_id
        )");
        q.addBindValue(systemName);

        if (q.exec()) {
            while (q.next()) {
                const QString crc32 = q.value(1).toString().toLower();
                const QString md5 = q.value(2).toString().toLower();
                const QString sha1 = q.value(3).toString().toLower();

                const bool found = (!crc32.isEmpty() && verifiedHashes.contains(crc32))
                    || (!md5.isEmpty() && verifiedHashes.contains(md5))
                    || (!sha1.isEmpty() && verifiedHashes.contains(sha1));
                if (!found) {
                    DatRomEntry entry;
                    entry.gameName = q.value(0).toString();
                    entry.crc32 = crc32;
                    entry.md5 = md5;
                    entry.sha1 = sha1;
                    missing.append(entry);
                }
            }
            return missing;
        }
        qWarning() << "VerificationEngine: compendium getMissingGames query failed:" << q.lastError().text();
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    loadDatCache(systemName);
    const auto &datEntries = m_datCache.value(systemName);

    QSet<QString> seenEntries;
    for (auto it = datEntries.begin(); it != datEntries.end(); ++it) {
        const DatRomEntry &entry = it.value();
        const QString entryKey = datEntryKey(entry);
        if (seenEntries.contains(entryKey)) {
            continue;
        }
        seenEntries.insert(entryKey);

        const bool found = verifiedHashes.contains(entry.crc32.toLower())
            || verifiedHashes.contains(entry.md5.toLower()) || verifiedHashes.contains(entry.sha1.toLower());
        if (!found) {
            missing.append(entry);
        }
    }

    return missing;
}

} // namespace Remus