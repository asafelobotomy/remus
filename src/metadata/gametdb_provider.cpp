#include "gametdb_provider.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QUrl>
#include <QDebug>

namespace Remus {

GameTDBProvider::GameTDBProvider(QObject *parent)
    : MetadataProvider(parent)
{
}

GameTDBProvider::~GameTDBProvider() = default;

// ============================================================================
// Database Loading
// ============================================================================

int GameTDBProvider::loadDatabases(const QString &directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        return 0;
    }

    int totalLoaded = 0;
    const QStringList xmlFiles = dir.entryList({QStringLiteral("*.xml")}, QDir::Files);
    for (const QString &fileName : xmlFiles) {
        int count = loadDatabase(dir.filePath(fileName));
        totalLoaded += count;
    }
    return totalLoaded;
}

int GameTDBProvider::loadDatabase(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "GameTDB: cannot open" << filePath;
        return 0;
    }

    // Infer default platform from filename (e.g. wiitdb.xml → Wii)
    const QString baseName = QFileInfo(filePath).baseName().toLower();
    QString defaultType;
    if (baseName.startsWith(QLatin1String("wiitdb")))       defaultType = QStringLiteral("Wii");
    else if (baseName.startsWith(QLatin1String("dstdb")))   defaultType = QStringLiteral("DS");
    else if (baseName.startsWith(QLatin1String("3dstdb")))  defaultType = QStringLiteral("3DS");
    else if (baseName.startsWith(QLatin1String("wiiutdb"))) defaultType = QStringLiteral("WiiU");
    else if (baseName.startsWith(QLatin1String("switchtdb"))) defaultType = QStringLiteral("Switch");
    else if (baseName.startsWith(QLatin1String("ps3tdb")))  defaultType = QStringLiteral("PS3");

    QXmlStreamReader xml(&file);
    int loaded = 0;

    GameTDBEntry current;
    bool inGame = false;
    bool inLocaleEN = false;
    bool inLocaleFallback = false;
    bool hasENLocale = false;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            const auto elemName = xml.name();

            if (elemName == QLatin1String("game")) {
                current = GameTDBEntry();
                inGame = true;
                hasENLocale = false;

            } else if (inGame && elemName == QLatin1String("id")) {
                current.id = xml.readElementText();

            } else if (inGame && elemName == QLatin1String("type")) {
                current.type = xml.readElementText();
                if (current.type.isEmpty()) {
                    current.type = defaultType;
                }

            } else if (inGame && elemName == QLatin1String("region")) {
                current.region = xml.readElementText();

            } else if (inGame && elemName == QLatin1String("locale")) {
                QString lang = xml.attributes().value(QLatin1String("lang")).toString();
                if (lang == QLatin1String("EN")) {
                    inLocaleEN = true;
                    hasENLocale = true;
                } else if (!hasENLocale && lang == QLatin1String("JA")) {
                    // Fallback to Japanese if no English locale yet
                    inLocaleFallback = true;
                }

            } else if ((inLocaleEN || inLocaleFallback) && elemName == QLatin1String("title")) {
                QString titleText = xml.readElementText();
                if (inLocaleEN || current.title.isEmpty()) {
                    current.title = titleText;
                }

            } else if ((inLocaleEN || inLocaleFallback) && elemName == QLatin1String("synopsis")) {
                QString synopsisText = xml.readElementText();
                if (inLocaleEN || current.synopsis.isEmpty()) {
                    current.synopsis = synopsisText;
                }

            } else if (inGame && elemName == QLatin1String("developer")) {
                current.developer = xml.readElementText();

            } else if (inGame && elemName == QLatin1String("publisher")) {
                current.publisher = xml.readElementText();

            } else if (inGame && elemName == QLatin1String("genre")) {
                current.genre = xml.readElementText();

            } else if (inGame && elemName == QLatin1String("input")) {
                bool ok = false;
                int val = xml.attributes().value(QLatin1String("players")).toInt(&ok);
                if (ok) current.players = val;

            } else if (inGame && elemName == QLatin1String("date")) {
                bool ok = false;
                int y = xml.attributes().value(QLatin1String("year")).toInt(&ok);
                if (ok) current.year = y;
                int m = xml.attributes().value(QLatin1String("month")).toInt(&ok);
                if (ok) current.month = m;
                int d = xml.attributes().value(QLatin1String("day")).toInt(&ok);
                if (ok) current.day = d;

            } else if (inGame && elemName == QLatin1String("rom")) {
                QString crc = xml.attributes().value(QLatin1String("crc")).toString().toUpper();
                QString md5 = xml.attributes().value(QLatin1String("md5")).toString().toUpper();
                QString sha1 = xml.attributes().value(QLatin1String("sha1")).toString().toUpper();
                if (!crc.isEmpty()) current.crc32 = crc;
                if (!md5.isEmpty()) current.md5 = md5;
                if (!sha1.isEmpty()) current.sha1 = sha1;
            }

        } else if (token == QXmlStreamReader::EndElement) {
            const auto elemName = xml.name();

            if (elemName == QLatin1String("locale")) {
                inLocaleEN = false;
                inLocaleFallback = false;

            } else if (elemName == QLatin1String("game") && inGame) {
                inGame = false;
                if (!current.id.isEmpty() && !current.title.isEmpty()) {
                    QMutexLocker locker(&m_mutex);
                    m_idIndex.insert(current.id, current);

                    if (!current.crc32.isEmpty()) {
                        m_crc32Index.insert(current.crc32, current.id);
                    }
                    if (!current.md5.isEmpty()) {
                        m_md5Index.insert(current.md5, current.id);
                    }
                    if (!current.sha1.isEmpty()) {
                        m_sha1Index.insert(current.sha1, current.id);
                    }
                    ++loaded;
                }
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "GameTDB: XML parse error in" << filePath << ":" << xml.errorString();
    }

    return loaded;
}

