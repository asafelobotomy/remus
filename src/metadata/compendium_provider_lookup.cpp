#include "compendium_provider.h"

#include "../core/constants/match_methods.h"
#include "../core/system_resolver.h"
#include "thumbnail_url_helper.h"

#include <QDate>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {

namespace {

int releaseYearFromDate(const QString &releaseDate)
{
    if (releaseDate.size() == 4) {
        bool ok = false;
        const int year = releaseDate.toInt(&ok);
        return ok ? year : 0;
    }

    const QDate parsed = QDate::fromString(releaseDate, Qt::ISODate);
    return parsed.isValid() ? parsed.year() : 0;
}

} // namespace

QList<SearchResult> CompendiumProvider::searchByName(const QString &title,
                                                     const QString &system,
                                                     const QString &region)
{
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
    query.prepare(QStringLiteral(
        "SELECT DISTINCT f.game_id "
        "FROM games_fts f "
        "JOIN games g ON g.game_id = f.game_id "
        "WHERE games_fts MATCH ? "
        "AND (? = 0 OR g.system_id = ?) "
        "AND (? = '' OR UPPER(COALESCE(g.primary_region_code, '')) = ?) "
        "ORDER BY rank "
        "LIMIT 10"));
    query.addBindValue(ftsExpr);
    query.addBindValue(systemId);
    query.addBindValue(systemId);
    query.addBindValue(regionCode);
    query.addBindValue(regionCode);
    usedFts = query.exec();

    if (!usedFts) {
        // Fallback to LIKE for DBs without FTS5 or malformed queries
        const QString likePattern = QStringLiteral("%%1%").arg(searchTerm);
        query.prepare(QStringLiteral(
            "SELECT DISTINCT g.game_id "
            "FROM games g "
            "WHERE (? = 0 OR g.system_id = ?) "
            "AND (? = '' OR UPPER(COALESCE(g.primary_region_code, '')) = ?) "
            "AND ("
            "    LOWER(g.canonical_title) LIKE LOWER(?) "
            "    OR EXISTS ("
            "        SELECT 1 FROM game_names gn "
            "        WHERE gn.game_id = g.game_id AND LOWER(gn.name_text) LIKE LOWER(?)"
            "    )"
            ") "
            "ORDER BY LOWER(g.canonical_title) "
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

    // Collect result IDs first, then batch-fetch all game data in a single
    // JOIN query instead of calling fetchGameMetadata() per row (N+1).
    QStringList ids;
    while (query.next())
        ids.append(query.value(0).toString());

    if (ids.isEmpty())
        return results;

    QStringList placeholders;
    placeholders.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i)
        placeholders.append(QStringLiteral("?"));

    QSqlQuery batchQuery(db);
    batchQuery.prepare(QStringLiteral(
        "SELECT g.game_id, g.canonical_title, g.primary_region_code, "
        "       g.release_date, g.release_year, s.internal_name "
        "FROM games g JOIN systems s ON s.system_id = g.system_id "
        "WHERE g.game_id IN (%1)").arg(placeholders.join(QLatin1Char(',')))
    );
    for (const QString &id : ids)
        batchQuery.addBindValue(id);

    if (!batchQuery.exec()) {
        qWarning() << "CompendiumProvider::searchByName batch query failed:"
                   << batchQuery.lastError().text();
        return results;
    }

    const QString loweredSearch = searchTerm.toLower();
    while (batchQuery.next()) {
        const QString gameId = batchQuery.value(0).toString();
        const QString title  = batchQuery.value(1).toString();
        if (gameId.isEmpty() || title.isEmpty())
            continue;

        SearchResult result;
        result.id     = gameId;
        result.title  = title;
        result.region = batchQuery.value(2).toString();
        const QString releaseDate = batchQuery.value(3).toString();
        result.releaseYear = !releaseDate.isEmpty()
            ? releaseYearFromDate(releaseDate)
            : batchQuery.value(4).toInt();
        result.system = batchQuery.value(5).toString();

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

GameMetadata CompendiumProvider::getByHash(const QString &hash, const QString &system)
{
    QString normalizedHash;
    const QString hashType = detectHashType(hash, normalizedHash);
    if (hashType.isEmpty()) {
        return {};
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return {};
    }

    const int systemId = resolveSystemId(system);

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT gs.game_id, gs.source_entry_key "
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
        return {};
    }
    if (!query.next()) {
        return {};
    }

    const QString gameId = query.value(0).toString();
    const QString sourceEntryKey = query.value(1).toString();

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
    }
    return metadata;
}

GameMetadata CompendiumProvider::getBySerial(const QString &serial, const QString &system)
{
    const QString trimmedSerial = serial.trimmed();
    if (trimmedSerial.isEmpty()) {
        return {};
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return {};
    }

    const int systemId = resolveSystemId(system);

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT gs.game_id, gs.source_entry_key "
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
        return {};
    }
    if (!query.next()) {
        return {};
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
    }
    return metadata;
}

GameMetadata CompendiumProvider::getById(const QString &id)
{
    return fetchGameMetadata(id);
}

ArtworkUrls CompendiumProvider::getArtwork(const QString &id)
{
    if (id.isEmpty()) {
        return {};
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return {};
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT g.canonical_title, s.libretro_name "
        "FROM games g "
        "JOIN systems s ON s.system_id = g.system_id "
        "WHERE g.game_id = ?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next()) {
        return {};
    }

    const QString title = query.value(0).toString();
    const QString libretroName = query.value(1).toString();
    if (title.isEmpty() || libretroName.isEmpty()) {
        return {};
    }

    ArtworkUrls artwork;
    const QStringList boxCandidates =
        Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(
            libretroName, title, QStringLiteral("Named_Boxarts"));
    const QStringList snapCandidates =
        Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(
            libretroName, title, QStringLiteral("Named_Snaps"));
    const QStringList titleCandidates =
        Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(
            libretroName, title, QStringLiteral("Named_Titles"));
    if (!boxCandidates.isEmpty()) {
        artwork.boxFront = QUrl(boxCandidates.first());
    }
    if (!snapCandidates.isEmpty()) {
        artwork.screenshot = QUrl(snapCandidates.first());
    }
    if (!titleCandidates.isEmpty()) {
        artwork.titleScreen = QUrl(titleCandidates.first());
    }
    return artwork;
}

} // namespace Remus
