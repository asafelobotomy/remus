#ifndef REMUS_LIBRETRO_METADATA_PARSER_H
#define REMUS_LIBRETRO_METADATA_PARSER_H

#include <QHash>
#include <QString>

namespace Remus {

/**
 * @brief Enrichment metadata for a single ROM, keyed by CRC32
 *
 * Populated from libretro-database metadat/ DATs (genre, developer,
 * publisher, maxusers, releaseyear).
 */
struct LibretroMetadata {
    QString genre;
    QString developer;
    QString publisher;
    QString description;
    int maxUsers = 0;
    int releaseYear = 0;
};

/**
 * @brief Parser for libretro-database metadat/ DAT files
 *
 * Each metadat DAT uses ClrMamePro format with one metadata field per file:
 *   game (
 *       comment "Game Name"
 *       genre "Action"
 *       rom ( crc ABCD1234 )
 *   )
 *
 * The parser extracts CRC → value pairs and merges them into a single
 * LibretroMetadata struct per CRC.
 */
class LibretroMetadataParser {
public:
    /**
     * @brief Load all metadata DATs from a base directory
     * @param metadataDir Path containing genre/, developer/, publisher/, etc.
     * @return Number of CRC entries with at least one metadata field set
     */
    int loadAll(const QString &metadataDir);

    /**
     * @brief Load all DATs of a single type from a directory
     * @param dir Path to a metadat type directory (e.g. data/metadata/genre/)
     * @param type One of: genre, developer, publisher, maxusers, releaseyear
     * @return Number of entries parsed
     */
    int loadType(const QString &dir, const QString &type);

    /**
     * @brief Look up enrichment metadata by CRC32
     * @param crc32 Uppercase CRC32 hex string
     * @return Metadata (empty struct if not found)
     */
    LibretroMetadata lookup(const QString &crc32) const;

    /**
     * @brief Look up enrichment metadata by serial
     * @param serial Product serial (e.g., "SLUS-00707")
     * @return Metadata (empty struct if not found)
     */
    LibretroMetadata lookupBySerial(const QString &serial) const;

    /**
     * @brief Look up enrichment metadata by game name
     * @param name Exact game name (e.g., "Silent Hill (USA)")
     * @return Metadata (empty struct if not found)
     */
    LibretroMetadata lookupByName(const QString &name) const;

    /**
     * @brief Check whether any metadata exists for a CRC32
     */
    bool contains(const QString &crc32) const;

    /**
     * @brief Total number of unique CRCs with metadata
     */
    int size() const;

    /**
     * @brief Clear all loaded metadata
     */
    void clear();

private:
    /**
     * @brief Parse a single metadat DAT file
     * @param filePath Path to .dat file
     * @param type Metadata type to extract
     * @return Number of entries parsed
     */
    int parseFile(const QString &filePath, const QString &type);

    // CRC32 (uppercase) → merged metadata
    QHash<QString, LibretroMetadata> m_index;

    // Serial (uppercase, trimmed) → merged metadata (for disc systems)
    QHash<QString, LibretroMetadata> m_serialIndex;

    // Game name → merged metadata (fallback for disc systems)
    QHash<QString, LibretroMetadata> m_nameIndex;
};

} // namespace Remus

#endif // REMUS_LIBRETRO_METADATA_PARSER_H
