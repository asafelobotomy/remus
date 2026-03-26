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
    // Local DAT files don't contain artwork URLs
    // This would require a separate artwork database or online provider
    Q_UNUSED(id);
    return ArtworkUrls();
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
    }
    
    qDebug() << "LocalDatabaseProvider:" << systemName 
             << "- CRC32:" << crc32Count 
             << "MD5:" << md5Count 
             << "SHA1:" << sha1Count;
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
