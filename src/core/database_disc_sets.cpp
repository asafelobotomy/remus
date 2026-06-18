#include "database.h"
#include "disc_set_utils.h"

#include <QHash>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <utility>

namespace Remus {

namespace {

    struct DiscSetMeta {
        QString discSetKey;
        int discNumber = 0;
    };

} // namespace

QList<FileRecord> Database::getFilesByDiscSetKey(const QString &discSetKey) {
    if (discSetKey.isEmpty())
        return { };

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, library_id, original_path, current_path, filename, extension, "
        "file_size, is_compressed, archive_path, archive_internal_path, "
        "system_id, crc32, md5, sha1, ra_md5, chd_sha1, rvz_sha1, hash_calculated, "
        "is_primary, parent_file_id, base_title, disc_set_key, disc_number, "
        "file_type, is_patched, patch_name, is_processed, processing_status, "
        "last_modified, scanned_at "
        "FROM files WHERE disc_set_key = ? AND is_primary = 1 "
        "ORDER BY disc_number, filename"));
    query.addBindValue(discSetKey);
    if (!query.exec()) {
        logError("Failed to query files by disc set key: " + query.lastError().text());
        return { };
    }

    QList<FileRecord> files;
    while (query.next()) {
        FileRecord r;
        r.id = query.value(0).toInt();
        r.libraryId = query.value(1).toInt();
        r.originalPath = query.value(2).toString();
        r.currentPath = query.value(3).toString();
        r.filename = query.value(4).toString();
        r.extension = query.value(5).toString();
        r.fileSize = query.value(6).toLongLong();
        r.isCompressed = query.value(7).toBool();
        r.archivePath = query.value(8).toString();
        r.archiveInternalPath = query.value(9).toString();
        r.systemId = query.value(10).toInt();
        r.crc32 = query.value(11).toString();
        r.md5 = query.value(12).toString();
        r.sha1 = query.value(13).toString();
        r.raMd5 = query.value(14).toString();
        r.chdSha1 = query.value(15).toString();
        r.rvzSha1 = query.value(16).toString();
        r.hashCalculated = query.value(17).toBool();
        r.isPrimary = query.value(18).toBool();
        r.parentFileId = query.value(19).toInt();
        r.baseTitle = query.value(20).toString();
        r.discSetKey = query.value(21).toString();
        r.discNumber = query.value(22).toInt();
        r.fileType = query.value(23).toString();
        r.isPatched = query.value(24).toBool();
        r.patchName = query.value(25).toString();
        r.isProcessed = query.value(26).toBool();
        r.processingStatus = query.value(27).toString();
        r.lastModified = query.value(28).toDateTime();
        r.scannedAt = query.value(29).toDateTime();
        if (r.id > 0)
            files.append(r);
    }
    return files;
}

bool Database::rebuildDiscSetsAll() {
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT DISTINCT library_id FROM files"))) {
        logError("Failed to enumerate libraries for disc set rebuild: " + query.lastError().text());
        return false;
    }

    while (query.next()) {
        if (!rebuildDiscSetsForLibrary(query.value(0).toInt()))
            return false;
    }
    return true;
}

bool Database::rebuildDiscSetsForLibrary(int libraryId) {
    if (libraryId <= 0)
        return true;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"(
        SELECT f.id, f.library_id, f.original_path, f.current_path, f.filename, f.extension,
               f.file_size, f.is_compressed, f.archive_path, f.archive_internal_path,
               f.system_id, f.is_primary, f.base_title,
               COALESCE(sys.display_name, '') AS system_name,
               (SELECT m.game_id FROM matches m
                WHERE m.file_id = f.id AND m.is_confirmed = 1 AND m.is_rejected = 0
                LIMIT 1) AS confirmed_game_id
        FROM files f
        LEFT JOIN systems sys ON f.system_id = sys.id
        WHERE f.library_id = ? AND f.is_primary = 1
    )"));
    query.addBindValue(libraryId);
    if (!query.exec()) {
        logError("Failed to load files for disc set rebuild: " + query.lastError().text());
        return false;
    }

    QHash<int, DiscSetMeta> metaByFileId;
    QHash<QPair<int, int>, QList<int>> confirmedGameGroups;

    while (query.next()) {
        FileRecord file;
        file.id = query.value(0).toInt();
        file.libraryId = query.value(1).toInt();
        file.originalPath = query.value(2).toString();
        file.currentPath = query.value(3).toString();
        file.filename = query.value(4).toString();
        file.extension = query.value(5).toString();
        file.fileSize = query.value(6).toLongLong();
        file.isCompressed = query.value(7).toBool();
        file.archivePath = query.value(8).toString();
        file.archiveInternalPath = query.value(9).toString();
        file.systemId = query.value(10).toInt();
        file.isPrimary = query.value(11).toBool();
        file.baseTitle = query.value(12).toString();

        const QString systemName = query.value(13).toString();
        const int confirmedGameId = query.value(14).toInt();

        DiscSetUtils::applyScanDiscMetadata(file, systemName);
        metaByFileId.insert(file.id, { file.discSetKey, file.discNumber });

        if (confirmedGameId > 0 && file.systemId > 0)
            confirmedGameGroups[{ confirmedGameId, file.systemId }].append(file.id);
    }

    for (auto it = confirmedGameGroups.constBegin(); it != confirmedGameGroups.constEnd(); ++it) {
        if (it.value().size() < 2)
            continue;
        const QString gameKey = DiscSetUtils::gameDiscSetKey(it.key().first, it.key().second);
        for (int fileId : it.value()) {
            DiscSetMeta &meta = metaByFileId[fileId];
            meta.discSetKey = gameKey;
        }
    }

    QHash<QString, int> keyCounts;
    for (const DiscSetMeta &meta : metaByFileId) {
        if (!meta.discSetKey.isEmpty())
            ++keyCounts[meta.discSetKey];
    }

    QSqlQuery updateQuery(m_db);
    updateQuery.prepare(QStringLiteral("UPDATE files SET disc_set_key = ?, disc_number = ? WHERE id = ?"));

    for (auto it = metaByFileId.constBegin(); it != metaByFileId.constEnd(); ++it) {
        DiscSetMeta meta = it.value();
        if (meta.discSetKey.isEmpty() || keyCounts.value(meta.discSetKey, 0) < 2) {
            meta.discSetKey.clear();
            meta.discNumber = 0;
        }

        updateQuery.addBindValue(meta.discSetKey.isEmpty() ? QVariant() : meta.discSetKey);
        updateQuery.addBindValue(meta.discNumber);
        updateQuery.addBindValue(it.key());
        if (!updateQuery.exec()) {
            logError("Failed to update disc set metadata: " + updateQuery.lastError().text());
            return false;
        }
    }

    return true;
}

bool Database::reconcileDiscSetForConfirmedFile(int fileId) {
    const FileRecord file = getFileById(fileId);
    if (file.id <= 0)
        return false;
    return rebuildDiscSetsForLibrary(file.libraryId);
}

} // namespace Remus
