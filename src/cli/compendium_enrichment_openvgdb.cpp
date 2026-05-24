#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"

#include <QDate>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace CompendiumEnrichmentSql;

namespace {

struct OpenVGDBEntry {
    QString description;
    QString genre;
    QString developer;
    QString publisher;
    int releaseYear = 0;
};

} // namespace

namespace CompendiumEnrichment {

bool enrichFromOpenVGDB(QSqlDatabase &database,
                        const QString &openvgdbPath,
                        int &gamesEnriched,
                        int &factsInserted,
                        QString &error)
{
    gamesEnriched = 0;
    factsInserted = 0;

    if (!QFile::exists(openvgdbPath))
        return true;

    // Open OpenVGDB as a read-only secondary SQLite connection
    const QString connName = QStringLiteral("openvgdb-reader");
    {
        QSqlDatabase ovgdb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        ovgdb.setDatabaseName(openvgdbPath);
        ovgdb.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!ovgdb.open()) {
            error = QStringLiteral("Cannot open OpenVGDB: %1").arg(ovgdb.lastError().text());
            QSqlDatabase::removeDatabase(connName);
            return false;
        }
    }

    // Load CRC32, MD5, and title entry maps from OpenVGDB (keep first occurrence per key)
    QHash<QString, OpenVGDBEntry> crcIndex;
    QHash<QString, OpenVGDBEntry> md5Index;
    // keyed by "systemShortName:normalizedTitle" — fallback for systems where hashes diverge
    QHash<QString, OpenVGDBEntry> titleIndex;
    {
        QSqlDatabase ovgdb = QSqlDatabase::database(connName);

        auto populateEntry = [](QSqlQuery &q, OpenVGDBEntry &e) {
            e.description    = q.value(1).toString();
            e.genre          = q.value(2).toString();
            e.developer      = q.value(3).toString();
            e.publisher      = q.value(4).toString();
            const QString dateStr = q.value(5).toString();
            if (dateStr.length() >= 4) {
                bool ok = false;
                const int y = dateStr.left(4).toInt(&ok);
                if (ok && y > 1970 && y < 2030)
                    e.releaseYear = y;
            }
        };

        // CRC32 index
        {
            QSqlQuery q(ovgdb);
            if (!q.exec(QStringLiteral(
                    "SELECT r.romHashCRC, re.releaseDescription, re.releaseGenre, "
                    "re.releaseDeveloper, re.releasePublisher, re.releaseDate "
                    "FROM ROMs r JOIN RELEASES re ON r.romID = re.romID "
                    "WHERE r.romHashCRC IS NOT NULL AND r.romHashCRC != ''"))) {
                error = QStringLiteral("OpenVGDB CRC32 query: %1").arg(q.lastError().text());
                QSqlDatabase::removeDatabase(connName);
                return false;
            }
            while (q.next()) {
                // Right-justify to 8 chars in case leading zeros were trimmed
                const QString crc = q.value(0).toString().toUpper()
                                        .rightJustified(8, QLatin1Char('0'));
                if (!crcIndex.contains(crc))
                    populateEntry(q, crcIndex[crc]);
            }
        }

        // MD5 index — covers disc-based systems (GCN, Saturn, PSX, Dreamcast)
        // where the compendium stores md5 signatures rather than crc32
        {
            QSqlQuery q(ovgdb);
            if (!q.exec(QStringLiteral(
                    "SELECT r.romHashMD5, re.releaseDescription, re.releaseGenre, "
                    "re.releaseDeveloper, re.releasePublisher, re.releaseDate "
                    "FROM ROMs r JOIN RELEASES re ON r.romID = re.romID "
                    "WHERE r.romHashMD5 IS NOT NULL AND r.romHashMD5 != ''"))) {
                error = QStringLiteral("OpenVGDB MD5 query: %1").arg(q.lastError().text());
                QSqlDatabase::removeDatabase(connName);
                return false;
            }
            while (q.next()) {
                const QString md5 = q.value(0).toString().toUpper().trimmed();
                if (md5.length() == 32 && !md5Index.contains(md5))
                    populateEntry(q, md5Index[md5]);
            }
        }

        // Title index — fallback for systems where ROM-set hashes differ between
        // OpenVGDB (older No-Intro dumps) and current No-Intro (e.g. NES, N64).
        // Only loads entries that carry a description so the pass is worthwhile.
        {
            QSqlQuery q(ovgdb);
            if (!q.exec(QStringLiteral(
                    "SELECT s.systemShortName, re.releaseTitleName, "
                    "re.releaseDescription, re.releaseGenre, "
                    "re.releaseDeveloper, re.releasePublisher, re.releaseDate "
                    "FROM RELEASES re "
                    "JOIN ROMs r ON r.romID = re.romID "
                    "JOIN SYSTEMS s ON s.systemID = r.systemID "
                    "WHERE re.releaseTitleName IS NOT NULL AND re.releaseTitleName != '' "
                    "  AND re.releaseDescription IS NOT NULL AND re.releaseDescription != ''"))) {
                error = QStringLiteral("OpenVGDB title query: %1").arg(q.lastError().text());
                QSqlDatabase::removeDatabase(connName);
                return false;
            }
            while (q.next()) {
                const QString systemShort = q.value(0).toString();
                const QString normTitle   = normalizeMetadataTitle(q.value(1).toString());
                if (normTitle.isEmpty()) continue;
                const QString key = systemShort + QLatin1Char(':') + normTitle;
                if (titleIndex.contains(key)) continue;
                OpenVGDBEntry &e  = titleIndex[key];
                e.description     = q.value(2).toString();
                e.genre           = q.value(3).toString();
                e.developer       = q.value(4).toString();
                e.publisher       = q.value(5).toString();
                const QString dateStr = q.value(6).toString();
                if (dateStr.length() >= 4) {
                    bool ok = false;
                    const int y = dateStr.left(4).toInt(&ok);
                    if (ok && y > 1970 && y < 2030)
                        e.releaseYear = y;
                }
            }
        }

        ovgdb.close();
    }
    QSqlDatabase::removeDatabase(connName);

