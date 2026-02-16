#ifndef REMUS_HEADER_DETECTOR_H
#define REMUS_HEADER_DETECTOR_H

#include <QObject>
#include <QString>
#include <QMap>

namespace Remus {

/**
 * @brief ROM header detection result
 */
struct HeaderInfo {
    bool hasHeader = false;
    int headerSize = 0;
    QString headerType;         // "iNES", "iNES2", "Lynx", "SMC", etc.
    QString systemHint;         // Detected system based on header
    QByteArray headerData;      // Raw header bytes (for analysis)
    bool valid = false;         // Whether header is valid/well-formed
    QString info;               // Additional info about header contents
};

/**
 * @brief Detects and strips ROM headers for verification
 * 
 * Many ROMs have copier headers that must be removed before hashing
 * to match No-Intro/Redump databases (which use headerless dumps).
 * 
 * Supported headers:
 * - iNES/NES2.0 (NES): 16 bytes
 * - Lynx (Atari Lynx): 64 bytes  
 * - SMC/SWC (SNES copiers): 512 bytes
 * - FDS (Famicom Disk System): 16 bytes
 * - A78 (Atari 7800): 128 bytes
 */
class HeaderDetector : public QObject {
    Q_OBJECT

public:
    explicit HeaderDetector(QObject *parent = nullptr);

    /**
     * @brief Detect header in a ROM file
     * @param filePath Path to ROM file
     * @return Header detection result
     */
    HeaderInfo detect(const QString &filePath);

    /**
     * @brief Detect header from raw data
     * @param data File data
     * @param extension File extension (for format hints)
     * @return Header detection result
     */
    HeaderInfo detectFromData(const QByteArray &data, const QString &extension);

    /**
     * @brief Strip header from file and save to new location
     * @param inputPath Source ROM with header
     * @param outputPath Destination for headerless ROM
     * @return True if successful
     */
    bool stripHeader(const QString &inputPath, const QString &outputPath);

    /**
     * @brief Get file data with header stripped (in memory)
     * @param filePath Path to ROM file
     * @return File data without header
     */
    QByteArray getHeaderlessData(const QString &filePath);

    /**
     * @brief Check if extension typically has headers
     * @param extension File extension (with dot)
     * @return True if format may have headers
     */
    static bool mayHaveHeader(const QString &extension);

    /**
     * @brief Get expected header size for extension
     * @param extension File extension  
     * @return Expected header size, or 0 if no header expected
     */
    static int getExpectedHeaderSize(const QString &extension);

private:
    HeaderInfo detectNES(const QByteArray &data);
    HeaderInfo detectLynx(const QByteArray &data);
    HeaderInfo detectSNES(const QByteArray &data, qint64 fileSize);
    HeaderInfo detectFDS(const QByteArray &data);
    HeaderInfo detectA78(const QByteArray &data);
    
    // iNES header parsing helpers
    QString parseMapperInfo(const QByteArray &header);
    bool isNES20Format(const QByteArray &header);
};

} // namespace Remus

#endif // REMUS_HEADER_DETECTOR_H
