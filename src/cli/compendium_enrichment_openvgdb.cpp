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
    QString releaseDate;
};

} // namespace

namespace CompendiumEnrichment {

bool enrichFromOpenVGDB(
    QSqlDatabase &database, const QString &openvgdbPath, int &gamesEnriched, int &factsInserted, QString &error) {
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
            e.description = q.value(1).toString();
            e.genre = q.value(2).toString();
            e.developer = q.value(3).toString();
            e.publisher = q.value(4).toString();
            const QString dateStr = q.value(5).toString();
            if (dateStr.length() >= 4) {
                bool ok = false;
                const int y = dateStr.left(4).toInt(&ok);
                if (ok && y > 1970 && y < 2030)
                    e.releaseYear = y;
            }
            if (dateStr.length() >= 10)
                e.releaseDate = dateStr.left(10);
        };

        // CRC32 index
        {
            QSqlQuery q(ovgdb);
            if (!q.exec(QStringLiteral("SELECT r.romHashCRC, re.releaseDescription, re.releaseGenre, "
                                       "re.releaseDeveloper, re.releasePublisher, re.releaseDate "
                                       "FROM ROMs r JOIN RELEASES re ON r.romID = re.romID "
                                       "WHERE r.romHashCRC IS NOT NULL AND r.romHashCRC != ''"))) {
                error = QStringLiteral("OpenVGDB CRC32 query: %1").arg(q.lastError().text());
                QSqlDatabase::removeDatabase(connName);
                return false;
            }
            while (q.next()) {
                // Right-justify to 8 chars in case leading zeros were trimmed
                const QString crc = q.value(0).toString().toUpper().rightJustified(8, QLatin1Char('0'));
                if (!crcIndex.contains(crc))
                    populateEntry(q, crcIndex[crc]);
            }
        }