    // Filter boilerplate descriptions: text appearing in more than 30 distinct
    // ROM entries is a genre-level placeholder (e.g. "Mahjong is a game for four
    // players..."), not a game-specific synopsis. Clear it so the field stays
    // available for a higher-quality source or a per-game lookup.
    {
        QHash<QString, int> descFreq;
        for (const auto &e : std::as_const(crcIndex))
            if (!e.description.isEmpty()) ++descFreq[e.description];
        for (const auto &e : std::as_const(md5Index))
            if (!e.description.isEmpty()) ++descFreq[e.description];
        constexpr int kBoilerplateThreshold = 30;
        for (auto &e : crcIndex)
            if (!e.description.isEmpty() && descFreq.value(e.description) > kBoilerplateThreshold)
                e.description.clear();
        for (auto &e : md5Index)
            if (!e.description.isEmpty() && descFreq.value(e.description) > kBoilerplateThreshold)
                e.description.clear();
        for (auto &e : titleIndex)
            if (!e.description.isEmpty() && descFreq.value(e.description) > kBoilerplateThreshold)
                e.description.clear();
    }

    if (crcIndex.isEmpty() && md5Index.isEmpty() && titleIndex.isEmpty())
        return true;

    const QString sourceId   = QStringLiteral("openvgdb");
    const QString snapshotId = QStringLiteral("openvgdb-v29.0");

    if (!upsertEnrichmentSource(database,
                                sourceId,
                                QStringLiteral("OpenVGDB"),
                                QStringLiteral("openvgdb"),
                                QStringLiteral("https://github.com/OpenVGDB/OpenVGDB"),
                                /*attributionRequired=*/false,
                                25,
                                snapshotId,
                                QStringLiteral("OpenVGDB v29.0"),
                                QStringLiteral("MIT"),
                                error))
        return false;

