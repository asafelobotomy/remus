#include "local_database_provider.h"
#include "../core/constants/match_methods.h"

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


QString LocalDatabaseProvider::identifierForEntry(const ClrMameProEntry &entry) const
{
    if (!entry.crc32.isEmpty()) {
        return entry.crc32;
    }
    if (!entry.md5.isEmpty()) {
        return entry.md5;
    }
    if (!entry.sha1.isEmpty()) {
        return entry.sha1;
    }
    return entry.serial.toUpper().trimmed();
}

QString LocalDatabaseProvider::systemForEntry(const ClrMameProEntry &entry) const
{
    if (!entry.crc32.isEmpty()) {
        const QString normalized = normalizeHash(entry.crc32);
        if (m_hashToSystem.contains(normalized)) {
            return m_hashToSystem.value(normalized);
        }
    }

    if (!entry.md5.isEmpty()) {
        const QString normalized = normalizeHash(entry.md5);
        if (m_hashToSystem.contains(normalized)) {
            return m_hashToSystem.value(normalized);
        }
    }

    if (!entry.sha1.isEmpty()) {
        const QString normalized = normalizeHash(entry.sha1);
        if (m_hashToSystem.contains(normalized)) {
            return m_hashToSystem.value(normalized);
        }
    }

    if (!entry.serial.isEmpty()) {
        const QString normalized = entry.serial.toUpper().trimmed();
        if (m_serialToSystem.contains(normalized)) {
            return m_serialToSystem.value(normalized);
        }
    }

    return QString();
}

bool LocalDatabaseProvider::findEntryById(const QString &id,
                                          ClrMameProEntry *entry,
                                          QString *systemName) const
{
    const QString normalizedHash = normalizeHash(id);

    if (m_crc32Index.contains(normalizedHash)) {
        if (entry) {
            *entry = m_crc32Index.value(normalizedHash);
        }
        if (systemName) {
            *systemName = m_hashToSystem.value(normalizedHash);
        }
        return true;
    }

    if (m_md5Index.contains(normalizedHash)) {
        if (entry) {
            *entry = m_md5Index.value(normalizedHash);
        }
        if (systemName) {
            *systemName = m_hashToSystem.value(normalizedHash);
        }
        return true;
    }

    if (m_sha1Index.contains(normalizedHash)) {
        if (entry) {
            *entry = m_sha1Index.value(normalizedHash);
        }
        if (systemName) {
            *systemName = m_hashToSystem.value(normalizedHash);
        }
        return true;
    }

    const QString normalizedSerial = id.toUpper().trimmed();
    if (m_serialIndex.contains(normalizedSerial)) {
        if (entry) {
            *entry = m_serialIndex.value(normalizedSerial);
        }
        if (systemName) {
            *systemName = m_serialToSystem.value(normalizedSerial);
        }
        return true;
    }

    return false;
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
            m_hashToSystem[normalized] = systemName;
            md5Count++;
        }
        
        // Index by SHA1 (discs)
        if (!entry.sha1.isEmpty()) {
            QString normalized = normalizeHash(entry.sha1);
            m_sha1Index[normalized] = entry;
            m_hashToSystem[normalized] = systemName;
            sha1Count++;
        }

        // Index by serial (for entries with serial, especially those
        // without any hash like GameCube/Wii/Saturn DAT entries)
        if (!entry.serial.isEmpty()) {
            const QString normalizedSerial = entry.serial.toUpper().trimmed();
            m_serialIndex.insert(normalizedSerial, entry);
            m_serialToSystem[normalizedSerial] = systemName;
        }

        // Index by game name (for name-based search)
        if (!entry.gameName.isEmpty()) {
            m_nameIndex[entry.gameName.toLower()].append(entry);
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
        }
    }

    const QString serial = metadata.externalIds.value(QStringLiteral("serial")).toUpper().trimmed();
    if (!serial.isEmpty()) {
        apply(m_metadataParser.lookupBySerial(serial));
    }

    // Try name-based lookup (disc systems where CRC doesn't match libretro data)
    if (!metadata.title.isEmpty()) {
        apply(m_metadataParser.lookupByName(metadata.title));
    }
}

GameMetadata LocalDatabaseProvider::datEntryToMetadata(const ClrMameProEntry &entry) const
{
    GameMetadata metadata;
    metadata.id = identifierForEntry(entry);
    metadata.providerId = QStringLiteral("localdatabase");
    
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
    
    // Use DAT description only if present; leave empty so remote providers can supply
    // a real synopsis. ClrMamePro/No-Intro DATs rarely carry meaningful descriptions.
    if (!entry.description.isEmpty()) {
        metadata.description = entry.description;
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
    metadata.system = systemForEntry(entry);
    
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
    enrichFromLibretro(metadata, entry.crc32);

    // Construct libretro-thumbnails artwork URLs with fallback candidates
    const QString systemName = metadata.system.isEmpty() ? systemForEntry(entry) : metadata.system;
    if (!systemName.isEmpty()) {
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

    return metadata;
}

QString LocalDatabaseProvider::normalizeHash(const QString &hash) const
{
    // Remove spaces, convert to uppercase
    return hash.trimmed().toUpper().remove(' ');
}

} // namespace Remus
