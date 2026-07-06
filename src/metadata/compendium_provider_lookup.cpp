#include "compendium_provider.h"

#include "../core/constants/match_methods.h"
#include "../core/system_resolver.h"
#include "thumbnail_url_helper.h"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {

namespace {

    int releaseYearFromDate(const QString &releaseDate) {
        if (releaseDate.size() == 4) {
            bool ok = false;
            const int year = releaseDate.toInt(&ok);
            return ok ? year : 0;
        }

        const QDate parsed = QDate::fromString(releaseDate, Qt::ISODate);
        return parsed.isValid() ? parsed.year() : 0;
    }

    bool catalogSizeMatches(QSqlDatabase &db, const QString &sourceEntryKey, qint64 fileSize) {
        if (fileSize <= 0 || sourceEntryKey.isEmpty()) {
            return true;
        }

        QSqlQuery sizeQuery(db);
        sizeQuery.prepare(QStringLiteral("SELECT payload_json FROM source_items WHERE external_key = ? LIMIT 1"));
        sizeQuery.addBindValue(sourceEntryKey);
        if (!sizeQuery.exec() || !sizeQuery.next()) {
            return true;
        }

        const QJsonObject payload = QJsonDocument::fromJson(sizeQuery.value(0).toByteArray()).object();
        const qint64 expectedSize = static_cast<qint64>(payload.value(QStringLiteral("size")).toDouble(0.0));
        if (expectedSize <= 0) {
            return true;
        }
        return expectedSize == fileSize;
    }

} // namespace

QList<SearchResult> CompendiumProvider::searchByName(
    const QString &title, const QString &system, const QString &region) {
    QList<SearchResult> results;
    const QString searchTerm = title.trimmed();
    if (searchTerm.isEmpty())
        return results;

    QSqlDatabase db = database();
    if (!db.isOpen())
        return results;

    const int systemId = resolveSystemId(system);
    QString regionCode = m_normalizer.resolveRegionCode(region);
    if (regionCode.isEmpty())
        regionCode = region.trimmed().toUpper();

    // Build FTS5 MATCH expression: each word gets a prefix wildcard
    QStringList words = searchTerm.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList ftsWords;
    ftsWords.reserve(words.size());
    for (const QString &w : words)
        ftsWords.append(w + QLatin1Char('*'));
    const QString ftsExpr = ftsWords.join(QLatin1Char(' '));

    QSqlQuery query(db);
    bool usedFts = false;
    // Trigram FTS requires >= 3 characters; short queries fall through to LIKE.
    if (searchTerm.length() >= 3) {
        // Dedupe via subquery: each game_id appears once (best-ranked alias wins).
        query.prepare(QStringLiteral("SELECT g.game_id, g.canonical_title, g.primary_region_code, "
                                     "       g.release_date, g.release_year, s.internal_name "
                                     "FROM ("
                                     "    SELECT game_id, MIN(rank) AS best_rank "
                                     "    FROM games_search "
                                     "    WHERE games_search MATCH ? "
                                     "    GROUP BY game_id"
                                     ") fts "
                                     "JOIN games g ON g.game_id = fts.game_id "
                                     "JOIN systems s ON s.system_id = g.system_id "
                                     "WHERE (? = 0 OR g.system_id = ?) "
                                     "AND (? = '' OR UPPER(COALESCE(g.primary_region_code, '')) = ?) "
                                     "ORDER BY fts.best_rank "
                                     "LIMIT 10"));
        query.addBindValue(ftsExpr);
        query.addBindValue(systemId);
        query.addBindValue(systemId);
        query.addBindValue(regionCode);
        query.addBindValue(regionCode);
        usedFts = query.exec();
    }

    if (!usedFts) {
        // Fallback to LIKE for DBs without FTS5 or malformed queries
        const QString likePattern = QStringLiteral("%%1%").arg(searchTerm);
        query.prepare(QStringLiteral("SELECT g.game_id, g.canonical_title, g.primary_region_code, "
                                     "       g.release_date, g.release_year, s.internal_name "
                                     "FROM games g "
                                     "JOIN systems s ON s.system_id = g.system_id "
                                     "WHERE (? = 0 OR g.system_id = ?) "
                                     "AND (? = '' OR UPPER(COALESCE(g.primary_region_code, '')) = ?) "
                                     "AND ("
                                     "    LOWER(g.canonical_title) LIKE LOWER(?) "
                                     "    OR EXISTS ("
                                     "        SELECT 1 FROM game_names gn "
                                     "        WHERE gn.game_id = g.game_id AND LOWER(gn.name_text) LIKE LOWER(?)"
                                     "    )"
                                     ") "
                                     "ORDER BY LOWER(g.canonical_title), g.game_id "
                                     "LIMIT 10"));
        query.addBindValue(systemId);
        query.addBindValue(systemId);
        query.addBindValue(regionCode);
        query.addBindValue(regionCode);
        query.addBindValue(likePattern);
        query.addBindValue(likePattern);
        if (!query.exec()) {
            qWarning() << "CompendiumProvider::searchByName query failed:" << query.lastError().text();
            return results;
        }
    }

    const QString loweredSearch = searchTerm.toLower();
    while (query.next()) {
        const QString gameId = query.value(0).toString();
        const QString title = query.value(1).toString();
        if (gameId.isEmpty() || title.isEmpty())
            continue;

        SearchResult result;
        result.id = gameId;
        result.title = title;
        result.region = query.value(2).toString();
        const QString releaseDate = query.value(3).toString();
        result.releaseYear = !releaseDate.isEmpty() ? releaseYearFromDate(releaseDate) : query.value(4).toInt();
        result.system = query.value(5).toString();

        const QString loweredTitle = title.toLower();
        if (loweredTitle == loweredSearch)
            result.matchScore = 1.0f;
        else if (loweredTitle.startsWith(loweredSearch))
            result.matchScore = 0.9f;
        else
            result.matchScore = 0.7f;

        results.append(result);
    }

    return results;
}

