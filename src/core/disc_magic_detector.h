#ifndef REMUS_DISC_MAGIC_DETECTOR_H
#define REMUS_DISC_MAGIC_DETECTOR_H

#include <QString>
#include <QList>

namespace Remus {

/**
 * @brief Disc image header info extracted from IP.BIN or ISO 9660 PVD
 */
struct DiscHeaderInfo {
    int systemId = 0;           ///< Detected system ID (from Constants::Systems)
    QString systemName;         ///< Internal system name (e.g., "GameCube")
    QString serial;             ///< Product serial from disc header (e.g., "HDR-0176")
    QString title;              ///< Game title from disc header
    QString releaseDate;        ///< Release date from disc header
    bool detected = false;      ///< True if magic bytes matched a known system
};

/**
 * @brief Detects gaming system from disc image magic bytes
 *
 * Uses the RetroArch MAGIC_NUMBERS[] approach: read magic byte sequences
 * at known offsets to disambiguate .iso/.bin/.cue/.cdi files.
 *
 * Sources:
 * - RetroArch task_database_cue.c MAGIC_NUMBERS[] (MIT/GPL-3.0)
 * - Dolphin DiscUtils.h GAMECUBE_DISC_MAGIC / WII_DISC_MAGIC
 * - PPSSPP Core/Loaders.cpp PVD system identifier check
 * - dreamcast.wiki IP.BIN header field table
 */
class DiscMagicDetector
{
public:
    /**
     * @brief Detect system from a disc image file
     * @param filePath Path to disc image (.iso, .bin, .cdi, .img, etc.)
     * @return Detection result with system ID and optional serial/title
     */
    static DiscHeaderInfo detect(const QString &filePath);

    /**
     * @brief Detect system from raw data buffer
     * @param data At least 64KB of file data from offset 0
     * @param fileSize Total file size (for PS1/PS2 disambiguation)
     * @return Detection result
     */
    static DiscHeaderInfo detectFromData(const QByteArray &data, qint64 fileSize);

    /**
     * @brief Extract Dreamcast serial/title from CDI or raw disc image
     *
     * Scans for "SEGA SEGAKATANA" magic, then reads IP.BIN fields:
     * - Product number at offset +0x40 (10 bytes)
     * - Game title at offset +0x80 (128 bytes)
     *
     * @param filePath Path to CDI/GDI/BIN file
     * @return DiscHeaderInfo with serial and title populated
     */
    static DiscHeaderInfo extractDreamcastHeader(const QString &filePath);

    /**
     * @brief Check if extension is a disc image format worth probing
     * @param extension File extension (with dot, e.g., ".iso")
     * @return True if magic-byte detection should be attempted
     */
    static bool isDiscImageExtension(const QString &extension);

private:
    struct MagicEntry {
        int systemId;
        const char *systemName;
        const char *magic;
        int magicLen;
        qint64 offset;
    };

    static const QList<MagicEntry> &magicTable();
    static DiscHeaderInfo scanForDreamcastHeader(const QByteArray &data);
};

} // namespace Remus

#endif // REMUS_DISC_MAGIC_DETECTOR_H
