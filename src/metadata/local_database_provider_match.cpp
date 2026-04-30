#include "local_database_provider.h"

#include "../core/constants/confidence.h"
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace Remus {

// Normalise a disc serial for fuzzy comparison.
// Strips known manufacturer prefixes (MK-, HDR-, T-, SDC-, etc.)
// and region suffixes (-50, -53, etc.) to extract the core product number.
static QString normalizeSerial(const QString &serial)
{
    QString s = serial.trimmed().toUpper();
    // Strip known prefixes: "MK-", "HDR-", "SDC-", "T-" prefix
    static const QRegularExpression prefixRe(
        QStringLiteral("^(?:MK-|HDR-|SDC-|T-|HKT-)"),
        QRegularExpression::CaseInsensitiveOption);
    s.replace(prefixRe, QString());
    // Strip trailing region suffix: dash followed by 1-2 digits at end (e.g. -50, -53)
    static const QRegularExpression suffixRe(QStringLiteral("-\\d{1,2}$"));
    s.replace(suffixRe, QString());
    return s;
}

// Compare two serial numbers with normalisation.
// Returns true if either the raw serials match or the normalised cores match.
static bool serialsMatch(const QString &a, const QString &b)
{
    if (a.compare(b, Qt::CaseInsensitive) == 0)
        return true;
    const QString na = normalizeSerial(a);
    const QString nb = normalizeSerial(b);
    if (na.isEmpty() || nb.isEmpty())
        return false;
    return na == nb;
}



QList<MultiSignalMatch> LocalDatabaseProvider::matchROM(const ROMSignals &input) const
{
    QMutexLocker locker(&m_mutex);
    QList<MultiSignalMatch> matches;

    using namespace Constants::Confidence;

    QList<ClrMameProEntry> hashCandidates;
    QString matchedVia;

    if (!input.crc32.isEmpty()) {
        const QString normalizedCrc = normalizeHash(input.crc32);
        if (m_crc32Index.contains(normalizedCrc)) {
            hashCandidates.append(m_crc32Index.value(normalizedCrc));
            matchedVia = QStringLiteral("CRC32");
        }
    }

    if (!input.md5.isEmpty() && hashCandidates.isEmpty()) {
        const QString normalizedMd5 = normalizeHash(input.md5);
        if (m_md5Index.contains(normalizedMd5)) {
            hashCandidates.append(m_md5Index.value(normalizedMd5));
            matchedVia = QStringLiteral("MD5");
        }
    }

    if (!input.sha1.isEmpty() && hashCandidates.isEmpty()) {
        const QString normalizedSha1 = normalizeHash(input.sha1);
        if (m_sha1Index.contains(normalizedSha1)) {
            hashCandidates.append(m_sha1Index.value(normalizedSha1));
            matchedVia = QStringLiteral("SHA1");
        }
    }

    if (!hashCandidates.isEmpty()) {
        for (const ClrMameProEntry &entry : hashCandidates) {
            MultiSignalMatch match;
            match.entry = entry;
            match.hashMatch = true;
            match.confidenceScore = MultiSignal::HASH_BASE;
            match.matchSignalCount = 1;

            if (matchedVia == QLatin1String("CRC32")) {
                match.matchedHash = "CRC32:" + entry.crc32;
            } else if (matchedVia == QLatin1String("MD5")) {
                match.matchedHash = "MD5:" + entry.md5;
            } else if (matchedVia == QLatin1String("SHA1")) {
                match.matchedHash = "SHA1:" + entry.sha1;
            }

            const QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
            const QString entryBase = QFileInfo(entry.romName).completeBaseName().toLower();
            if (signalBase == entryBase) {
                match.filenameMatch = true;
                match.confidenceScore += MultiSignal::FILENAME_BONUS;
                match.matchSignalCount++;
            }

            const qint64 sizeDiff = qAbs(input.fileSize - entry.size);
            if (sizeDiff <= MultiSignal::SIZE_TOLERANCE) {
                match.sizeMatch = true;
                match.confidenceScore += MultiSignal::SIZE_BONUS;
                match.matchSignalCount++;
            }

            if (!input.serial.isEmpty() && !entry.serial.isEmpty()) {
                if (serialsMatch(input.serial, entry.serial)) {
                    match.serialMatch = true;
                    match.confidenceScore += MultiSignal::SERIAL_BONUS;
                    match.matchSignalCount++;
                }
            }

            matches.append(match);
        }
    }

    if (matches.isEmpty()) {
        const QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
        QSet<QString> seenEntries;

        auto tryFallbackScan = [&](auto &index) {
            if (!matches.isEmpty()) return;
            for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
                const ClrMameProEntry &entry = it.value();
                const QString entryKey = entry.gameName + "|" + entry.romName;
                if (seenEntries.contains(entryKey))
                    continue;
                seenEntries.insert(entryKey);

                const QString entryBase = QFileInfo(entry.romName).completeBaseName().toLower();
                const bool filenameExact = (signalBase == entryBase);
                const qint64 sizeDiff = qAbs(input.fileSize - entry.size);
                const bool sizeMatch = (sizeDiff <= MultiSignal::SIZE_TOLERANCE);

                if (filenameExact && sizeMatch) {
                    MultiSignalMatch match;
                    match.entry = entry;
                    match.filenameMatch = true;
                    match.sizeMatch = true;
                    match.confidenceScore = MultiSignal::FILENAME_SIZE_BASE;
                    match.matchSignalCount = 2;

                    if (!input.serial.isEmpty() && !entry.serial.isEmpty()) {
                        if (serialsMatch(input.serial, entry.serial)) {
                            match.serialMatch = true;
                            match.confidenceScore += MultiSignal::SERIAL_BONUS;
                            match.matchSignalCount++;
                        }
                    }

                    matches.append(match);
                    matchedVia = QStringLiteral("filename+size");
                    break;
                }
            }
        };

        tryFallbackScan(m_crc32Index);
        tryFallbackScan(m_md5Index);
        tryFallbackScan(m_sha1Index);
    }

    // Third fallback: serial-only matching (for disc images where hash and
    // filename may not match, but the disc serial was extracted from the header).
    // Search both the CRC32 index (for entries that have both hash and serial)
    // and the dedicated serial index (for entries like GameCube/Wii/Saturn
    // that have serial but no hash).
    if (matches.isEmpty() && !input.serial.isEmpty()) {
        const QString normalizedSerial = input.serial.toUpper().trimmed();
        QSet<QString> seenEntries;

        // Check serial index first (O(1) lookup for serial-only entries)
        auto serialRange = m_serialIndex.equal_range(normalizedSerial);
        for (auto it = serialRange.first; it != serialRange.second; ++it) {
            const ClrMameProEntry &entry = it.value();
            const QString entryKey = entry.gameName + "|" + entry.romName;
            if (seenEntries.contains(entryKey))
                continue;
            seenEntries.insert(entryKey);

            MultiSignalMatch match;
            match.entry = entry;
            match.serialMatch = true;
            match.confidenceScore = MultiSignal::SERIAL_BASE;
            match.matchSignalCount = 1;

            const qint64 sizeDiff = qAbs(input.fileSize - entry.size);
            if (sizeDiff <= MultiSignal::SIZE_TOLERANCE) {
                match.sizeMatch = true;
                match.confidenceScore += MultiSignal::SIZE_BONUS;
                match.matchSignalCount++;
            }

            matches.append(match);
            matchedVia = QStringLiteral("serial");
        }

        // Also scan CRC32 index for entries that have both hash and serial
        if (matches.isEmpty()) {
            for (auto it = m_crc32Index.constBegin(); it != m_crc32Index.constEnd(); ++it) {
                const ClrMameProEntry &entry = it.value();
                if (entry.serial.isEmpty())
                    continue;
                const QString entryKey = entry.gameName + "|" + entry.romName;
                if (seenEntries.contains(entryKey))
                    continue;
                seenEntries.insert(entryKey);

                if (serialsMatch(input.serial, entry.serial)) {
                    MultiSignalMatch match;
                    match.entry = entry;
                    match.serialMatch = true;
                    match.confidenceScore = MultiSignal::SERIAL_BASE;
                    match.matchSignalCount = 1;

                    const qint64 sizeDiff = qAbs(input.fileSize - entry.size);
                    if (sizeDiff <= MultiSignal::SIZE_TOLERANCE) {
                        match.sizeMatch = true;
                        match.confidenceScore += MultiSignal::SIZE_BONUS;
                        match.matchSignalCount++;
                    }

                    matches.append(match);
                    matchedVia = QStringLiteral("serial");
                }
            }
        }
    }

    std::sort(matches.begin(), matches.end(), [](const MultiSignalMatch &a, const MultiSignalMatch &b) {
        return a.confidenceScore > b.confidenceScore;
    });

    if (!matches.isEmpty()) {
        qDebug() << "LocalDB:" << input.filename << "→" << matches.first().entry.gameName
                 << "via" << matchedVia
                 << "(" << matches.first().confidencePercent() << "%)";
    }

    return matches;
}

} // namespace Remus