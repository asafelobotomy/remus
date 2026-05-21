#include "verification_engine.h"
#include "patched_rom_parser.h"
#include "constants/systems.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace Remus {

void VerificationEngine::loadDatCache(const QString &systemName)
{
    if (m_datCache.contains(systemName)) {
        return;  // Already loaded
    }

    QMap<QString, DatRomEntry> entries;
    QString hashType;

    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);

        // Preferred hash for this system
        q.prepare("SELECT preferred_hash FROM systems WHERE internal_name = ?");
        q.addBindValue(systemName);
        if (q.exec() && q.next()) {
            hashType = q.value(0).toString().toLower();
        }

        // Aggregate all hashes per game from game_signatures
        q.prepare(R"(
            SELECT g.canonical_title,
                   MAX(CASE WHEN gs.hash_type='crc32' THEN gs.hash_value ELSE NULL END) AS crc32,
                   MAX(CASE WHEN gs.hash_type='md5'   THEN gs.hash_value ELSE NULL END) AS md5,
                   MAX(CASE WHEN gs.hash_type='sha1'  THEN gs.hash_value ELSE NULL END) AS sha1,
                   MAX(CASE WHEN gs.hash_type='sha256' THEN gs.hash_value ELSE NULL END) AS sha256
            FROM games g
            JOIN systems s ON g.system_id = s.system_id
            JOIN game_signatures gs ON gs.game_id = g.game_id
            WHERE s.internal_name = ?
            GROUP BY g.game_id
        )");;
        q.addBindValue(systemName);

        if (q.exec()) {
            while (q.next()) {
                DatRomEntry entry;
                entry.gameName = q.value(0).toString();
                entry.crc32    = q.value(1).toString();
                entry.md5      = q.value(2).toString();
                entry.sha1     = q.value(3).toString();
                entry.sha256   = q.value(4).toString();

                if (!entry.sha256.isEmpty())
                    entries.insert(entry.sha256.toLower(), entry);
                if (!entry.sha1.isEmpty())
                    entries.insert(entry.sha1.toLower(), entry);
                if (!entry.md5.isEmpty())
                    entries.insert(entry.md5.toLower(), entry);
                if (!entry.crc32.isEmpty())
                    entries.insert(entry.crc32.toLower(), entry);
            }
            m_datCache.insert(systemName, entries);
            m_datHashTypes.insert(systemName,
                                  hashType.isEmpty() ? QStringLiteral("crc32") : hashType);
            qDebug() << "Loaded" << entries.size()
                     << "compendium DAT entries for" << systemName;
            return;
        }
        qWarning() << "VerificationEngine: compendium loadDatCache query failed:"
                   << q.lastError().text();
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    hashType = getPreferredHashType(systemName);
    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT e.game_name, e.rom_name, e.rom_size, e.crc32, e.md5, e.sha1, e.sha256, e.description, e.status
        FROM dat_entries e
        JOIN verification_dats d ON e.dat_id = d.id
        WHERE d.system_name = ?
    )");
    query.addBindValue(systemName);

    if (!query.exec()) {
        qWarning() << "Failed to load DAT cache:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        DatRomEntry entry;
        entry.gameName = query.value(0).toString();
        entry.romName = query.value(1).toString();
        entry.size = query.value(2).toLongLong();
        entry.crc32 = query.value(3).toString();
        entry.md5 = query.value(4).toString();
        entry.sha1 = query.value(5).toString();
        entry.sha256 = query.value(6).toString();
        entry.description = query.value(7).toString();
        entry.status = query.value(8).toString();

        if (!entry.sha256.isEmpty()) {
            entries.insert(entry.sha256.toLower(), entry);
        }
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

    QMap<QString, DatRomEntry> entries;

    // ── Compendium path ────────────────────────────────────────────────────
    if (!m_compendiumConnectionName.isEmpty()) {
        QSqlDatabase cdb = QSqlDatabase::database(m_compendiumConnectionName);
        QSqlQuery q(cdb);
        q.prepare(R"(
            SELECT pe.game_name, pe.rom_name, pe.rom_size, pe.crc32, pe.md5, pe.sha1, pe.sha256,
                   pe.description, pe.status, pe.base_title, pe.patch_name, pe.file_type
            FROM patch_entries pe
            JOIN patch_catalog_sources pcs ON pe.source_id = pcs.source_id
            WHERE pcs.system_name = ?
        )");
        q.addBindValue(systemName);

        if (q.exec()) {
            while (q.next()) {
                DatRomEntry entry;
                entry.gameName  = q.value(0).toString();
                entry.romName   = q.value(1).toString();
                entry.size      = q.value(2).toLongLong();
                entry.crc32     = q.value(3).toString();
                entry.md5       = q.value(4).toString();
                entry.sha1      = q.value(5).toString();
                entry.sha256    = q.value(6).toString();
                entry.description = q.value(7).toString();
                entry.status    = q.value(8).toString();
                entry.baseTitle = q.value(9).toString();
                entry.patchName = q.value(10).toString();
                entry.fileType  = q.value(11).toString();

                if (!entry.sha256.isEmpty())
                    entries.insert(entry.sha256.toLower(), entry);
                if (!entry.sha1.isEmpty())
                    entries.insert(entry.sha1.toLower(), entry);
                if (!entry.md5.isEmpty())
                    entries.insert(entry.md5.toLower(), entry);
                if (!entry.crc32.isEmpty())
                    entries.insert(entry.crc32.toLower(), entry);
            }
            m_patchDatCache.insert(systemName, entries);
            qDebug() << "Loaded" << entries.size()
                     << "compendium patch entries for" << systemName;
            return;
        }
        qWarning() << "VerificationEngine: compendium loadPatchDatCache query failed:"
                   << q.lastError().text();
    }

    // ── Runtime-import fallback ────────────────────────────────────────────
    QSqlQuery query(m_database->database());
    query.prepare(R"(
        SELECT e.game_name, e.rom_name, e.rom_size, e.crc32, e.md5, e.sha1, e.sha256,
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

    while (query.next()) {
        DatRomEntry entry;
        entry.gameName = query.value(0).toString();
        entry.romName = query.value(1).toString();
        entry.size = query.value(2).toLongLong();
        entry.crc32 = query.value(3).toString();
        entry.md5 = query.value(4).toString();
        entry.sha1 = query.value(5).toString();
        entry.sha256 = query.value(6).toString();
        entry.description = query.value(7).toString();
        entry.status = query.value(8).toString();
        entry.baseTitle = query.value(9).toString();
        entry.patchName = query.value(10).toString();
        entry.fileType = query.value(11).toString();

        if (!entry.sha256.isEmpty()) {
            entries.insert(entry.sha256.toLower(), entry);
        }
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
