#ifndef REMUS_METADATA_CACHE_H
#define REMUS_METADATA_CACHE_H

#include <QObject>
#include <QSqlDatabase>
#include "metadata_provider.h"

namespace Remus {

/**
 * @brief Local cache for metadata and artwork
 * 
 * Stores fetched metadata in SQLite to avoid redundant API calls.
 */
class MetadataCache : public QObject {
    Q_OBJECT

public:
    explicit MetadataCache(QSqlDatabase &db, QObject *parent = nullptr);

    /**
     * @brief Get cached metadata by hash
     * @param hash File hash (CRC32/MD5/SHA1)
     * @param system System name
     * @return Cached metadata (empty if not found)
     */
    GameMetadata getByHash(const QString &hash, const QString &system);

    /**
     * @brief Get cached metadata by provider ID
     * @param providerId Provider name
     * @param gameId Game ID from provider
     * @return Cached metadata (empty if not found)
     */
    GameMetadata getByProviderId(const QString &providerId, const QString &gameId);

    /**
     * @brief Store metadata in cache
     * @param metadata Metadata to cache
     * @param hash Optional file hash
     * @param system Optional system name
     * @return True if successful
     */
    bool store(const GameMetadata &metadata, 
               const QString &hash = QString(),
               const QString &system = QString());

    /**
     * @brief Store artwork URLs in cache
     * @param gameId Provider game ID
     * @param artwork Artwork URLs
     * @return True if successful
     */
    bool storeArtwork(const QString &gameId, const ArtworkUrls &artwork);

    /**
     * @brief Get cached artwork URLs
     */
    ArtworkUrls getArtwork(const QString &gameId);

    /**
     * @brief Clear cache older than specified days
     * @param days Age threshold
     * @return Number of entries deleted
     */
    int clearOldCache(int days = 30);

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        int totalEntries = 0;
        int entriesThisWeek = 0;
        qint64 totalSizeBytes = 0;
    };
    CacheStats getStats();

private:
    QSqlDatabase &m_db;
};

} // namespace Remus

#endif // REMUS_METADATA_CACHE_H
