#pragma once

#include <QString>
#include <QStringList>
#include "database.h"

namespace Remus {

QString selectBestMatchHash(const FileRecord &file);
QString deriveMatchingDisplayName(const FileRecord &file);

/// All non-empty digests for a ROM, in verification-aligned order (sha256 → preferred → sha1 → md5 → crc32).
QStringList orderedMatchHashValues(const QString &preferredHashType, const QString &crc32, const QString &md5,
    const QString &sha1, const QString &sha256 = QString());

} // namespace Remus