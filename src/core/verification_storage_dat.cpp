#include "verification_engine.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {

int VerificationEngine::importDat(const QString &datFilePath, const QString &systemName)
{
    DatParser parser;
    const DatParseResult parseResult = parser.parse(datFilePath);
    return importDat(parseResult, systemName);
}

int VerificationEngine::importDat(const DatParseResult &parseResult, const QString &systemName)
{
    if (!parseResult.success) {
        emit error(QString("Failed to parse DAT file: %1").arg(parseResult.error));
        return -1;
    }

    QSqlQuery query(m_database->database());
    query.prepare("DELETE FROM verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);
    if (!query.exec()) {
        emit error(QString("Failed to clear existing DAT entries: %1").arg(query.lastError().text()));
        return -1;
    }

    const QString source = DatParser::detectSource(parseResult.header);
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

    const int datId = query.lastInsertId().toInt();
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
    m_datCache.remove(systemName);

    qInfo() << "Imported" << imported << "entries from DAT:" << parseResult.header.name;
    emit datImportProgress(imported, parseResult.entryCount);

    return imported;
}

int VerificationEngine::importPatchDat(const QString &datFilePath, const QString &systemName)
{
    DatParser parser;
    const DatParseResult parseResult = parser.parse(datFilePath);

    if (!parseResult.success) {
        emit error(QString("Failed to parse patch DAT file: %1").arg(parseResult.error));
        return -1;
    }

    QSqlQuery query(m_database->database());
    query.prepare("DELETE FROM patch_verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);
    if (!query.exec()) {
        emit error(QString("Failed to clear existing patch DAT entries: %1").arg(query.lastError().text()));
        return -1;
    }

    const QString source = DatParser::detectSource(parseResult.header);
    query.prepare(R"(
        INSERT INTO patch_verification_dats
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
        emit error(QString("Failed to insert patch DAT: %1").arg(query.lastError().text()));
        return -1;
    }

    const int datId = query.lastInsertId().toInt();
    m_database->database().transaction();

    int imported = 0;
    for (const DatRomEntry &entry : parseResult.entries) {
        query.prepare(R"(
            INSERT INTO patch_dat_entries
            (dat_id, game_name, rom_name, rom_size, crc32, md5, sha1,
             description, status, base_title, patch_name, file_type)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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
        query.addBindValue(entry.baseTitle);
        query.addBindValue(entry.patchName);
        query.addBindValue(entry.fileType);

        if (query.exec()) {
            imported++;
        }

        if (imported % 100 == 0) {
            emit datImportProgress(imported, parseResult.entryCount);
        }
    }

    m_database->database().commit();
    m_patchDatCache.remove(systemName);

    qInfo() << "Imported" << imported << "entries from patch DAT:" << parseResult.header.name;
    emit datImportProgress(imported, parseResult.entryCount);

    return imported;
}

QMap<QString, DatHeader> VerificationEngine::getImportedDats()
{
    QMap<QString, DatHeader> dats;

    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);
        q.exec(R"(
            SELECT DISTINCT s.internal_name, s.display_name, s.preferred_hash
            FROM systems s
            JOIN games g ON g.system_id = s.system_id
            JOIN game_signatures gs ON gs.game_id = g.game_id
            ORDER BY s.internal_name
        )");
        while (q.next()) {
            DatHeader header;
            const QString sysName = q.value(0).toString();
            header.name    = q.value(1).toString();
            header.category = q.value(2).toString();   // preferred_hash in category slot
            dats.insert(sysName, header);
        }
        if (!dats.isEmpty()) return dats;
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    QSqlQuery query(m_database->database());
    query.exec("SELECT system_name, dat_name, dat_version, dat_source, dat_description FROM verification_dats");
    while (query.next()) {
        DatHeader header;
        const QString systemName = query.value(0).toString();
        header.name = query.value(1).toString();
        header.version = query.value(2).toString();
        header.category = query.value(3).toString();
        header.description = query.value(4).toString();
        dats.insert(systemName, header);
    }

    return dats;
}

QMap<QString, DatHeader> VerificationEngine::getImportedPatchDats()
{
    QMap<QString, DatHeader> dats;

    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);
        q.exec(R"(
            SELECT system_name, catalog_name, catalog_version, catalog_source, catalog_description
            FROM patch_catalog_sources
            ORDER BY system_name
        )");
        while (q.next()) {
            DatHeader header;
            const QString sysName = q.value(0).toString();
            header.name        = q.value(1).toString();
            header.version     = q.value(2).toString();
            header.category    = q.value(3).toString();
            header.description = q.value(4).toString();
            dats.insert(sysName, header);
        }
        if (!dats.isEmpty()) return dats;
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    QSqlQuery query(m_database->database());
    query.exec("SELECT system_name, dat_name, dat_version, dat_source, dat_description FROM patch_verification_dats");
    while (query.next()) {
        DatHeader header;
        const QString systemName = query.value(0).toString();
        header.name = query.value(1).toString();
        header.version = query.value(2).toString();
        header.category = query.value(3).toString();
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

bool VerificationEngine::removePatchDat(const QString &systemName)
{
    QSqlQuery query(m_database->database());
    query.prepare("DELETE FROM patch_verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);

    if (query.exec()) {
        m_patchDatCache.remove(systemName);
        return true;
    }
    return false;
}

bool VerificationEngine::hasDat(const QString &systemName)
{
    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);
        q.prepare(R"(
            SELECT COUNT(DISTINCT gs.signature_id)
            FROM game_signatures gs
            JOIN games g ON gs.game_id = g.game_id
            JOIN systems s ON g.system_id = s.system_id
            WHERE s.internal_name = ?
        )");
        q.addBindValue(systemName);
        if (q.exec() && q.next() && q.value(0).toInt() > 0) {
            return true;
        }
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    QSqlQuery query(m_database->database());
    query.prepare("SELECT COUNT(*) FROM verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

bool VerificationEngine::hasPatchDat(const QString &systemName)
{
    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);
        q.prepare("SELECT COUNT(*) FROM patch_catalog_sources WHERE system_name = ?");
        q.addBindValue(systemName);
        if (q.exec() && q.next() && q.value(0).toInt() > 0) {
            return true;
        }
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    QSqlQuery query(m_database->database());
    query.prepare("SELECT COUNT(*) FROM patch_verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

} // namespace Remus