    // Preload CRC32 → gameId from compendium signatures
    QHash<QString, QString> gameIdByCrc;
    {
        QSqlQuery q(database);
        if (!q.exec(QStringLiteral(
                "SELECT game_id, UPPER(hash_value) FROM game_signatures WHERE hash_type = 'crc32'"))) {
            error = QStringLiteral("Load CRC32 hashes: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next())
            gameIdByCrc.insert(q.value(1).toString(), q.value(0).toString());
    }

    // Preload MD5 → gameId from compendium signatures (for disc-based systems)
    QHash<QString, QString> gameIdByMd5;
    if (!md5Index.isEmpty()) {
        QSqlQuery q(database);
        if (!q.exec(QStringLiteral(
                "SELECT game_id, UPPER(hash_value) FROM game_signatures WHERE hash_type = 'md5'"))) {
            error = QStringLiteral("Load MD5 hashes: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next())
            gameIdByMd5.insert(q.value(1).toString(), q.value(0).toString());
    }

    QSqlQuery updateQuery(database);
    updateQuery.prepare(QStringLiteral(
        "UPDATE games SET "
        "genre        = COALESCE(genre, ?), "
        "developer    = COALESCE(developer, ?), "
        "publisher    = COALESCE(publisher, ?), "
        "release_year = COALESCE(release_year, ?), "
        "description  = COALESCE(description, ?) "
        "WHERE game_id = ?"));

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    auto insertFact = [&](const QString &gameId,
                          const QString &field,
                          const QString &value,
                          const QString &valueType) -> bool {
        if (value.isEmpty()) return true;
        factQuery.bindValue(0, gameId);
        factQuery.bindValue(1, field);
        factQuery.bindValue(2, value);
        factQuery.bindValue(3, valueType);
        factQuery.bindValue(4, sourceId);
        factQuery.bindValue(5, snapshotId);
        factQuery.bindValue(6, 25);
        factQuery.bindValue(7, 0.80);
        if (!execPrepared(factQuery, error,
                          QStringLiteral("Insert openvgdb fact %1").arg(field)))
            return false;
        if (factQuery.numRowsAffected() > 0) ++factsInserted;
        return true;
    };

    auto nullStr = [](const QString &s) {
        return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
    };
    auto nullInt = [](int v) {
        return v > 0 ? QVariant(v) : QVariant(QMetaType(QMetaType::Int));
    };

    for (auto it = gameIdByCrc.cbegin(); it != gameIdByCrc.cend(); ++it) {
        const QString &crc32  = it.key();
        const QString &gameId = it.value();

        auto entryIt = crcIndex.constFind(crc32);
        if (entryIt == crcIndex.cend()) continue;
        const OpenVGDBEntry &e = *entryIt;

        const bool hasData = !e.description.isEmpty() || !e.genre.isEmpty()
            || !e.developer.isEmpty() || !e.publisher.isEmpty() || e.releaseYear > 0;
        if (!hasData) continue;

        updateQuery.bindValue(0, nullStr(e.genre));
        updateQuery.bindValue(1, nullStr(e.developer));
        updateQuery.bindValue(2, nullStr(e.publisher));
        updateQuery.bindValue(3, nullInt(e.releaseYear));
        updateQuery.bindValue(4, nullStr(e.description));
        updateQuery.bindValue(5, gameId);
        if (!execPrepared(updateQuery, error,
                          QStringLiteral("Update game openvgdb")))
            return false;
        if (updateQuery.numRowsAffected() > 0) ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"),       e.genre,        QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("developer"),   e.developer,    QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("publisher"),   e.publisher,    QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("description"), e.description,  QStringLiteral("text"))) return false;
        if (e.releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"),
                           QString::number(e.releaseYear),     QStringLiteral("int"))) return false;
    }

    // MD5 pass — enriches disc-based systems (GCN, Saturn, PSX, Dreamcast, N64)
    // where the compendium stores md5 rather than crc32 signatures.
    // COALESCE and INSERT OR IGNORE prevent overwriting data already set by the CRC pass.
    for (auto it = gameIdByMd5.cbegin(); it != gameIdByMd5.cend(); ++it) {
        const QString &md5    = it.key();
        const QString &gameId = it.value();

        auto entryIt = md5Index.constFind(md5);
        if (entryIt == md5Index.cend()) continue;
        const OpenVGDBEntry &e = *entryIt;

        const bool hasData = !e.description.isEmpty() || !e.genre.isEmpty()
            || !e.developer.isEmpty() || !e.publisher.isEmpty() || e.releaseYear > 0;
        if (!hasData) continue;

        updateQuery.bindValue(0, nullStr(e.genre));
        updateQuery.bindValue(1, nullStr(e.developer));
        updateQuery.bindValue(2, nullStr(e.publisher));
        updateQuery.bindValue(3, nullInt(e.releaseYear));
        updateQuery.bindValue(4, nullStr(e.description));
        updateQuery.bindValue(5, gameId);
        if (!execPrepared(updateQuery, error,
                          QStringLiteral("Update game openvgdb MD5")))
            return false;
        if (updateQuery.numRowsAffected() > 0) ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"),       e.genre,        QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("developer"),   e.developer,    QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("publisher"),   e.publisher,    QStringLiteral("text"))) return false;
        if (!insertFact(gameId, QStringLiteral("description"), e.description,  QStringLiteral("text"))) return false;
        if (e.releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"),
                           QString::number(e.releaseYear),     QStringLiteral("int"))) return false;
    }

    // Title-based fallback pass — for games still without a description after both
    // hash passes.  Matches on normalised title + OpenVGDB systemShortName (which
    // equals our systems.internal_name for NES, N64, SNES, GBA, etc.).
    // Confidence is 0.60 (vs 0.80 for hash) since title matching is less precise.
    if (!titleIndex.isEmpty()) {
        // Games that still lack a description after hash matching
        QHash<QString, QPair<QString, QString>> gamesForTitleMatch; // gameId → (title, internalName)
        {
            QSqlQuery q(database);
            if (!q.exec(QStringLiteral(
                    "SELECT g.game_id, g.canonical_title, sys.internal_name "
                    "FROM games g "
                    "JOIN systems sys ON sys.system_id = g.system_id "
                    "WHERE g.description IS NULL"))) {
                error = QStringLiteral("Load games for title match: %1").arg(q.lastError().text());
                return false;
            }
            while (q.next())
                gamesForTitleMatch.insert(q.value(0).toString(),
                                          {q.value(1).toString(), q.value(2).toString()});
        }

        QSqlQuery titleFactQuery(database);
        titleFactQuery.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        auto insertTitleFact = [&](const QString &gameId,
                                   const QString &field,
                                   const QString &value,
                                   const QString &valueType) -> bool {
            if (value.isEmpty()) return true;
            titleFactQuery.bindValue(0, gameId);
            titleFactQuery.bindValue(1, field);
            titleFactQuery.bindValue(2, value);
            titleFactQuery.bindValue(3, valueType);
            titleFactQuery.bindValue(4, sourceId);
            titleFactQuery.bindValue(5, snapshotId);
            titleFactQuery.bindValue(6, 25);
            titleFactQuery.bindValue(7, 0.60);
            if (!execPrepared(titleFactQuery, error,
                              QStringLiteral("Insert openvgdb title fact %1").arg(field)))
                return false;
            if (titleFactQuery.numRowsAffected() > 0) ++factsInserted;
            return true;
        };

        for (auto it = gamesForTitleMatch.cbegin(); it != gamesForTitleMatch.cend(); ++it) {
            const QString &gameId       = it.key();
            const QString &title        = it.value().first;
            const QString &internalName = it.value().second;

            const QString key = internalName + QLatin1Char(':') + normalizeMetadataTitle(title);
            auto entryIt = titleIndex.constFind(key);
            if (entryIt == titleIndex.cend()) continue;
            const OpenVGDBEntry &e = *entryIt;

            const bool hasData = !e.description.isEmpty() || !e.genre.isEmpty()
                || !e.developer.isEmpty() || !e.publisher.isEmpty() || e.releaseYear > 0;
            if (!hasData) continue;

            updateQuery.bindValue(0, nullStr(e.genre));
            updateQuery.bindValue(1, nullStr(e.developer));
            updateQuery.bindValue(2, nullStr(e.publisher));
            updateQuery.bindValue(3, nullInt(e.releaseYear));
            updateQuery.bindValue(4, nullStr(e.description));
            updateQuery.bindValue(5, gameId);
            if (!execPrepared(updateQuery, error,
                              QStringLiteral("Update game openvgdb title")))
                return false;
            if (updateQuery.numRowsAffected() > 0) ++gamesEnriched;

            if (!insertTitleFact(gameId, QStringLiteral("genre"),       e.genre,        QStringLiteral("text"))) return false;
            if (!insertTitleFact(gameId, QStringLiteral("developer"),   e.developer,    QStringLiteral("text"))) return false;
            if (!insertTitleFact(gameId, QStringLiteral("publisher"),   e.publisher,    QStringLiteral("text"))) return false;
            if (!insertTitleFact(gameId, QStringLiteral("description"), e.description,  QStringLiteral("text"))) return false;
            if (e.releaseYear > 0
                && !insertTitleFact(gameId, QStringLiteral("release_year"),
                                    QString::number(e.releaseYear),     QStringLiteral("int"))) return false;
        }
    }

    return true;
}

} // namespace CompendiumEnrichment