// ============================================================================
// MetadataProvider Interface
// ============================================================================

QList<SearchResult> GameTDBProvider::searchByName(const QString &title,
                                                   const QString &system,
                                                   const QString &region)
{
    Q_UNUSED(system)
    Q_UNUSED(region)

    QList<SearchResult> results;
    QMutexLocker locker(&m_mutex);

    const QString needle = title.toLower();
    for (auto it = m_idIndex.constBegin(); it != m_idIndex.constEnd(); ++it) {
        const GameTDBEntry &entry = it.value();
        if (entry.title.toLower().contains(needle)) {
            SearchResult sr;
            sr.id = entry.id;
            sr.title = entry.title;
            sr.system = entry.type;
            sr.region = entry.region;
            sr.releaseYear = entry.year;
            sr.matchScore = (entry.title.toLower() == needle) ? 1.0f : 0.5f;
            sr.provider = QStringLiteral("GameTDB");
            results.append(sr);
        }
    }
    return results;
}

GameMetadata GameTDBProvider::getByHash(const QString &hash, const QString &system)
{
    Q_UNUSED(system)

    QMutexLocker locker(&m_mutex);
    QString normalized = normalizeHash(hash);

    // Try CRC32 first, then MD5, then SHA1
    QString gameId;
    if (m_crc32Index.contains(normalized)) {
        gameId = m_crc32Index[normalized];
    } else if (m_md5Index.contains(normalized)) {
        gameId = m_md5Index[normalized];
    } else if (m_sha1Index.contains(normalized)) {
        gameId = m_sha1Index[normalized];
    }

    if (gameId.isEmpty() || !m_idIndex.contains(gameId)) {
        return GameMetadata();
    }

    return entryToMetadata(m_idIndex[gameId]);
}

GameMetadata GameTDBProvider::getById(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_idIndex.contains(id)) {
        return GameMetadata();
    }
    return entryToMetadata(m_idIndex[id]);
}

ArtworkUrls GameTDBProvider::getArtwork(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    if (!m_idIndex.contains(id)) {
        return ArtworkUrls();
    }

    const GameTDBEntry &entry = m_idIndex[id];
    QString platform = cdnPlatformCode(entry.type);
    QString regionCdn = cdnRegionCode(entry.region);

    if (platform.isEmpty()) {
        return ArtworkUrls();
    }

    ArtworkUrls artwork;

    // Wii/GameCube use .png; others use .jpg for covers
    bool isPng = (platform == QLatin1String("wii"));
    QString ext = isPng ? QStringLiteral("png") : QStringLiteral("jpg");
    QString coverType = isPng ? QStringLiteral("cover") : QStringLiteral("coverHQ");

    artwork.boxFront = QUrl(buildArtworkUrl(platform, coverType, regionCdn, entry.id, ext));

    // 3D box only for Wii/GameCube
    if (isPng) {
        artwork.boxFull = QUrl(buildArtworkUrl(platform, QStringLiteral("cover3D"), regionCdn, entry.id, ext));
    }

    // Disc art for Wii/GameCube/PS3
    if (platform == QLatin1String("wii") || platform == QLatin1String("ps3")) {
        QString discExt = isPng ? QStringLiteral("png") : QStringLiteral("jpg");
        QString discType = isPng ? QStringLiteral("disc") : QStringLiteral("discHQ");
        artwork.boxBack = QUrl(buildArtworkUrl(platform, discType, regionCdn, entry.id, discExt));
    }

    return artwork;
}

// ============================================================================
// Static Helpers
// ============================================================================

