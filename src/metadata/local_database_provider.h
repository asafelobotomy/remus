#ifndef LOCAL_DATABASE_PROVIDER_H
#define LOCAL_DATABASE_PROVIDER_H

#include "metadata_provider.h"
#include "clrmamepro_parser.h"
#include <QMap>
#include <QHash>
#include <QMutex>
#include <QDateTime>

namespace Remus {

/**
 * @brief Metadata about a loaded DAT file
 */
struct DatMetadata {
    QString name;               // DAT name (e.g., "Sega - Mega Drive - Genesis")
    QString version;            // Version string (e.g., "2026.01.17")
    QString description;        // DAT description
    QString filePath;           // Path to .dat file
    QDateTime loadedAt;         // When was it loaded
    int entryCount = 0;         // Number of entries
};

/**
 * @brief Multi-signal match result with confidence scoring
 */
struct MultiSignalMatch {
    ClrMameProEntry entry;      // Matched DAT entry
    int confidenceScore = 0;    // Combined confidence (0-200 scale)
    
    // Signal match indicators
    bool hashMatch = false;     // Hash matched (100 points)
    bool filenameMatch = false; // Filename matched (50 points)
    bool sizeMatch = false;     // File size matched (30 points)
    bool serialMatch = false;   // Serial matched (20 points)
    
    QString matchedHash;        // Which hash matched
    int matchSignalCount = 0;   // Number of signals that matched
    
    // Confidence percentage (0-100%)
    int confidencePercent() const {
        // Normalize to 0-100 scale
        // Perfect match (all signals) = 200 points = 100%
        return qMin(100, (confidenceScore * 100) / 200);
    }
};

/**
 * @brief Input signals for multi-signal matching
 */
struct ROMSignals {
    QString crc32;              // CRC32 hash (optional)
    QString md5;                // MD5 hash (optional)
    QString sha1;               // SHA1 hash (optional)
    QString filename;           // ROM filename (required)
    qint64 fileSize = 0;        // File size in bytes (required)
    QString serial;             // Serial number (optional)
};


/**
 * @brief Offline ROM metadata provider using local DAT files
 * 
 * Uses No-Intro/Redump DAT files from libretro-database for hash-based
 * ROM identification without requiring API keys or internet connection.
 * 
 * Priority: 100 (highest - checked first, before online APIs)
 * 
 * DAT files location: data/databases/*.dat
 * 
 * Matching methods:
 * - CRC32 hash (cartridge-based systems)
 * - MD5/SHA1 hash (disc-based systems)
 * - File size + filename (fallback)
 */
class LocalDatabaseProvider : public MetadataProvider
{
    Q_OBJECT
    
public:
    explicit LocalDatabaseProvider(QObject *parent = nullptr);
    ~LocalDatabaseProvider() override;
    
    /**
     * @brief Load DAT files from directory
     * @param directory Path to directory containing .dat files
     * @return Number of entries loaded
     */
    int loadDatabases(const QString &directory);
    
    /**
     * @brief Load a single DAT file
     * @param filePath Path to .dat file
     * @return Number of entries loaded from this file
     */
    int loadDatabase(const QString &filePath);
    
    /**
     * @brief Get database statistics
     * @return Map of system name -> entry count
     */
    QMap<QString, int> getDatabaseStats() const;
    
    /**
     * @brief Get metadata for all loaded DAT files
     * @return List of DAT metadata
     */
    QList<DatMetadata> getLoadedDats() const;
    
    /**
     * @brief Check if a DAT file needs updating
     * @param filePath Path to potential newer .dat file
     * @return True if file is newer version, false otherwise
     */
    bool isDatNewer(const QString &filePath) const;
    
    /**
     * @brief Reload a DAT file with newer version
     * @param filePath Path to .dat file
     * @return Number of entries loaded, or -1 on error
     */
    int reloadDatabase(const QString &filePath);
    
    /**
     * @brief Multi-signal ROM matching with confidence scoring
     * 
     * Combines multiple signals (hash, filename, size, serial) to identify ROMs
     * and returns matches sorted by confidence score.
     * 
     * @param input ROM signals (hash, filename, size, serial)
     * @return List of matches sorted by confidence (highest first)
     */
    QList<MultiSignalMatch> matchROM(const ROMSignals &input) const;
    
    // MetadataProvider interface
    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system,
                                     const QString &region = QString()) override;
    
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;
    
    // MetadataProvider required methods
    QString name() const override { return "LocalDatabase"; }
    bool requiresAuth() const override { return false; }
    
    QString providerName() const { return "LocalDatabase"; }
    int priority() const { return 110; } // Highest priority
    
signals:
    void databaseLoaded(const QString &systemName, int entryCount);
    void loadingProgress(int current, int total);
    void updateAvailable(const QString &systemName, const QString &currentVersion, const QString &newVersion);
    
private:
    /**
     * @brief Index DAT entries for fast lookup
     * @param entries List of DAT entries
     * @param systemName System name for this database
     */
    void indexEntries(const QList<ClrMameProEntry> &entries, const QString &systemName);
    
    /**
     * @brief Convert ClrMameProEntry to GameMetadata
     * @param entry DAT entry
     * @return Game metadata
     */
    GameMetadata datEntryToMetadata(const ClrMameProEntry &entry) const;
    
    /**
     * @brief Normalize hash for lookup (uppercase, no spaces)
     * @param hash Input hash
     * @return Normalized hash
     */
    QString normalizeHash(const QString &hash) const;
    
    // Hash indexes for O(1) lookup
    QHash<QString, ClrMameProEntry> m_crc32Index;   // CRC32 -> ClrMameProEntry
    QHash<QString, ClrMameProEntry> m_md5Index;     // MD5 -> ClrMameProEntry
    QHash<QString, ClrMameProEntry> m_sha1Index;    // SHA1 -> ClrMameProEntry
    
    // System statistics
    QMap<QString, int> m_systemStats;
    
    // DAT metadata tracking
    QMap<QString, DatMetadata> m_datMetadata;  // System name -> DAT metadata
    
    // Thread safety
    mutable QMutex m_mutex;
    
    int m_totalEntries = 0;
};

} // namespace Remus

#endif // LOCAL_DATABASE_PROVIDER_H
