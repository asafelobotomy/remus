#include "local_database_provider.h"

#include <QDebug>
#include <QFileInfo>
#include <QSet>

namespace Remus {

QList<MultiSignalMatch> LocalDatabaseProvider::matchROM(const ROMSignals &input) const
{
    QMutexLocker locker(&m_mutex);
    QList<MultiSignalMatch> matches;

    qDebug() << "LocalDatabaseProvider: Multi-signal matching for" << input.filename;
    qDebug() << "  CRC32:" << input.crc32 << "MD5:" << input.md5 << "SHA1:" << input.sha1;
    qDebug() << "  Size:" << input.fileSize << "Serial:" << input.serial;

    QList<ClrMameProEntry> hashCandidates;

    if (!input.crc32.isEmpty()) {
        const QString normalizedCrc = normalizeHash(input.crc32);
        if (m_crc32Index.contains(normalizedCrc)) {
            hashCandidates.append(m_crc32Index.value(normalizedCrc));
            qDebug() << "  Hash match (CRC32):" << m_crc32Index.value(normalizedCrc).gameName;
        }
    }

    if (!input.md5.isEmpty() && hashCandidates.isEmpty()) {
        const QString normalizedMd5 = normalizeHash(input.md5);
        if (m_md5Index.contains(normalizedMd5)) {
            hashCandidates.append(m_md5Index.value(normalizedMd5));
            qDebug() << "  Hash match (MD5):" << m_md5Index.value(normalizedMd5).gameName;
        }
    }

    if (!input.sha1.isEmpty() && hashCandidates.isEmpty()) {
        const QString normalizedSha1 = normalizeHash(input.sha1);
        if (m_sha1Index.contains(normalizedSha1)) {
            hashCandidates.append(m_sha1Index.value(normalizedSha1));
            qDebug() << "  Hash match (SHA1):" << m_sha1Index.value(normalizedSha1).gameName;
        }
    }

    if (!hashCandidates.isEmpty()) {
        for (const ClrMameProEntry &entry : hashCandidates) {
            MultiSignalMatch match;
            match.entry = entry;
            match.hashMatch = true;
            match.confidenceScore = 100;
            match.matchSignalCount = 1;

            if (!input.crc32.isEmpty() && normalizeHash(input.crc32) == normalizeHash(entry.crc32)) {
                match.matchedHash = "CRC32:" + entry.crc32;
            } else if (!input.md5.isEmpty() && normalizeHash(input.md5) == normalizeHash(entry.md5)) {
                match.matchedHash = "MD5:" + entry.md5;
            } else if (!input.sha1.isEmpty() && normalizeHash(input.sha1) == normalizeHash(entry.sha1)) {
                match.matchedHash = "SHA1:" + entry.sha1;
            }

            const QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
            const QString entryBase = QFileInfo(entry.romName).completeBaseName().toLower();
            if (signalBase == entryBase) {
                match.filenameMatch = true;
                match.confidenceScore += 50;
                match.matchSignalCount++;
            }

            const qint64 sizeDiff = qAbs(input.fileSize - entry.size);
            if (sizeDiff <= 1024) {
                match.sizeMatch = true;
                match.confidenceScore += 30;
                match.matchSignalCount++;
            }

            if (!input.serial.isEmpty() && !entry.serial.isEmpty()) {
                if (input.serial.compare(entry.serial, Qt::CaseInsensitive) == 0) {
                    match.serialMatch = true;
                    match.confidenceScore += 20;
                    match.matchSignalCount++;
                }
            }

            matches.append(match);
        }
    }

    if (matches.isEmpty()) {
        qDebug() << "  No hash match, trying filename + size matching...";

        const QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
        QSet<QString> seenEntries;

        for (auto it = m_crc32Index.constBegin(); it != m_crc32Index.constEnd(); ++it) {
            const ClrMameProEntry &entry = it.value();
            const QString entryKey = entry.gameName + "|" + entry.romName;
            if (seenEntries.contains(entryKey)) {
                continue;
            }
            seenEntries.insert(entryKey);

            const QString entryBase = QFileInfo(entry.romName).completeBaseName().toLower();
            const bool filenameExact = (signalBase == entryBase);
            const qint64 sizeDiff = qAbs(input.fileSize - entry.size);
            const bool sizeMatch = (sizeDiff <= 1024);

            if (filenameExact && sizeMatch) {
                MultiSignalMatch match;
                match.entry = entry;
                match.filenameMatch = true;
                match.sizeMatch = true;
                match.confidenceScore = 80;
                match.matchSignalCount = 2;

                if (!input.serial.isEmpty() && !entry.serial.isEmpty()) {
                    if (input.serial.compare(entry.serial, Qt::CaseInsensitive) == 0) {
                        match.serialMatch = true;
                        match.confidenceScore += 20;
                        match.matchSignalCount++;
                    }
                }

                matches.append(match);
                qDebug() << "  Filename+size match:" << entry.gameName << "score:" << match.confidenceScore;
                break;
            }
        }
    }

    std::sort(matches.begin(), matches.end(), [](const MultiSignalMatch &a, const MultiSignalMatch &b) {
        return a.confidenceScore > b.confidenceScore;
    });

    qDebug() << "LocalDatabaseProvider: Found" << matches.size() << "multi-signal matches";
    if (!matches.isEmpty()) {
        qDebug() << "  Best match:" << matches.first().entry.gameName
                 << "confidence:" << matches.first().confidencePercent() << "%"
                 << "signals:" << matches.first().matchSignalCount;
    }

    return matches;
}

} // namespace Remus