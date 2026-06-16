#include "gametdb_provider.h"
#include "../core/match_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QDebug>

namespace Remus {

GameTDBProvider::GameTDBProvider(QObject *parent)
    : MetadataProvider(parent) { }

GameTDBProvider::~GameTDBProvider() = default;

// ============================================================================
// Database Loading
// ============================================================================

int GameTDBProvider::loadDatabases(const QString &directory) {
    QDir dir(directory);
    if (!dir.exists()) {
        return 0;
    }

    int totalLoaded = 0;
    const QStringList xmlFiles = dir.entryList({ QStringLiteral("*.xml") }, QDir::Files);
    for (const QString &fileName : xmlFiles) {
        int count = loadDatabase(dir.filePath(fileName));
        totalLoaded += count;
    }
    return totalLoaded;
}

int GameTDBProvider::loadDatabase(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "GameTDB: cannot open" << filePath;
        return 0;
    }

    // Infer default platform from filename (e.g. wiitdb.xml → Wii)
    const QString baseName = QFileInfo(filePath).baseName().toLower();
    QString defaultType;
    if (baseName.startsWith(QLatin1String("wiitdb")))
        defaultType = QStringLiteral("Wii");
    else if (baseName.startsWith(QLatin1String("dstdb")))
        defaultType = QStringLiteral("DS");
    else if (baseName.startsWith(QLatin1String("3dstdb")))
        defaultType = QStringLiteral("3DS");
    else if (baseName.startsWith(QLatin1String("wiiutdb")))
        defaultType = QStringLiteral("WiiU");
    else if (baseName.startsWith(QLatin1String("switchtdb")))
        defaultType = QStringLiteral("Switch");
    else if (baseName.startsWith(QLatin1String("ps3tdb")))
        defaultType = QStringLiteral("PS3");

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
                if (ok)
                    current.players = val;

            } else if (inGame && elemName == QLatin1String("date")) {
                bool ok = false;
                int y = xml.attributes().value(QLatin1String("year")).toInt(&ok);
                if (ok)
                    current.year = y;
                int m = xml.attributes().value(QLatin1String("month")).toInt(&ok);
                if (ok)
                    current.month = m;
                int d = xml.attributes().value(QLatin1String("day")).toInt(&ok);
                if (ok)
                    current.day = d;

            } else if (inGame && elemName == QLatin1String("rom")) {
                QString crc = xml.attributes().value(QLatin1String("crc")).toString().toUpper();
                QString md5 = xml.attributes().value(QLatin1String("md5")).toString().toUpper();
                QString sha1 = xml.attributes().value(QLatin1String("sha1")).toString().toUpper();
                if (!crc.isEmpty())
                    current.crc32 = crc;
                if (!md5.isEmpty())
                    current.md5 = md5;
                if (!sha1.isEmpty())
                    current.sha1 = sha1;
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
                    // Title index: first entry for a normalized title wins.
                    // Used by the enrichment pipeline for O(1) name fallback.
                    const QString normTitle = current.title.trimmed().toLower();
                    if (!normTitle.isEmpty() && !m_titleIndex.contains(normTitle)) {
                        m_titleIndex.insert(normTitle, current.id);
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

QString GameTDBProvider::gameIdByNormalizedTitle(const QString &normalizedTitle) const {
    QMutexLocker locker(&m_mutex);
    return m_titleIndex.value(normalizedTitle);
}

QList<SearchResult> GameTDBProvider::searchByName(const QString &title, const QString &system, const QString &region) {
    QList<SearchResult> results;
    QMutexLocker locker(&m_mutex);

    const QString needle = title.toLower();
    for (auto it = m_idIndex.constBegin(); it != m_idIndex.constEnd(); ++it) {
        const GameTDBEntry &entry = it.value();
        if (!system.isEmpty() && entry.type.compare(system, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!region.isEmpty() && !entry.region.contains(region, Qt::CaseInsensitive)) {
            continue;
        }
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

GameMetadata GameTDBProvider::getByHash(const QString &hash, const QString &system) {
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

GameMetadata GameTDBProvider::getByHashes(const QString &crc32, const QString &md5, const QString &sha1,
    const QString &system, const QString &preferredHashType) {
    const QStringList candidates = orderedMatchHashValues(preferredHashType, crc32, md5, sha1);
    for (const QString &candidate : candidates) {
        const GameMetadata metadata = getByHash(candidate, system);
        if (!metadata.title.isEmpty()) {
            return metadata;
        }
    }
    return GameMetadata();
}

GameMetadata GameTDBProvider::getById(const QString &id) {
    QMutexLocker locker(&m_mutex);
    if (!m_idIndex.contains(id)) {
        return GameMetadata();
    }
    return entryToMetadata(m_idIndex[id]);
}

QString GameTDBProvider::normalizeHash(const QString &hash) const {
    return hash.trimmed().toUpper().remove(QLatin1Char(' '));
}

} // namespace Remus
