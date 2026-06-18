#pragma once

#include <QtGlobal>
#include <QString>

namespace Remus {

/**
 * @brief Input signals for multi-signal ROM matching.
 */
struct ROMSignals {
    QString crc32;
    QString md5;
    QString sha1;
    /// CHD header or RVZ/GCZ content SHA1 for compressed-disc compendium lookup.
    QString contentSha1;
    QString filename;
    qint64 fileSize = 0;
    QString serial;
    int discNumber = 0; ///< Parsed or caller-supplied disc index for corroboration
};

/**
 * @brief Multi-signal match result from the compendium offline matcher.
 */
struct CompendiumMultiSignalMatch {
    QString gameId;
    QString sourceEntryKey;
    QString romName;
    qint64 romSize = 0;
    QString serial;
    int confidenceScore = 0;

    bool hashMatch = false;
    bool filenameMatch = false;
    bool sizeMatch = false;
    bool serialMatch = false;

    QString matchedHash;
    int matchSignalCount = 0;

    QString setKey;
    int catalogDiscNumber = 0;
    int catalogDiscCount = 0;
    bool discNumberMatch = false;

    int confidencePercent() const {
        return qMin(100, (confidenceScore * 100) / 200);
    }
};

} // namespace Remus
