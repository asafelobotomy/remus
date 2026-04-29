#include "verification_hash_matcher.h"

namespace Remus {
namespace VerificationHashMatcher {

QList<QString> orderedOfficialHashTypes(const QString &preferredHashType)
{
    QList<QString> ordered;

    auto appendType = [&ordered](const QString &hashType) {
        if (!ordered.contains(hashType)) {
            ordered.append(hashType);
        }
    };

    appendType(preferredHashType);
    appendType(QStringLiteral("sha1"));
    appendType(QStringLiteral("md5"));
    appendType(QStringLiteral("crc32"));
    return ordered;
}

bool findOfficialDatMatch(const QMap<QString, DatRomEntry> &datEntries,
                          const QString &preferredHashType,
                          const QString &crc32,
                          const QString &md5,
                          const QString &sha1,
                          DatRomEntry &matchedEntry,
                          QString &matchedHash,
                          QString &matchedHashType)
{
    for (const QString &hashType : orderedOfficialHashTypes(preferredHashType)) {
        QString candidateHash;
        if (hashType == QStringLiteral("sha1")) {
            candidateHash = sha1.toLower();
        } else if (hashType == QStringLiteral("md5")) {
            candidateHash = md5.toLower();
        } else {
            candidateHash = crc32.toLower();
        }

        if (!candidateHash.isEmpty() && datEntries.contains(candidateHash)) {
            matchedEntry = datEntries.value(candidateHash);
            matchedHash = candidateHash;
            matchedHashType = hashType;
            return true;
        }
    }

    return false;
}

QString datEntryKey(const DatRomEntry &entry)
{
    return entry.gameName + QLatin1Char('\x1f')
        + entry.romName + QLatin1Char('\x1f')
        + QString::number(entry.size);
}

} // namespace VerificationHashMatcher
} // namespace Remus