QString GameTDBProvider::cdnPlatformCode(const QString &gameType)
{
    // GameTDB type -> CDN platform code
    // GameCube shares the "wii" CDN path
    if (gameType == QLatin1String("Wii") || gameType == QLatin1String("WiiWare") ||
        gameType == QLatin1String("GameCube") || gameType == QLatin1String("Channel")) {
        return QStringLiteral("wii");
    }
    if (gameType == QLatin1String("DS") || gameType == QLatin1String("DSiWare")) {
        return QStringLiteral("ds");
    }
    if (gameType == QLatin1String("3DS") || gameType == QLatin1String("3DSWare")) {
        return QStringLiteral("3ds");
    }
    if (gameType == QLatin1String("WiiU") || gameType == QLatin1String("WiiUWare")) {
        return QStringLiteral("wiiu");
    }
    if (gameType == QLatin1String("Switch")) {
        return QStringLiteral("switch");
    }
    if (gameType == QLatin1String("PS3")) {
        return QStringLiteral("ps3");
    }
    return QString();
}

QString GameTDBProvider::cdnRegionCode(const QString &gameRegion)
{
    // GameTDB region string -> CDN region code
    if (gameRegion.contains(QLatin1String("NTSC-U")) ||
        gameRegion.contains(QLatin1String("USA")) ||
        gameRegion == QLatin1String("ALL")) {
        return QStringLiteral("US");
    }
    if (gameRegion.contains(QLatin1String("NTSC-J")) ||
        gameRegion.contains(QLatin1String("JPN")) ||
        gameRegion == QLatin1String("NTSC-J")) {
        return QStringLiteral("JA");
    }
    if (gameRegion.contains(QLatin1String("PAL"))) {
        return QStringLiteral("EN");
    }
    if (gameRegion.contains(QLatin1String("KOR"))) {
        return QStringLiteral("KO");
    }
    // Default to US
    return QStringLiteral("US");
}

QString GameTDBProvider::buildArtworkUrl(const QString &platformCode,
                                          const QString &artType,
                                          const QString &regionCode,
                                          const QString &gameId,
                                          const QString &extension)
{
    // URL: https://art.gametdb.com/{platform}/{artType}/{region}/{gameId}.{ext}
    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(QStringLiteral("art.gametdb.com"));
    url.setPath(QLatin1Char('/') + platformCode +
                QLatin1Char('/') + artType +
                QLatin1Char('/') + regionCode +
                QLatin1Char('/') + gameId +
                QLatin1Char('.') + extension);
    return url.toString(QUrl::FullyEncoded);
}

// ============================================================================
// Private Helpers
// ============================================================================

GameMetadata GameTDBProvider::entryToMetadata(const GameTDBEntry &entry) const
{
    GameMetadata metadata;
    metadata.id = entry.id;
    metadata.title = entry.title;
    metadata.system = entry.type;
    metadata.region = entry.region;
    metadata.publisher = entry.publisher;
    metadata.developer = entry.developer;
    metadata.description = entry.synopsis;
    metadata.players = entry.players;
    metadata.providerId = QStringLiteral("gametdb");
    metadata.matchMethod = QStringLiteral("hash");

    // Genre: split comma-separated string
    if (!entry.genre.isEmpty()) {
        metadata.genres = entry.genre.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString &g : metadata.genres) {
            g = g.trimmed();
        }
    }

    // Release date as ISO 8601
    if (entry.year > 0) {
        if (entry.month > 0 && entry.day > 0) {
            metadata.releaseDate = QStringLiteral("%1-%2-%3")
                .arg(entry.year, 4, 10, QLatin1Char('0'))
                .arg(entry.month, 2, 10, QLatin1Char('0'))
                .arg(entry.day, 2, 10, QLatin1Char('0'));
        } else {
            metadata.releaseDate = QString::number(entry.year);
        }
    }

    metadata.matchScore = 1.0f;

    // External IDs
    metadata.externalIds.insert(QStringLiteral("gametdb"), entry.id);

    // Artwork URLs via CDN
    QString platform = cdnPlatformCode(entry.type);
    QString regionCdn = cdnRegionCode(entry.region);
    if (!platform.isEmpty()) {
        bool isPng = (platform == QLatin1String("wii"));
        QString ext = isPng ? QStringLiteral("png") : QStringLiteral("jpg");
        QString coverType = isPng ? QStringLiteral("cover") : QStringLiteral("coverHQ");

        metadata.boxArtUrl = buildArtworkUrl(platform, coverType, regionCdn, entry.id, ext);
    }

    return metadata;
}

QString GameTDBProvider::normalizeHash(const QString &hash) const
{
    return hash.trimmed().toUpper().remove(QLatin1Char(' '));
}

} // namespace Remus
