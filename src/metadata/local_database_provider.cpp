#include "local_database_provider.h"
#include "../core/constants/match_methods.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

namespace Remus {

LocalDatabaseProvider::LocalDatabaseProvider(QObject *parent)
    : MetadataProvider(parent)
{
    qDebug() << "LocalDatabaseProvider: Initialized";
}

LocalDatabaseProvider::~LocalDatabaseProvider()
{
    qDebug() << "LocalDatabaseProvider: Total entries indexed:" << m_totalEntries;
}

int LocalDatabaseProvider::loadDatabases(const QString &directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        qWarning() << "LocalDatabaseProvider: Directory not found:" << directory;
        return 0;
    }
    
    QStringList filters;
    filters << "*.dat";
    QFileInfoList datFiles = dir.entryInfoList(filters, QDir::Files);
    
    qDebug() << "LocalDatabaseProvider: Found" << datFiles.size() << "DAT files in" << directory;
    
    int totalLoaded = 0;
    int current = 0;
    int total = datFiles.size();
    
    for (const QFileInfo &fileInfo : datFiles) {
        current++;
        emit loadingProgress(current, total);
        
        int loaded = loadDatabase(fileInfo.absoluteFilePath());
        totalLoaded += loaded;
    }
    
    qDebug() << "LocalDatabaseProvider: Loaded" << totalLoaded << "total entries from" << datFiles.size() << "databases";
    return totalLoaded;
}

int LocalDatabaseProvider::loadMetadata(const QString &metadataDir)
{
    int loaded = m_metadataParser.loadAll(metadataDir);
    qDebug() << "LocalDatabaseProvider: Enrichment metadata loaded for" << loaded << "unique CRCs";
    return loaded;
}

int LocalDatabaseProvider::loadDatabase(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString systemName = fileInfo.baseName(); // e.g., "Sega - Mega Drive - Genesis"
    
    qDebug() << "LocalDatabaseProvider: Loading" << systemName << "from" << filePath;
    
    // Parse ClrMamePro DAT file
    QMap<QString, QString> header = ClrMameProParser::parseHeader(filePath);
    QList<ClrMameProEntry> entries = ClrMameProParser::parse(filePath);
    
    if (entries.isEmpty()) {
        qWarning() << "LocalDatabaseProvider: No entries parsed from" << filePath;
        return 0;
    }
    
    // Index entries
    indexEntries(entries, systemName);
    
    // Update statistics
    m_systemStats[systemName] = entries.size();
    m_totalEntries += entries.size();
    
    // Store DAT metadata
    DatMetadata metadata;
    metadata.name = header.value("name", systemName);
    metadata.version = header.value("version", "unknown");
    metadata.description = header.value("description", "");
    metadata.filePath = filePath;
    metadata.loadedAt = QDateTime::currentDateTime();
    metadata.entryCount = entries.size();
    
    QMutexLocker locker(&m_mutex);
    m_datMetadata[systemName] = metadata;
    locker.unlock();
    
    emit databaseLoaded(systemName, entries.size());
    
    qDebug() << "LocalDatabaseProvider: Indexed" << entries.size() << "entries for" << systemName
             << "(Version:" << metadata.version << ")";
    
    return entries.size();
}

QMap<QString, int> LocalDatabaseProvider::getDatabaseStats() const
{
    QMutexLocker locker(&m_mutex);
    return m_systemStats;
}