namespace {

    QString patchHashColumn(const QString &hashType) {
        if (hashType == QStringLiteral("crc32"))
            return QStringLiteral("crc32");
        if (hashType == QStringLiteral("md5"))
            return QStringLiteral("md5");
        if (hashType == QStringLiteral("sha1"))
            return QStringLiteral("sha1");
        return { };
    }

    GameMetadata metadataFromPatchRow(
        const QSqlQuery &query, const QString &systemName, const QString &hashType, const QString &normalizedHash) {
        GameMetadata metadata;
        const QString gameName = query.value(0).toString();
        const QString baseTitle = query.value(3).toString();
        const QString patchName = query.value(4).toString();
        const QString fileType = query.value(5).toString();
        const QString catalogSystem = query.value(6).toString();

        metadata.id = QStringLiteral("patch:%1:%2:%3").arg(catalogSystem, hashType, normalizedHash);
        metadata.title = gameName.isEmpty() ? baseTitle : gameName;
        metadata.system = catalogSystem.isEmpty() ? systemName : catalogSystem;
        metadata.description = query.value(2).toString();
        metadata.matchScore = 1.0f;
        metadata.matchMethod = QString::fromLatin1(Constants::MatchMethods::HASH);
        metadata.providerId = QStringLiteral("compendium");
        if (!baseTitle.isEmpty())
            metadata.externalIds.insert(QStringLiteral("base_title"), baseTitle);
        if (!patchName.isEmpty())
            metadata.externalIds.insert(QStringLiteral("patch_name"), patchName);
        if (!fileType.isEmpty())
            metadata.externalIds.insert(QStringLiteral("file_type"), fileType);
        return metadata;
    }

} // namespace

GameMetadata CompendiumProvider::lookupPatchByHash(const QString &hashType, const QString &normalizedHash,
    const QString &system, int systemId, qint64 fileSize) const {
    const QString hashColumn = patchHashColumn(hashType);
    if (hashColumn.isEmpty()) {
        return { };
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return { };
    }

    const QString patchSystemName = systemId != 0 ? SystemResolver::internalName(systemId) : system.trimmed();

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT pe.game_name, pe.rom_name, pe.description, pe.base_title, pe.patch_name, "
                                 "pe.file_type, pcs.system_name, pe.rom_size "
                                 "FROM patch_entries pe "
                                 "JOIN patch_catalog_sources pcs ON pe.source_id = pcs.source_id "
                                 "WHERE (? = '' OR pcs.system_name = ?) "
                                 "AND LOWER(pe.%1) = ? "
                                 "LIMIT 1")
            .arg(hashColumn));
    query.addBindValue(patchSystemName);
    query.addBindValue(patchSystemName);
    query.addBindValue(normalizedHash);
    if (!query.exec()) {
        // Older compendium builds may not include patch catalog tables yet.
        if (!query.lastError().text().contains(QStringLiteral("no such table"), Qt::CaseInsensitive)) {
            qWarning() << "CompendiumProvider::lookupPatchByHash query failed:" << query.lastError().text();
        }
        return { };
    }
    if (!query.next()) {
        return { };
    }

    if (fileSize > 0) {
        const qint64 catalogSize = query.value(7).toLongLong();
        if (catalogSize > 0 && catalogSize != fileSize) {
            return { };
        }
    }

    return metadataFromPatchRow(query, system, hashType, normalizedHash);
}

GameMetadata CompendiumProvider::getByHash(const QString &hash, const QString &system) {
    return getByHash(hash, system, 0);
}

