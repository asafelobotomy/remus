#include "local_database_provider.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>

namespace Remus {

int LocalDatabaseProvider::loadDatabases(const QString &directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        qWarning() << "LocalDatabaseProvider: Directory not found:" << directory;
        return 0;
    }

    const QFileInfoList datFiles = dir.entryInfoList({QStringLiteral("*.dat")}, QDir::Files);
    qDebug() << "LocalDatabaseProvider: Found" << datFiles.size() << "DAT files in" << directory;

    int totalLoaded = 0;
    int current = 0;
    const int total = datFiles.size();
    for (const QFileInfo &fileInfo : datFiles) {
        current++;
        emit loadingProgress(current, total);
        totalLoaded += loadDatabase(fileInfo.absoluteFilePath());
    }

    qDebug() << "LocalDatabaseProvider: Loaded" << totalLoaded << "total entries from" << datFiles.size() << "databases";
    return totalLoaded;
}

int LocalDatabaseProvider::loadMetadata(const QString &metadataDir)
{
    const int loaded = m_metadataParser.loadAll(metadataDir);
    qDebug() << "LocalDatabaseProvider: Enrichment metadata loaded for" << loaded << "unique CRCs";
    return loaded;
}

int LocalDatabaseProvider::loadDatabase(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString systemName = fileInfo.baseName();
    qDebug() << "LocalDatabaseProvider: Loading" << systemName << "from" << filePath;

    const QMap<QString, QString> header = ClrMameProParser::parseHeader(filePath);
    const QList<ClrMameProEntry> entries = ClrMameProParser::parse(filePath);
    if (entries.isEmpty()) {
        qWarning() << "LocalDatabaseProvider: No entries parsed from" << filePath;
        return 0;
    }

    indexEntries(entries, systemName);
    m_systemStats[systemName] = entries.size();
    m_totalEntries += entries.size();

    DatMetadata metadata;
    metadata.name = header.value(QStringLiteral("name"), systemName);
    metadata.version = header.value(QStringLiteral("version"), QStringLiteral("unknown"));
    metadata.description = header.value(QStringLiteral("description"), QString());
    metadata.filePath = filePath;
    metadata.loadedAt = QDateTime::currentDateTime();
    metadata.entryCount = entries.size();

    QMutexLocker locker(&m_mutex);
    m_datMetadata[systemName] = metadata;
    locker.unlock();

    emit databaseLoaded(systemName, entries.size());
    qDebug() << "LocalDatabaseProvider: Indexed" << entries.size() << "entries for" << systemName << "(Version:" << metadata.version << ")";
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
    const QString searchLower = title.toLower();

    QSet<QString> seenEntries;
    auto maybeAppend = [&](const ClrMameProEntry &entry) {
        if (results.size() >= 10) {
            return;
        }

        const QString entryKey = entry.gameName + QLatin1Char('|') + entry.romName + QLatin1Char('|') + entry.serial;
        if (seenEntries.contains(entryKey) || !entry.gameName.toLower().contains(searchLower)) {
            return;
        }
        seenEntries.insert(entryKey);

        const QString entrySystem = systemForEntry(entry);
        if (!system.isEmpty() && !entrySystem.isEmpty()
            && !entrySystem.contains(system, Qt::CaseInsensitive)
            && !system.contains(entrySystem, Qt::CaseInsensitive)) {
            return;
        }

        if (!region.isEmpty()
            && !entry.region.contains(region, Qt::CaseInsensitive)
            && !entry.gameName.contains(region, Qt::CaseInsensitive)) {
            return;
        }

        SearchResult result;
        result.id = identifierForEntry(entry);
        if (result.id.isEmpty()) {
            return;
        }

        result.title = entry.gameName;
        result.system = entrySystem.isEmpty() ? system : entrySystem;
        result.region = entry.region;
        result.releaseYear = entry.releaseYear;
        if (entry.gameName.toLower() == searchLower) {
            result.matchScore = 1.0f;
        } else if (entry.gameName.toLower().startsWith(searchLower)) {
            result.matchScore = 0.9f;
        } else {
            result.matchScore = 0.7f;
        }

        results.append(result);
    };

    for (auto it = m_nameIndex.constBegin(); it != m_nameIndex.constEnd(); ++it) {
        if (!it.key().contains(searchLower))
            continue;
        for (const ClrMameProEntry &entry : it.value()) {
            maybeAppend(entry);
            if (results.size() >= 10)
                goto done;
        }
    }
done:;

    std::sort(results.begin(), results.end(), [](const SearchResult &left, const SearchResult &right) {
        return left.matchScore > right.matchScore;
    });

    qDebug() << "LocalDatabaseProvider: Name search for" << title << "found" << results.size() << "results";
    return results;
}