QList<SearchResult> LocalDatabaseProvider::searchByName(const QString &title,
                                                         const QString &system,
                                                         const QString &region)
{
    QMutexLocker locker(&m_mutex);
    QList<SearchResult> results;
    
    // Name-based search in local database is less accurate
    // We primarily rely on hash-based matching
    // This is a fallback for when no hash is available
    
    QString searchLower = title.toLower();
    
    // Search CRC32 index (most entries)
    for (auto it = m_crc32Index.constBegin(); it != m_crc32Index.constEnd(); ++it) {
        const ClrMameProEntry &entry = it.value();
        
        // Simple substring matching
        if (entry.gameName.toLower().contains(searchLower)) {
            SearchResult result;
            result.id = entry.crc32; // Use CRC32 as ID
            result.title = entry.gameName;
            result.system = system;
            
            // Calculate match score
            if (entry.gameName.toLower() == searchLower) {
                result.matchScore = 1.0f; // Exact match
            } else if (entry.gameName.toLower().startsWith(searchLower)) {
                result.matchScore = 0.9f; // Starts with
            } else {
                result.matchScore = 0.7f; // Contains
            }
            
            // Filter by region if specified (extract from gameName)
            if (!region.isEmpty()) {
                if (!entry.gameName.contains(region, Qt::CaseInsensitive)) {
                    continue; // Skip non-matching regions
                }
            }
            
            results.append(result);
            
            if (results.size() >= 10) {
                break; // Limit results
            }
        }
    }
    
    // Sort by match score
    std::sort(results.begin(), results.end(), [](const SearchResult &a, const SearchResult &b) {
        return a.matchScore > b.matchScore;
    });
    
    qDebug() << "LocalDatabaseProvider: Name search for" << title << "found" << results.size() << "results";
    return results;
}

GameMetadata LocalDatabaseProvider::getByHash(const QString &hash, const QString &system)
{
    QMutexLocker locker(&m_mutex);
    
    QString normalizedHash = normalizeHash(hash);
    ClrMameProEntry entry;
    bool found = false;
    
    // Try CRC32 first (most common for cartridges)
    if (normalizedHash.length() == 8) {
        if (m_crc32Index.contains(normalizedHash)) {
            entry = m_crc32Index.value(normalizedHash);
            found = true;
            qDebug() << "LocalDatabaseProvider: CRC32 match found:" << entry.gameName;
        }
    }
    // Try MD5 (32 chars)
    else if (normalizedHash.length() == 32) {
        if (m_md5Index.contains(normalizedHash)) {
            entry = m_md5Index.value(normalizedHash);
            found = true;
            qDebug() << "LocalDatabaseProvider: MD5 match found:" << entry.gameName;
        }
    }
    // Try SHA1 (40 chars)
    else if (normalizedHash.length() == 40) {
        if (m_sha1Index.contains(normalizedHash)) {
            entry = m_sha1Index.value(normalizedHash);
            found = true;
            qDebug() << "LocalDatabaseProvider: SHA1 match found:" << entry.gameName;
        }
    }
    
    if (found) {
        return datEntryToMetadata(entry);
    }
    
    // Not found
    qDebug() << "LocalDatabaseProvider: No hash match for" << normalizedHash.left(8) << "...";
    return GameMetadata();
}

GameMetadata LocalDatabaseProvider::getById(const QString &id)
{
    // ID is the CRC32/MD5/SHA1 hash
    return getByHash(id, QString());
}

ArtworkUrls LocalDatabaseProvider::getArtwork(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    QString normalized = normalizeHash(id);

    // Find the entry and system name via CRC32 lookup
    if (!m_crc32Index.contains(normalized) || !m_hashToSystem.contains(normalized)) {
        return ArtworkUrls();
    }

    const ClrMameProEntry &entry = m_crc32Index[normalized];
    const QString &systemName = m_hashToSystem[normalized];

    ArtworkUrls artwork;
    QStringList boxCandidates = generateThumbnailCandidates(systemName, entry.gameName, QStringLiteral("Named_Boxarts"));
    QStringList snapCandidates = generateThumbnailCandidates(systemName, entry.gameName, QStringLiteral("Named_Snaps"));
    QStringList titleCandidates = generateThumbnailCandidates(systemName, entry.gameName, QStringLiteral("Named_Titles"));
    if (!boxCandidates.isEmpty()) { artwork.boxFront = QUrl(boxCandidates.first()); }
    if (!snapCandidates.isEmpty()) { artwork.screenshot = QUrl(snapCandidates.first()); }
    if (!titleCandidates.isEmpty()) { artwork.titleScreen = QUrl(titleCandidates.first()); }
    return artwork;
}