GameMetadata CompendiumProvider::getByHash(const QString &hash, const QString &system, qint64 fileSize) {
    QString normalizedHash;
    const QString hashType = detectHashType(hash, normalizedHash);
    if (hashType.isEmpty()) {
        return { };
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return { };
    }

    const int systemId = resolveSystemId(system);

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT gs.game_id, gs.source_entry_key "
                                 "FROM game_signatures gs "
                                 "JOIN games g ON g.game_id = gs.game_id "
                                 "WHERE gs.hash_type = ? AND gs.hash_value = ? "
                                 "AND (? = 0 OR g.system_id = ?) "
                                 "LIMIT 1"));
    query.addBindValue(hashType);
    query.addBindValue(normalizedHash);
    query.addBindValue(systemId);
    query.addBindValue(systemId);
    if (!query.exec()) {
        qWarning() << "CompendiumProvider::getByHash query failed:" << query.lastError().text();
        return { };
    }
    if (!query.next()) {
        return lookupPatchByHash(hashType, normalizedHash, system, systemId, fileSize);
    }

    const QString gameId = query.value(0).toString();
    const QString sourceEntryKey = query.value(1).toString();
    if (!catalogSizeMatches(db, sourceEntryKey, fileSize)) {
        return { };
    }

    GameMetadata metadata = fetchGameMetadata(gameId);
    if (!metadata.id.isEmpty()) {
        metadata.matchScore = 1.0f;
        metadata.matchMethod = QString::fromLatin1(Constants::MatchMethods::HASH);

        // The source_entry_key is formatted as "System|ROM title|..." where the second
        // pipe-delimited segment is the No-Intro/Redump title for this specific hash.
        // Using it avoids returning a merged canonical title (which may be a Beta or
        // alternate-region variant) when the matched ROM is a distinct regional release.
        const QStringList entryParts = sourceEntryKey.split(QLatin1Char('|'));
        if (entryParts.size() >= 2) {
            const QString romTitle = entryParts.at(1).trimmed();
            if (!romTitle.isEmpty()) {
                metadata.title = romTitle;
            }
        }
        populateDiscContextFromSourceEntry(metadata, sourceEntryKey);
    }
    return metadata;
}

GameMetadata CompendiumProvider::getBySerial(const QString &serial, const QString &system) {
    const QString trimmedSerial = serial.trimmed();
    if (trimmedSerial.isEmpty()) {
        return { };
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return { };
    }

    const int systemId = resolveSystemId(system);

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT gs.game_id, gs.source_entry_key "
                                 "FROM game_serials gs "
                                 "JOIN games g ON g.game_id = gs.game_id "
                                 "WHERE gs.serial_value = ? "
                                 "AND (? = 0 OR g.system_id = ?) "
                                 "LIMIT 1"));
    query.addBindValue(trimmedSerial);
    query.addBindValue(systemId);
    query.addBindValue(systemId);
    if (!query.exec()) {
        qWarning() << "CompendiumProvider::getBySerial query failed:" << query.lastError().text();
        return { };
    }
    if (!query.next()) {
        return { };
    }

    const QString gameId = query.value(0).toString();
    const QString sourceEntryKey = query.value(1).toString();

    GameMetadata metadata = fetchGameMetadata(gameId);
    if (!metadata.id.isEmpty()) {
        metadata.matchScore = 0.9f;
        metadata.matchMethod = QStringLiteral("serial");

        const QStringList entryParts = sourceEntryKey.split(QLatin1Char('|'));
        if (entryParts.size() >= 2) {
            const QString romTitle = entryParts.at(1).trimmed();
            if (!romTitle.isEmpty()) {
                metadata.title = romTitle;
            }
        }
        populateDiscContextFromSourceEntry(metadata, sourceEntryKey);
    }
    return metadata;
}

GameMetadata CompendiumProvider::getById(const QString &id) {
    GameMetadata m = fetchGameMetadata(id);
    if (!m.id.isEmpty()) {
        m.matchScore = 1.0f;
        m.matchMethod = QStringLiteral("id");
    }
    return m;
}

ArtworkUrls CompendiumProvider::getArtwork(const QString &id) {
    if (id.isEmpty()) {
        return { };
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return { };
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT g.canonical_title, s.libretro_name "
                                 "FROM games g "
                                 "JOIN systems s ON s.system_id = g.system_id "
                                 "WHERE g.game_id = ?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next()) {
        return { };
    }

    const QString title = query.value(0).toString();
    const QString libretroName = query.value(1).toString();
    if (title.isEmpty() || libretroName.isEmpty()) {
        return { };
    }

    const QString repoRoot = Metadata::ThumbnailUrlHelper::repoRootFromCompendiumDb(m_databasePath);

    ArtworkUrls artwork;
    const auto resolve = [&](const QString &assetType) -> QUrl {
        const QString url = Metadata::ThumbnailUrlHelper::resolveArtworkUrl(
            db, repoRoot, id, libretroName, title, assetType, m_strictOffline);
        return url.isEmpty() ? QUrl() : QUrl(url);
    };

    artwork.boxFront = resolve(QStringLiteral("box"));
    artwork.screenshot = resolve(QStringLiteral("snap"));
    artwork.titleScreen = resolve(QStringLiteral("title"));
    return artwork;
}

} // namespace Remus
