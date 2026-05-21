#pragma once

#include "dat_parser.h"

#include <QList>
#include <QString>

namespace Remus {
namespace VerificationHashMatcher {

QList<QString> orderedOfficialHashTypes(const QString &preferredHashType);

bool findOfficialDatMatch(const QMap<QString, DatRomEntry> &datEntries,
                          const QString &preferredHashType,
                          const QString &crc32,
                          const QString &md5,
                          const QString &sha1,
                          const QString &sha256,
                          DatRomEntry &matchedEntry,
                          QString &matchedHash,
                          QString &matchedHashType);

QString datEntryKey(const DatRomEntry &entry);

} // namespace VerificationHashMatcher
} // namespace Remus