QString LocalDatabaseProvider::sanitizeThumbnailName(const QString &gameName)
{
    // Per libretro convention: &*/:\<>?\| are replaced with _
    QString sanitized = gameName;
    static const QString invalidChars = QStringLiteral("&*/:\\<>?|\"");
    for (QChar ch : invalidChars) {
        sanitized.replace(ch, QLatin1Char('_'));
    }
    return sanitized;
}

QString LocalDatabaseProvider::stripLanguageTags(const QString &gameName)
{
    // Matches parenthetical groups that contain only ISO 639-1 language codes
    // e.g. (En), (En,Ja), (En,Fr,De,Es,It), (Ja)
    // Pattern: ( OptWS  Code  {, Code}*  OptWS )
    static const QRegularExpression langTagRe(
        QStringLiteral("\\s*\\(\\s*(?:[A-Z][a-z])(?:,\\s*[A-Z][a-z])*\\s*\\)"));

    QString result = gameName;
    result.remove(langTagRe);
    return result.trimmed();
}

QStringList LocalDatabaseProvider::generateThumbnailCandidates(const QString &systemName,
                                                                const QString &gameName,
                                                                const QString &type)
{
    QStringList candidates;
    QSet<QString> seen;

    auto addCandidate = [&](const QString &name) {
        QString url = buildThumbnailUrl(systemName, name, type);
        if (!seen.contains(url)) {
            seen.insert(url);
            candidates.append(url);
        }
    };

    // 1. Exact DAT name (most specific)
    addCandidate(gameName);

    // 2. Language tags stripped — CDN often omits (En), (En,Ja) etc.
    QString stripped = stripLanguageTags(gameName);
    if (stripped != gameName) {
        addCandidate(stripped);
    }

    return candidates;
}

QString LocalDatabaseProvider::buildThumbnailUrl(const QString &systemName,
                                                  const QString &gameName,
                                                  const QString &type)
{
    // URL: https://thumbnails.libretro.com/{System}/{Type}/{SanitizedName}.png
    // Path components are percent-encoded (spaces → %20, etc.)
    QString sanitized = sanitizeThumbnailName(gameName);
    QString path = systemName + QLatin1Char('/') + type + QLatin1Char('/') + sanitized + QStringLiteral(".png");

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(QStringLiteral("thumbnails.libretro.com"));
    url.setPath(QLatin1Char('/') + path, QUrl::DecodedMode);
    return url.toString(QUrl::FullyEncoded);
}

void LocalDatabaseProvider::indexEntries(const QList<ClrMameProEntry> &entries, const QString &systemName)
{
    QMutexLocker locker(&m_mutex);
    
    int crc32Count = 0, md5Count = 0, sha1Count = 0;
    
    for (const ClrMameProEntry &entry : entries) {
        // Index by CRC32 (primary for cartridges)
        if (!entry.crc32.isEmpty()) {
            QString normalized = normalizeHash(entry.crc32);
            m_crc32Index[normalized] = entry;
            m_hashToSystem[normalized] = systemName;
            crc32Count++;
        }
        
        // Index by MD5 (discs)
        if (!entry.md5.isEmpty()) {
            QString normalized = normalizeHash(entry.md5);
            m_md5Index[normalized] = entry;
            md5Count++;
        }
        
        // Index by SHA1 (discs)
        if (!entry.sha1.isEmpty()) {
            QString normalized = normalizeHash(entry.sha1);
            m_sha1Index[normalized] = entry;
            sha1Count++;
        }

        // Index by serial (for entries with serial, especially those
        // without any hash like GameCube/Wii/Saturn DAT entries)
        if (!entry.serial.isEmpty()) {
            m_serialIndex.insert(entry.serial.toUpper().trimmed(), entry);
        }
    }
    
    qDebug() << "LocalDatabaseProvider:" << systemName 
             << "- CRC32:" << crc32Count 
             << "MD5:" << md5Count 
             << "SHA1:" << sha1Count;
}

GameMetadata LocalDatabaseProvider::getMetadataForEntry(const MultiSignalMatch &match) const
{
    return datEntryToMetadata(match.entry);
}

