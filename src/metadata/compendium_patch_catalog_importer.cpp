#include "compendium_patch_catalog_importer.h"

#include "../core/constants/constants.h"
#include "../core/patched_rom_parser.h"
#include "clrmamepro_parser.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {
namespace CompendiumPatchCatalog {

namespace {

    QString resolveSystemInternalName(QSqlDatabase &database, const QString &libretroName, QString &error) {
        QSqlQuery q(database);
        q.prepare(QStringLiteral("SELECT internal_name FROM systems WHERE libretro_name = ? LIMIT 1"));
        q.addBindValue(libretroName);
        if (!q.exec()) {
            error = q.lastError().text();
            return { };
        }
        if (q.next())
            return q.value(0).toString();
        return libretroName;
    }

    QString inferFileType(const ClrMameProEntry &entry) {
        const PatchedRomInfo parsed = PatchedRomParser::parse(entry.gameName);
        if (!parsed.fileType.isEmpty())
            return parsed.fileType;
        if (entry.gameName.contains(QStringLiteral("[T-"), Qt::CaseInsensitive)
            || entry.gameName.contains(QStringLiteral("Translation"), Qt::CaseInsensitive))
            return Constants::FileTypes::TRANSLATION;
        if (entry.gameName.contains(QStringLiteral("[Hack"), Qt::CaseInsensitive))
            return Constants::FileTypes::HACK;
        return Constants::FileTypes::HACK;
    }

} // namespace

bool importDatFile(
    QSqlDatabase &database, const QString &datPath, const QString &catalogName, ImportStats &stats, QString &error) {
    const QFileInfo fileInfo(datPath);
    if (!fileInfo.isReadable()) {
        error = QStringLiteral("Unreadable patch DAT: %1").arg(datPath);
        return false;
    }

    QMap<QString, QString> header;
    const QList<ClrMameProEntry> entries = ClrMameProParser::parseAll(datPath, header);
    if (entries.isEmpty()) {
        ++stats.filesSkipped;
        return true;
    }

    const QString libretroName = fileInfo.completeBaseName();
    const QString systemName = resolveSystemInternalName(database, libretroName, error);
    if (systemName.isEmpty() && !error.isEmpty())
        return false;

    if (!database.transaction()) {
        error = database.lastError().text();
        return false;
    }

    QSqlQuery deleteEntries(database);
    deleteEntries.prepare(QStringLiteral(
        "DELETE FROM patch_entries WHERE source_id IN "
        "(SELECT source_id FROM patch_catalog_sources WHERE system_name = ? AND catalog_name = ?)"));
    deleteEntries.addBindValue(systemName);
    deleteEntries.addBindValue(catalogName);
    if (!deleteEntries.exec()) {
        error = deleteEntries.lastError().text();
        database.rollback();
        return false;
    }

    QSqlQuery deleteSource(database);
    deleteSource.prepare(
        QStringLiteral("DELETE FROM patch_catalog_sources WHERE system_name = ? AND catalog_name = ?"));
    deleteSource.addBindValue(systemName);
    deleteSource.addBindValue(catalogName);
    if (!deleteSource.exec()) {
        error = deleteSource.lastError().text();
        database.rollback();
        return false;
    }

    QSqlQuery insertSource(database);
    insertSource.prepare(QStringLiteral(
        "INSERT INTO patch_catalog_sources "
        "(system_name, catalog_name, catalog_version, catalog_source, catalog_description, entry_count) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    insertSource.addBindValue(systemName);
    insertSource.addBindValue(catalogName);
    insertSource.addBindValue(header.value(QStringLiteral("version")));
    insertSource.addBindValue(QStringLiteral("libretro-hacks"));
    insertSource.addBindValue(header.value(QStringLiteral("description"), header.value(QStringLiteral("name"))));
    insertSource.addBindValue(entries.size());
    if (!insertSource.exec()) {
        error = insertSource.lastError().text();
        database.rollback();
        return false;
    }

    const int sourceId = insertSource.lastInsertId().toInt();

    QSqlQuery insertEntry(database);
    insertEntry.prepare(QStringLiteral(
        "INSERT INTO patch_entries "
        "(source_id, game_name, rom_name, rom_size, crc32, md5, sha1, description, status, base_title, patch_name, "
        "file_type) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int imported = 0;
    for (const ClrMameProEntry &entry : entries) {
        const PatchedRomInfo parsed = PatchedRomParser::parse(entry.gameName);
        const QString patchName
            = entry.patchUrl.isEmpty() ? parsed.patchName : entry.patchUrl;
        const QString baseTitle = parsed.baseTitle.isEmpty() ? entry.romName : parsed.baseTitle;

        insertEntry.addBindValue(sourceId);
        insertEntry.addBindValue(entry.gameName);
        insertEntry.addBindValue(entry.romName);
        insertEntry.addBindValue(entry.size > 0 ? QVariant(entry.size) : QVariant());
        insertEntry.addBindValue(entry.crc32);
        insertEntry.addBindValue(entry.md5);
        insertEntry.addBindValue(entry.sha1);
        insertEntry.addBindValue(entry.description);
        insertEntry.addBindValue(QString());
        insertEntry.addBindValue(baseTitle);
        insertEntry.addBindValue(patchName);
        insertEntry.addBindValue(inferFileType(entry));
        if (!insertEntry.exec()) {
            error = insertEntry.lastError().text();
            database.rollback();
            return false;
        }
        ++imported;
    }

    if (!database.commit()) {
        error = database.lastError().text();
        database.rollback();
        return false;
    }

    ++stats.sourcesImported;
    stats.entriesImported += imported;
    return true;
}

bool importDirectory(QSqlDatabase &database, const QString &patchDir, ImportStats &stats, QString &error) {
    stats = { };
    error.clear();

    QDir root(patchDir);
    if (!root.exists()) {
        return true;
    }

    const auto datFiles = root.entryInfoList({ QStringLiteral("*.dat") }, QDir::Files, QDir::Name);
    for (const QFileInfo &fileInfo : datFiles) {
        if (!importDatFile(database, fileInfo.absoluteFilePath(), QStringLiteral("hacks"), stats, error))
            return false;
    }

    QDirIterator subdirs(patchDir, QDir::Dirs | QDir::NoDotAndDotDot);
    while (subdirs.hasNext()) {
        subdirs.next();
        const QDir sub(subdirs.filePath());
        const auto nested = sub.entryInfoList({ QStringLiteral("*.dat") }, QDir::Files, QDir::Name);
        for (const QFileInfo &fileInfo : nested) {
            if (!importDatFile(database, fileInfo.absoluteFilePath(), sub.dirName(), stats, error))
                return false;
        }
    }

    return true;
}

} // namespace CompendiumPatchCatalog
} // namespace Remus
