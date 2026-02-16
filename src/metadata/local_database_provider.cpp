#include "local_database_provider.h"
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
    metadata.matchMethod = "hash";
    
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

QList<MultiSignalMatch> LocalDatabaseProvider::matchROM(const ROMSignals &input) const
{
    QMutexLocker locker(&m_mutex);
    QList<MultiSignalMatch> matches;
    
    qDebug() << "LocalDatabaseProvider: Multi-signal matching for" << input.filename;
    qDebug() << "  CRC32:" << input.crc32 << "MD5:" << input.md5 << "SHA1:" << input.sha1;
    qDebug() << "  Size:" << input.fileSize << "Serial:" << input.serial;
    
    // Pass 1: Hash-based matching (highest confidence)
    QList<ClrMameProEntry> hashCandidates;
    
    // Try CRC32
    if (!input.crc32.isEmpty()) {
        QString normalizedCrc = normalizeHash(input.crc32);
        if (m_crc32Index.contains(normalizedCrc)) {
            hashCandidates.append(m_crc32Index.value(normalizedCrc));
            qDebug() << "  Hash match (CRC32):" << m_crc32Index.value(normalizedCrc).gameName;
        }
    }
    
    // Try MD5
    if (!input.md5.isEmpty() && hashCandidates.isEmpty()) {
        QString normalizedMd5 = normalizeHash(input.md5);
        if (m_md5Index.contains(normalizedMd5)) {
            hashCandidates.append(m_md5Index.value(normalizedMd5));
            qDebug() << "  Hash match (MD5):" << m_md5Index.value(normalizedMd5).gameName;
        }
    }
    
    // Try SHA1
    if (!input.sha1.isEmpty() && hashCandidates.isEmpty()) {
        QString normalizedSha1 = normalizeHash(input.sha1);
        if (m_sha1Index.contains(normalizedSha1)) {
            hashCandidates.append(m_sha1Index.value(normalizedSha1));
            qDebug() << "  Hash match (SHA1):" << m_sha1Index.value(normalizedSha1).gameName;
        }
    }
    
    // If we have hash matches, score them with additional signals
    if (!hashCandidates.isEmpty()) {
        for (const ClrMameProEntry &entry : hashCandidates) {
            MultiSignalMatch match;
            match.entry = entry;
            match.hashMatch = true;
            match.confidenceScore = 100; // Hash match = 100 points
            match.matchSignalCount = 1;
            
            // Track which hash matched
            if (!input.crc32.isEmpty() && normalizeHash(input.crc32) == normalizeHash(entry.crc32)) {
                match.matchedHash = "CRC32:" + entry.crc32;
            } else if (!input.md5.isEmpty() && normalizeHash(input.md5) == normalizeHash(entry.md5)) {
                match.matchedHash = "MD5:" + entry.md5;
            } else if (!input.sha1.isEmpty() && normalizeHash(input.sha1) == normalizeHash(entry.sha1)) {
                match.matchedHash = "SHA1:" + entry.sha1;
            }
            
            // Check filename match (case-insensitive, ignore extension)
            QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
            QString entryBase = QFileInfo(entry.romName).completeBaseName().toLower();
            if (signalBase == entryBase) {
                match.filenameMatch = true;
                match.confidenceScore += 50;
                match.matchSignalCount++;
            }
            
            // Check size match (±1KB tolerance for header variations)
            qint64 sizeDiff = qAbs(input.fileSize - entry.size);
            if (sizeDiff <= 1024) {
                match.sizeMatch = true;
                match.confidenceScore += 30;
                match.matchSignalCount++;
            }
            
            // Check serial match
            if (!input.serial.isEmpty() && !entry.serial.isEmpty()) {
                if (input.serial.compare(entry.serial, Qt::CaseInsensitive) == 0) {
                    match.serialMatch = true;
                    match.confidenceScore += 20;
                    match.matchSignalCount++;
                }
            }
            
            matches.append(match);
        }
    }
    
    // Pass 2: Filename + size matching (no hash match)
    if (matches.isEmpty()) {
        qDebug() << "  No hash match, trying filename + size matching...";
        
        QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
        
        // Search through all entries in all indexes
        // Use a set to avoid duplicates since same entry might be in multiple hash indexes
        QSet<QString> seenEntries;
        
        // Search CRC32 index
        for (auto it = m_crc32Index.constBegin(); it != m_crc32Index.constEnd(); ++it) {
            const ClrMameProEntry &entry = it.value();
            
            // Skip if we've already checked this entry
            QString entryKey = entry.gameName + "|" + entry.romName;
            if (seenEntries.contains(entryKey)) {
                continue;
            }
            seenEntries.insert(entryKey);
            
            QString entryBase = QFileInfo(entry.romName).completeBaseName().toLower();
            
            // Check exact filename match
            bool filenameExact = (signalBase == entryBase);
            
            // Check size match
            qint64 sizeDiff = qAbs(input.fileSize - entry.size);
            bool sizeMatch = (sizeDiff <= 1024);
            
            // Only consider if both filename and size match
            if (filenameExact && sizeMatch) {
                MultiSignalMatch match;
                match.entry = entry;
                match.filenameMatch = true;
                match.sizeMatch = true;
                match.confidenceScore = 80; // 50 (filename) + 30 (size)
                match.matchSignalCount = 2;
                
                // Check serial if available
                if (!input.serial.isEmpty() && !entry.serial.isEmpty()) {
                    if (input.serial.compare(entry.serial, Qt::CaseInsensitive) == 0) {
                        match.serialMatch = true;
                        match.confidenceScore += 20;
                        match.matchSignalCount++;
                    }
                }
                
                matches.append(match);
                qDebug() << "  Filename+size match:" << entry.gameName << "score:" << match.confidenceScore;
                break; // Found a match, stop searching
            }
        }
    }
    
    // Sort by confidence score (highest first)
    std::sort(matches.begin(), matches.end(), [](const MultiSignalMatch &a, const MultiSignalMatch &b) {
        return a.confidenceScore > b.confidenceScore;
    });
    
    qDebug() << "LocalDatabaseProvider: Found" << matches.size() << "multi-signal matches";
    if (!matches.isEmpty()) {
        qDebug() << "  Best match:" << matches.first().entry.gameName 
                 << "confidence:" << matches.first().confidencePercent() << "%"
                 << "signals:" << matches.first().matchSignalCount;
    }
    
    return matches;
}

} // namespace Remus