void LocalDatabaseProvider::enrichFromLibretro(GameMetadata &metadata, const QString &crc32) const
{
    auto apply = [&metadata](const LibretroMetadata &e) {
        if (!e.genre.isEmpty() && metadata.genres.isEmpty())
            metadata.genres = QStringList{e.genre};
        if (!e.developer.isEmpty() && metadata.developer.isEmpty())
            metadata.developer = e.developer;
        if (!e.publisher.isEmpty() && metadata.publisher.isEmpty())
            metadata.publisher = e.publisher;
        if (e.maxUsers > 0 && metadata.players == 0)
            metadata.players = e.maxUsers;
        if (e.releaseYear > 0 && metadata.releaseDate.isEmpty())
            metadata.releaseDate = QString::number(e.releaseYear);
    };

    // Try CRC first (cartridge systems)
    if (!crc32.isEmpty()) {
        const QString normalized = crc32.toUpper().trimmed();
        if (m_metadataParser.contains(normalized)) {
            apply(m_metadataParser.lookup(normalized));
            return;
        }
    }

    // Try name-based lookup (disc systems where CRC doesn't match libretro data)
    if (!metadata.title.isEmpty()) {
        LibretroMetadata byName = m_metadataParser.lookupByName(metadata.title);
        if (!byName.developer.isEmpty() || !byName.publisher.isEmpty()) {
            apply(byName);
            return;
        }
    }
}

GameMetadata LocalDatabaseProvider::datEntryToMetadata(const ClrMameProEntry &entry) const
{
    GameMetadata metadata;
    
    // Use gameName as title (parent game)
    metadata.title = entry.gameName;
    
    // Try to extract region from gameName (e.g., "Sonic (USA, Europe)")
    QRegularExpression regionRegex("\\(([^)]+)\\)");
    QRegularExpressionMatch match = regionRegex.match(entry.gameName);
    if (match.hasMatch()) {
        QString regionText = match.captured(1);
        // Take first region if comma-separated
        if (regionText.contains(',')) {
            metadata.region = regionText.split(',').first().trimmed();
        } else {
            metadata.region = regionText.trimmed();
        }
    }
    
    // Description uses the description field if available
    if (!entry.description.isEmpty()) {
        metadata.description = entry.description;
    } else {
        // Fallback: clean up game name (remove region markers)
        QString desc = entry.gameName;
        desc.remove(QRegularExpression("\\s*\\([^)]*\\)\\s*")); // Remove (USA), (Rev 1), etc.
        metadata.description = desc.trimmed();
    }
    
    // External ID is the hash
    if (!entry.crc32.isEmpty()) {
        metadata.externalIds["crc32"] = entry.crc32;
    }
    if (!entry.md5.isEmpty()) {
        metadata.externalIds["md5"] = entry.md5;
    }
    if (!entry.sha1.isEmpty()) {
        metadata.externalIds["sha1"] = entry.sha1;
    }
    if (!entry.serial.isEmpty()) {
        metadata.externalIds["serial"] = entry.serial;
    }
    
    // Match score and method
    metadata.matchScore = 1.0f; // Hash match is 100% confidence
    metadata.matchMethod = QString::fromLatin1(Remus::Constants::MatchMethods::HASH);
    
    // Primary enrichment: inline DAT metadata (Redump/GameTDB DATs)
    if (!entry.publisher.isEmpty()) {
        metadata.publisher = entry.publisher;
    }
    if (!entry.developer.isEmpty()) {
        metadata.developer = entry.developer;
    }
    if (entry.releaseYear > 0) {
        metadata.releaseDate = QString::number(entry.releaseYear);
    }
    if (entry.users > 0) {
        metadata.players = entry.users;
    }

    // Secondary enrichment: libretro metadata files (fills gaps not covered by DAT)
    if (!entry.crc32.isEmpty() && m_metadataParser.contains(entry.crc32)) {
        LibretroMetadata enrichment = m_metadataParser.lookup(entry.crc32);
        if (!enrichment.genre.isEmpty() && metadata.genres.isEmpty()) {
            metadata.genres = QStringList{enrichment.genre};
        }
        if (!enrichment.developer.isEmpty() && metadata.developer.isEmpty()) {
            metadata.developer = enrichment.developer;
        }
        if (!enrichment.publisher.isEmpty() && metadata.publisher.isEmpty()) {
            metadata.publisher = enrichment.publisher;
        }
        if (enrichment.maxUsers > 0 && metadata.players == 0) {
            metadata.players = enrichment.maxUsers;
        }
        if (enrichment.releaseYear > 0 && metadata.releaseDate.isEmpty()) {
            metadata.releaseDate = QString::number(enrichment.releaseYear);
        }
    }

    // Construct libretro-thumbnails artwork URLs with fallback candidates
    if (!entry.crc32.isEmpty()) {
        QString normalized = normalizeHash(entry.crc32);
        if (m_hashToSystem.contains(normalized)) {
            const QString &systemName = m_hashToSystem[normalized];
            metadata.system = systemName;

            // Box art: primary candidate first, then fallbacks
            QStringList boxCandidates = generateThumbnailCandidates(
                systemName, entry.gameName, QStringLiteral("Named_Boxarts"));
            if (!boxCandidates.isEmpty()) {
                metadata.boxArtUrl = boxCandidates.first();
            }

            // Screenshots: snap + title primary candidates, then all fallbacks
            QStringList snapCandidates = generateThumbnailCandidates(
                systemName, entry.gameName, QStringLiteral("Named_Snaps"));
            QStringList titleCandidates = generateThumbnailCandidates(
                systemName, entry.gameName, QStringLiteral("Named_Titles"));
            if (!snapCandidates.isEmpty()) {
                metadata.screenshotUrls.append(snapCandidates.first());
            }
            if (!titleCandidates.isEmpty()) {
                metadata.screenshotUrls.append(titleCandidates.first());
            }

            // Append fallback box art candidates (index 1+)
            for (int i = 1; i < boxCandidates.size(); ++i) {
                metadata.screenshotUrls.append(boxCandidates.at(i));
            }
        }
    }

    return metadata;
}

