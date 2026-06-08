#include "database.h"
#include "constants/file_types.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDateTime>

namespace Remus {

bool Database::insertAppliedPatch(const AppliedPatchRecord &record) {
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
    query.addBindValue(record.fileType.isEmpty() ? QVariant(Constants::FileTypes::HACK) : record.fileType);
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
    const QString &crc32, const QString &md5, const QString &sha1) {
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
        )")
            .arg(conditions.join(" OR ")));

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

int Database::insertModInstallation(const ModInstallationRecord &record) {
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"(
        INSERT INTO mod_installations
            (base_file_id, patched_file_id, catalog_mod_id, mod_title,
             mod_author, mod_version, mod_type, patch_format,
             patch_url, patch_sha1, source_url)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(record.baseFileId);
    query.addBindValue(record.patchedFileId > 0 ? record.patchedFileId : QVariant());
    query.addBindValue(record.catalogModId);
    query.addBindValue(record.modTitle);
    query.addBindValue(record.modAuthor);
    query.addBindValue(record.modVersion);
    query.addBindValue(record.modType);
    query.addBindValue(record.patchFormat);
    query.addBindValue(record.patchUrl);
    query.addBindValue(record.patchSha1);
    query.addBindValue(record.sourceUrl);

    if (!query.exec()) {
        logError("Failed to insert mod installation: " + query.lastError().text());
        return -1;
    }
    return query.lastInsertId().toInt();
}

QList<Database::ModInstallationRecord> Database::getModInstallations(int baseFileId) {
    QList<ModInstallationRecord> results;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"(
        SELECT id, base_file_id, patched_file_id, catalog_mod_id, mod_title,
               mod_author, mod_version, mod_type, patch_format,
               patch_url, patch_sha1, source_url, installed_at
        FROM mod_installations
        WHERE base_file_id = ?
        ORDER BY installed_at DESC
    )"));
    query.addBindValue(baseFileId);

    if (!query.exec()) {
        logError("Failed to query mod installations: " + query.lastError().text());
        return results;
    }

    while (query.next()) {
        ModInstallationRecord rec;
        rec.id = query.value(0).toInt();
        rec.baseFileId = query.value(1).toInt();
        rec.patchedFileId = query.value(2).toInt();
        rec.catalogModId = query.value(3).toString();
        rec.modTitle = query.value(4).toString();
        rec.modAuthor = query.value(5).toString();
        rec.modVersion = query.value(6).toString();
        rec.modType = query.value(7).toString();
        rec.patchFormat = query.value(8).toString();
        rec.patchUrl = query.value(9).toString();
        rec.patchSha1 = query.value(10).toString();
        rec.sourceUrl = query.value(11).toString();
        rec.installedAt = query.value(12).toDateTime();
        results.append(rec);
    }
    return results;
}

bool Database::removeModInstallation(int id) {
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM mod_installations WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec()) {
        logError("Failed to remove mod installation: " + query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

int Database::upsertCatalogCache(const ModCatalogCacheRecord &record) {
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"(
        INSERT INTO mod_catalog_cache (source_url, etag, fetched_at, mod_count)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(source_url)
        DO UPDATE SET etag = excluded.etag,
                      fetched_at = excluded.fetched_at,
                      mod_count = excluded.mod_count
    )"));
    query.addBindValue(record.sourceUrl);
    query.addBindValue(record.etag);
    query.addBindValue(record.fetchedAt.isValid() ? record.fetchedAt.toString(Qt::ISODate)
                                                  : QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(record.modCount);

    if (!query.exec()) {
        logError("Failed to upsert catalog cache: " + query.lastError().text());
        return -1;
    }
    return query.lastInsertId().toInt();
}

Database::ModCatalogCacheRecord Database::getCatalogCache(const QString &sourceUrl) {
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(R"(
        SELECT id, source_url, etag, fetched_at, mod_count
        FROM mod_catalog_cache
        WHERE source_url = ?
    )"));
    query.addBindValue(sourceUrl);

    ModCatalogCacheRecord record;
    if (query.exec() && query.next()) {
        record.id = query.value(0).toInt();
        record.sourceUrl = query.value(1).toString();
        record.etag = query.value(2).toString();
        record.fetchedAt = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
        record.modCount = query.value(4).toInt();
    }
    return record;
}

} // namespace Remus
