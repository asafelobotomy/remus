#ifndef REMUS_CLRMAMEPRO_PARSER_H
#define REMUS_CLRMAMEPRO_PARSER_H

#include <QString>
#include <QList>
#include <QMap>

namespace Remus {

/**
 * @brief ClrMamePro DAT entry (used by libretro-database)
 */
struct ClrMameProEntry {
    QString gameName;       // Game name
    QString description;    // Description
    QString region;         // Extracted from name (e.g., "USA", "Europe")
    QString romName;        // ROM filename
    qint64 size = 0;        // File size
    QString crc32;          // CRC32 hash
    QString md5;            // MD5 hash
    QString sha1;           // SHA1 hash
    QString serial;         // Serial number
};

/**
 * @brief Parser for ClrMamePro format DAT files
 * 
 * Used by libretro-database (GitHub: libretro/libretro-database)
 * Format:
 *   clrmamepro (
 *     name "System Name"
 *     description "..."
 *   )
 *   game (
 *     name "Game Name"
 *     description "..."
 *     rom ( name "file.bin" size 524288 crc F9394E97 md5 ... sha1 ... )
 *   )
 */
class ClrMameProParser
{
public:
    /**
     * @brief Parse a ClrMamePro DAT file
     * @param filePath Path to .dat file
     * @return List of game entries
     */
    static QList<ClrMameProEntry> parse(const QString &filePath);
    
    /**
     * @brief Parse header section
     * @param filePath Path to .dat file
     * @return Header metadata (name, description, version)
     */
    static QMap<QString, QString> parseHeader(const QString &filePath);

private:
    /**
     * @brief Parse game blocks from content
     * @param content DAT file content
     * @return List of entries
     */
    static QList<ClrMameProEntry> parseGameBlocks(const QString &content);
    
    /**
     * @brief Extract values from a parenthetical block
     * @param block Text between parentheses
     * @return Map of key->value pairs
     */
    static QMap<QString, QString> extractKeyValues(const QString &block);
    
    /**
     * @brief Parse inline attributes from a single line (for ROM blocks)
     * @param line Single-line attribute string (e.g., 'name "file.md" size 524288 crc ABC123')
     * @return Map of key->value pairs
     */
    static QMap<QString, QString> parseInlineAttributes(const QString &line);
    
    /**
     * @brief Extract quoted string value
     * @param text Input text with quotes
     * @return String without quotes
     */
    static QString extractQuoted(const QString &text);
};

} // namespace Remus

#endif // REMUS_CLRMAMEPRO_PARSER_H