GameMetadata LocalDatabaseProvider::getByHash(const QString &hash, const QString &system)
{
    Q_UNUSED(system);

    QMutexLocker locker(&m_mutex);
    const QString normalizedHash = normalizeHash(hash);
    ClrMameProEntry entry;
    bool found = false;

    if (normalizedHash.length() == 8 && m_crc32Index.contains(normalizedHash)) {
        entry = m_crc32Index.value(normalizedHash);
        found = true;
        qDebug() << "LocalDatabaseProvider: CRC32 match found:" << entry.gameName;
    } else if (normalizedHash.length() == 32 && m_md5Index.contains(normalizedHash)) {
        entry = m_md5Index.value(normalizedHash);
        found = true;
        qDebug() << "LocalDatabaseProvider: MD5 match found:" << entry.gameName;
    } else if (normalizedHash.length() == 40 && m_sha1Index.contains(normalizedHash)) {
        entry = m_sha1Index.value(normalizedHash);
        found = true;
        qDebug() << "LocalDatabaseProvider: SHA1 match found:" << entry.gameName;
    }

    if (found) {
        return datEntryToMetadata(entry);
    }

    qDebug() << "LocalDatabaseProvider: No hash match for" << normalizedHash.left(8) << "...";
    return {};
}

GameMetadata LocalDatabaseProvider::getById(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    ClrMameProEntry entry;
    if (!findEntryById(id, &entry)) {
        return {};
    }
    return datEntryToMetadata(entry);
}

ArtworkUrls LocalDatabaseProvider::getArtwork(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    ClrMameProEntry entry;
    QString systemName;
    if (!findEntryById(id, &entry, &systemName) || systemName.isEmpty()) {
        return {};
    }

    ArtworkUrls artwork;
    const QStringList boxCandidates = generateThumbnailCandidates(systemName, entry.gameName, QStringLiteral("Named_Boxarts"));
    const QStringList snapCandidates = generateThumbnailCandidates(systemName, entry.gameName, QStringLiteral("Named_Snaps"));
    const QStringList titleCandidates = generateThumbnailCandidates(systemName, entry.gameName, QStringLiteral("Named_Titles"));
    if (!boxCandidates.isEmpty()) artwork.boxFront = QUrl(boxCandidates.first());
    if (!snapCandidates.isEmpty()) artwork.screenshot = QUrl(snapCandidates.first());
    if (!titleCandidates.isEmpty()) artwork.titleScreen = QUrl(titleCandidates.first());
    return artwork;
}

QList<DatMetadata> LocalDatabaseProvider::getLoadedDats() const
{
    QMutexLocker locker(&m_mutex);
    return m_datMetadata.values();
}

bool LocalDatabaseProvider::isDatNewer(const QString &filePath) const
{
    const QFileInfo fileInfo(filePath);
    const QString systemName = fileInfo.baseName();
    const QMap<QString, QString> header = ClrMameProParser::parseHeader(filePath);
    const QString newVersion = header.value(QStringLiteral("version"), QString());
    if (newVersion.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    if (!m_datMetadata.contains(systemName)) {
        return true;
    }

    const QString currentVersion = m_datMetadata[systemName].version;
    const bool newer = newVersion > currentVersion;
    if (newer) {
        qInfo() << "LocalDatabaseProvider: Update available for" << systemName << "- current:" << currentVersion << "new:" << newVersion;
    }
    return newer;
}

int LocalDatabaseProvider::reloadDatabase(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString systemName = fileInfo.baseName();
    qInfo() << "LocalDatabaseProvider: Reloading" << systemName << "from" << filePath;

    if (!isDatNewer(filePath)) {
        qWarning() << "LocalDatabaseProvider: File is not newer, skipping reload";
        return -1;
    }

    QMutexLocker locker(&m_mutex);
    if (m_datMetadata.contains(systemName)) {
        const int oldCount = m_datMetadata[systemName].entryCount;
        m_totalEntries -= oldCount;
        qWarning() << "LocalDatabaseProvider: Clearing" << oldCount << "old entries";
    }
    locker.unlock();

    return loadDatabase(filePath);
}

} // namespace Remus