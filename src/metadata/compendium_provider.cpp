#include "compendium_provider.h"

#include "../core/constants/match_methods.h"
#include "../core/constants/providers.h"
#include "../core/system_resolver.h"
#include "thumbnail_url_helper.h"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

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

CompendiumProvider::CompendiumProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_connectionName(QStringLiteral("compendium_provider_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

CompendiumProvider::~CompendiumProvider()
{
    closeConnection();
}

bool CompendiumProvider::openDatabase(const QString &databasePath)
{
    closeConnection();

    const QFileInfo info(databasePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(info.absoluteFilePath());
    if (!db.open()) {
        closeConnection();
        return false;
    }

    m_databasePath = info.absoluteFilePath();
    ensureFts5Index();
    return true;
}

void CompendiumProvider::ensureFts5Index()
{
    QSqlDatabase db = database();
    if (!db.isOpen())
        return;

    // Create FTS5 virtual table if not present (handles existing DBs without rebuild)
    QSqlQuery create(db);
    create.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS games_fts USING fts5("
        "    game_id UNINDEXED,"
        "    system_id UNINDEXED,"
        "    title_text,"
        "    tokenize='unicode61 remove_diacritics 1'"
        ")"));

    // Populate only when the table is empty (first time or after wipe)
    QSqlQuery countQ(db);
    countQ.exec(QStringLiteral("SELECT COUNT(*) FROM games_fts"));
    if (countQ.next() && countQ.value(0).toInt() > 0)
        return;

    QSqlQuery populate(db);
    populate.exec(QStringLiteral(
        "INSERT INTO games_fts(game_id, system_id, title_text) "
        "SELECT game_id, system_id, canonical_title FROM games "
        "UNION ALL "
        "SELECT gn.game_id, g.system_id, gn.name_text "
        "FROM game_names gn JOIN games g ON gn.game_id = g.game_id"));
}

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
        if (!query.exec())
            return results;
    }

    const QString loweredSearch = searchTerm.toLower();
    while (query.next()) {
        const QString gameId = query.value(0).toString();
        const GameMetadata metadata = fetchGameMetadata(gameId);
        if (metadata.id.isEmpty())
            continue;

        SearchResult result;
        result.id = metadata.id;
        result.title = metadata.title;
        result.system = metadata.system;
        result.region = metadata.region;
        result.releaseYear = releaseYearFromDate(metadata.releaseDate);

        const QString loweredTitle = metadata.title.toLower();
        if (loweredTitle == loweredSearch) {
            result.matchScore = 1.0f;
        } else if (loweredTitle.startsWith(loweredSearch)) {
            result.matchScore = 0.9f;
        } else {
            result.matchScore = 0.7f;
        }

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
    if (!query.exec() || !query.next()) {
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
    if (!query.exec() || !query.next()) {
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

bool CompendiumProvider::isAvailable()
{
    QSqlDatabase db = database();
    return db.isValid() && db.isOpen();
}

QSqlDatabase CompendiumProvider::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

int CompendiumProvider::resolveSystemId(const QString &system) const
{
    const QString systemName = system.trimmed();
    if (systemName.isEmpty()) {
        return 0;
    }

    int systemId = SystemResolver::systemIdByName(systemName);
    if (systemId != 0) {
        return systemId;
    }

    systemId = SystemResolver::systemIdByDatName(systemName);
    if (systemId != 0) {
        return systemId;
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return 0;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT system_id FROM systems "
        "WHERE LOWER(internal_name) = LOWER(?) OR LOWER(display_name) = LOWER(?) "
        "LIMIT 1"));
    query.addBindValue(systemName);
    query.addBindValue(systemName);
    if (!query.exec() || !query.next()) {
        return 0;
    }

    return query.value(0).toInt();
}

void CompendiumProvider::closeConnection()
{
    const QString connectionName = m_connectionName;
    if (!QSqlDatabase::contains(connectionName)) {
        m_databasePath.clear();
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    m_databasePath.clear();
}

QString CompendiumProvider::detectHashType(const QString &hash, QString &normalizedValue)
{
    const QString compact = hash.trimmed().remove(QLatin1Char(' '));
    if (compact.size() == 8) {
        normalizedValue = compact.toUpper();
        return QStringLiteral("crc32");
    }
    if (compact.size() == 32) {
        normalizedValue = compact.toLower();
        return QStringLiteral("md5");
    }
    if (compact.size() == 40) {
        normalizedValue = compact.toLower();
        return QStringLiteral("sha1");
    }

    normalizedValue.clear();
    return {};
}

} // namespace Remus