QString LocalDatabaseProvider::normalizeHash(const QString &hash) const
{
    // Remove spaces, convert to uppercase
    return hash.trimmed().toUpper().remove(' ');
}

QList<DatMetadata> LocalDatabaseProvider::getLoadedDats() const
{
    QMutexLocker locker(&m_mutex);
    return m_datMetadata.values();
}

bool LocalDatabaseProvider::isDatNewer(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString systemName = fileInfo.baseName();
    
    // Parse header to get version
    QMap<QString, QString> header = ClrMameProParser::parseHeader(filePath);
    QString newVersion = header.value("version", "");
    
    if (newVersion.isEmpty()) {
        return false; // Can't determine if newer
    }
    
    QMutexLocker locker(&m_mutex);
    if (!m_datMetadata.contains(systemName)) {
        return true; // Not loaded yet, consider it "newer"
    }
    
    QString currentVersion = m_datMetadata[systemName].version;
    
    // Compare version strings
    // Format is typically: YYYY.MM.DD-HHMMSS or YYYY.MM.DD
    // Simple string comparison works for this format
    bool isNewer = newVersion > currentVersion;
    
    if (isNewer) {
        qInfo() << "LocalDatabaseProvider: Update available for" << systemName
                << "- current:" << currentVersion << "new:" << newVersion;
    }
    
    return isNewer;
}

int LocalDatabaseProvider::reloadDatabase(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString systemName = fileInfo.baseName();
    
    qInfo() << "LocalDatabaseProvider: Reloading" << systemName << "from" << filePath;
    
    // Check if it's actually newer
    if (!isDatNewer(filePath)) {
        qWarning() << "LocalDatabaseProvider: File is not newer, skipping reload";
        return -1;
    }
    
    // Clear existing entries for this system
    QMutexLocker locker(&m_mutex);
    if (m_datMetadata.contains(systemName)) {
        int oldCount = m_datMetadata[systemName].entryCount;
        m_totalEntries -= oldCount;
        
        // Remove from indexes (would need to track system per entry for proper cleanup)
        // For now, we'll just reload everything
        qWarning() << "LocalDatabaseProvider: Clearing" << oldCount << "old entries";
    }
    locker.unlock();
    
    // Load new version
    return loadDatabase(filePath);
}

} // namespace Remus