        // MD5 index — covers disc-based systems (GCN, Saturn, PSX, Dreamcast)
        // where the compendium stores md5 signatures rather than crc32
        {
            QSqlQuery q(ovgdb);
            if (!q.exec(QStringLiteral("SELECT r.romHashMD5, re.releaseDescription, re.releaseGenre, "
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
            if (!q.exec(QStringLiteral("SELECT s.systemShortName, re.releaseTitleName, "
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
                const QString normTitle = normalizeMetadataTitle(q.value(1).toString());
                if (normTitle.isEmpty())
                    continue;
                const QString key = systemShort + QLatin1Char(':') + normTitle;
                if (titleIndex.contains(key))
                    continue;
                OpenVGDBEntry &e = titleIndex[key];
                e.description = q.value(2).toString();
                e.genre = q.value(3).toString();
                e.developer = q.value(4).toString();
                e.publisher = q.value(5).toString();
                const QString dateStr = q.value(6).toString();
                if (dateStr.length() >= 4) {
                    bool ok = false;
                    const int y = dateStr.left(4).toInt(&ok);
                    if (ok && y > 1970 && y < 2030)
                        e.releaseYear = y;
                }
                if (dateStr.length() >= 10)
                    e.releaseDate = dateStr.left(10);
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
            if (!e.description.isEmpty())
                ++descFreq[e.description];
        for (const auto &e : std::as_const(md5Index))
            if (!e.description.isEmpty())
                ++descFreq[e.description];
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

    const QString sourceId = QStringLiteral("openvgdb");
    const QString snapshotId = QStringLiteral("openvgdb-v29.0");

    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("OpenVGDB"),
                QStringLiteral("openvgdb"),
                QStringLiteral("https://github.com/OpenVGDB/OpenVGDB"),
                /*attributionRequired=*/false,
                /*priority=*/25,
                QStringLiteral("MIT"),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("OpenVGDB v29.0"),
            },
            error))
        return false;

    // Preload CRC32 → gameId from compendium signatures
    QHash<QString, QString> gameIdByCrc;
    {
        QSqlQuery q(database);
        if (!q.exec(
                QStringLiteral("SELECT game_id, UPPER(hash_value) FROM game_signatures WHERE hash_type = 'crc32'"))) {
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
        if (!q.exec(QStringLiteral("SELECT game_id, UPPER(hash_value) FROM game_signatures WHERE hash_type = 'md5'"))) {
            error = QStringLiteral("Load MD5 hashes: %1").arg(q.lastError().text());
            return false;
        }
        while (q.next())
            gameIdByMd5.insert(q.value(1).toString(), q.value(0).toString());
    }

    QSqlQuery updateQuery(database);
    updateQuery.prepare(QStringLiteral("UPDATE games SET "
                                       "genre        = COALESCE(NULLIF(genre, ''), ?), "
                                       "developer    = COALESCE(NULLIF(developer, ''), ?), "
                                       "publisher    = COALESCE(NULLIF(publisher, ''), ?), "
                                       "release_year = COALESCE(release_year, ?), "
                                       "release_date = COALESCE(release_date, ?), "
                                       "description  = COALESCE(NULLIF(description, ''), ?) "
                                       "WHERE game_id = ?"));

    QSqlQuery factQuery(database);
    factQuery.prepare(QStringLiteral(
        "INSERT INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery delQuery(database);
    delQuery.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    const FactInsertSpec factSpec {
        sourceId,
        snapshotId,
        25,
        0.0,
    };

    auto insertFact = [&](const QString &gameId, const QString &field, const QString &value, const QString &valueType,
                          double confidence, const QString &contextPrefix) -> bool {
        FactInsertSpec scopedFactSpec = factSpec;
        scopedFactSpec.confidence = confidence;
        bool inserted = false;
        if (!insertGameFact(
                delQuery, factQuery, scopedFactSpec, gameId, field, value, valueType, error, contextPrefix, &inserted))
            return false;
        if (inserted)
            ++factsInserted;
        return true;
    };

    auto hasData = [](const OpenVGDBEntry &e) {
        return !e.description.isEmpty() || !e.genre.isEmpty() || !e.developer.isEmpty() || !e.publisher.isEmpty()
            || e.releaseYear > 0;
    };

    auto applyEntryToGame = [&](const QString &gameId, const OpenVGDBEntry &e, double confidence,
                                const QString &contextPrefix, const QString &releaseYearType) -> bool {
        if (!hasData(e))
            return true;

        updateQuery.bindValue(0, nullableText(e.genre));
        updateQuery.bindValue(1, nullableText(e.developer));
        updateQuery.bindValue(2, nullableText(e.publisher));
        updateQuery.bindValue(3, nullableInt(e.releaseYear));
        updateQuery.bindValue(4, nullableText(e.releaseDate));
        updateQuery.bindValue(5, nullableText(e.description));
        updateQuery.bindValue(6, gameId);
        if (!execPrepared(updateQuery, error, QStringLiteral("Update game %1").arg(contextPrefix))) {
            return false;
        }
        if (updateQuery.numRowsAffected() > 0)
            ++gamesEnriched;

        if (!insertFact(gameId, QStringLiteral("genre"), e.genre, QStringLiteral("text"), confidence, contextPrefix))
            return false;
        if (!insertFact(
                gameId, QStringLiteral("developer"), e.developer, QStringLiteral("text"), confidence, contextPrefix))
            return false;
        if (!insertFact(
                gameId, QStringLiteral("publisher"), e.publisher, QStringLiteral("text"), confidence, contextPrefix))
            return false;
        if (!insertFact(gameId, QStringLiteral("description"), e.description, QStringLiteral("text"), confidence,
                contextPrefix))
            return false;
        if (!e.releaseDate.isEmpty()
            && !insertFact(gameId, QStringLiteral("release_date"), e.releaseDate, QStringLiteral("text"), confidence,
                contextPrefix))
            return false;
        if (e.releaseYear > 0
            && !insertFact(gameId, QStringLiteral("release_year"), QString::number(e.releaseYear), releaseYearType,
                confidence, contextPrefix))
            return false;
        return true;
    };

    for (auto it = gameIdByCrc.cbegin(); it != gameIdByCrc.cend(); ++it) {
        const QString &crc32 = it.key();
        const QString &gameId = it.value();

        auto entryIt = crcIndex.constFind(crc32);
        if (entryIt == crcIndex.cend())
            continue;
        const OpenVGDBEntry &e = *entryIt;

        if (!applyEntryToGame(gameId, e, 0.80, QStringLiteral("openvgdb"), QStringLiteral("int"))) {
            return false;
        }
    }

    // MD5 pass — enriches disc-based systems (GCN, Saturn, PSX, Dreamcast, N64)
    // where the compendium stores md5 rather than crc32 signatures.
    // COALESCE and INSERT OR IGNORE prevent overwriting data already set by the CRC pass.
    for (auto it = gameIdByMd5.cbegin(); it != gameIdByMd5.cend(); ++it) {
        const QString &md5 = it.key();
        const QString &gameId = it.value();

        auto entryIt = md5Index.constFind(md5);
        if (entryIt == md5Index.cend())
            continue;
        const OpenVGDBEntry &e = *entryIt;

        if (!applyEntryToGame(gameId, e, 0.80, QStringLiteral("openvgdb MD5"), QStringLiteral("int"))) {
            return false;
        }
    }

    // Title-based fallback pass — for games still without a description after both
    // hash passes.  Matches on normalised title + OpenVGDB systemShortName (which
    // equals our systems.internal_name for NES, N64, SNES, GBA, etc.).
    // Confidence is 0.60 (vs 0.80 for hash) since title matching is less precise.
    if (!titleIndex.isEmpty()) {
        // Games that still lack a description after hash matching (NULL or empty string)
        QHash<QString, QPair<QString, QString>> gamesForTitleMatch; // gameId → (title, internalName)
        {
            QSqlQuery q(database);
            if (!q.exec(QStringLiteral("SELECT g.game_id, g.canonical_title, sys.internal_name "
                                       "FROM games g "
                                       "JOIN systems sys ON sys.system_id = g.system_id "
                                       "WHERE g.description IS NULL OR g.description = '' "
                                       "   OR g.developer IS NULL OR g.developer = '' "
                                       "   OR g.publisher IS NULL OR g.publisher = '' "
                                       "   OR g.release_year IS NULL"))) {
                error = QStringLiteral("Load games for title match: %1").arg(q.lastError().text());
                return false;
            }
            while (q.next())
                gamesForTitleMatch.insert(q.value(0).toString(), { q.value(1).toString(), q.value(2).toString() });
        }

        for (auto it = gamesForTitleMatch.cbegin(); it != gamesForTitleMatch.cend(); ++it) {
            const QString &gameId = it.key();
            const QString &title = it.value().first;
            const QString &internalName = it.value().second;

            const QString key = internalName + QLatin1Char(':') + normalizeMetadataTitle(title);
            auto entryIt = titleIndex.constFind(key);
            if (entryIt == titleIndex.cend())
                continue;
            const OpenVGDBEntry &e = *entryIt;

            if (!applyEntryToGame(gameId, e, 0.60, QStringLiteral("openvgdb title"), QStringLiteral("int"))) {
                return false;
            }
        }
    }

    return true;
}

} // namespace CompendiumEnrichment
