#include "verification_engine.h"

#include <QDebug>
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
    query.exec();

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
    query.exec();

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
    QSqlQuery query(m_database->database());
    query.prepare("SELECT COUNT(*) FROM patch_verification_dats WHERE system_name = ?");
    query.addBindValue(systemName);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

} // namespace Remus