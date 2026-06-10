#pragma once

#include "dat_parser.h"

#include <QList>
#include <QString>

namespace Remus {
namespace VerificationHashMatcher {

    /**
     * @brief Canonical digest try-order for official and patch catalog lookups.
     *
     * Order: sha256 → system preferred type → sha1 → md5 → crc32 (each type once).
     * Metadata matching uses the same order via orderedMatchHashValues() in match_utils.cpp.
     */
    QList<QString> orderedOfficialHashTypes(const QString &preferredHashType);

    /// Shared hash cascade used by verification (official + patch) and metadata match
    /// (via orderedMatchHashValues in match_utils.cpp).
    bool findHashInDatEntries(const QMap<QString, DatRomEntry> &datEntries, const QString &preferredHashType,
        const QString &crc32, const QString &md5, const QString &sha1, const QString &sha256, DatRomEntry &matchedEntry,
        QString &matchedHash, QString &matchedHashType);

    bool findOfficialDatMatch(const QMap<QString, DatRomEntry> &datEntries, const QString &preferredHashType,
        const QString &crc32, const QString &md5, const QString &sha1, const QString &sha256, DatRomEntry &matchedEntry,
        QString &matchedHash, QString &matchedHashType);

    QString datEntryKey(const DatRomEntry &entry);

} // namespace VerificationHashMatcher
} // namespace Remus
