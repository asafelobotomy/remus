#ifndef REMUS_DAT_PARSER_H
#define REMUS_DAT_PARSER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QXmlStreamReader>

namespace Remus {

/**
 * @brief DAT file header information
 */
struct DatHeader {
    QString name;           // e.g., "Nintendo - Nintendo Entertainment System (Headerless)"
    QString description;    // e.g., "No-Intro | 2024-01-15"
    QString version;        // e.g., "20240115"
    QString author;
    QString category;
    QString url;
    QDateTime date;
};

/**
 * @brief Individual ROM entry in a DAT file
 */
struct DatRomEntry {
    QString gameName;       // Parent game name
    QString description;    // Game description
    QString romName;        // ROM filename
    qint64 size = 0;        // File size in bytes
    QString crc32;          // CRC32 hash (lowercase hex)
    QString md5;            // MD5 hash (lowercase hex)
    QString sha1;           // SHA1 hash (lowercase hex)
    QString status;         // "verified", "good", "bad", etc.
    QString serial;         // Game serial number (if available)
};

/**
 * @brief DAT file parse result
 */
struct DatParseResult {
    bool success = false;
    QString error;
    DatHeader header;
    QList<DatRomEntry> entries;
    int entryCount = 0;
};

/**
 * @brief Parses No-Intro/Redump XML DAT files (Logiqx format)
 * 
 * Supports:
 * - Standard Logiqx DTD format used by No-Intro and Redump
 * - Multi-ROM games (each ROM file as separate entry)
 * - Hash extraction (CRC32, MD5, SHA1)
 */
class DatParser : public QObject {
    Q_OBJECT

public:
    explicit DatParser(QObject *parent = nullptr);

    /**
     * @brief Parse a DAT file
     * @param filePath Path to .dat or .xml file
     * @return Parse result with header and entries
     */
    DatParseResult parse(const QString &filePath);

    /**
     * @brief Parse DAT content from string
     * @param content XML content as string
     * @return Parse result with header and entries
     */
    DatParseResult parseContent(const QString &content);

    /**
     * @brief Get entries by hash (for verification lookup)
     * @param entries List of DAT entries
     * @param hashType "crc32", "md5", or "sha1"
     * @return Map of hash value to entry
     */
    static QMap<QString, DatRomEntry> indexByHash(
        const QList<DatRomEntry> &entries, 
        const QString &hashType);

    /**
     * @brief Detect DAT file source
     * @param header DAT header
     * @return "no-intro", "redump", "tosec", or "unknown"
     */
    static QString detectSource(const DatHeader &header);

signals:
    void parseProgress(int entriesParsed, int totalEstimate);
    void parseError(const QString &error);

private:
    bool parseHeader(QXmlStreamReader &xml, DatHeader &header);
    bool parseGame(QXmlStreamReader &xml, QList<DatRomEntry> &entries);
    QString normalizeHash(const QString &hash);
};

} // namespace Remus

#endif // REMUS_DAT_PARSER_H
