#pragma once

#include <QHash>
#include <QString>

namespace Remus {
namespace Compendium {

    struct HasheousOfflineMatch {
        QString igdbId;
        QString description;
        QString genre;
        QString raGameId;
    };

    /// Resolve path to Hasheous offline dump root (@c data/hasheous/dumps), or empty if absent.
    QString findHasheousDumpDir();

    /// Load or rebuild a hash→metadata index from offline dump JSON files.
    /// Uses a sidecar SQLite cache keyed by dump directory mtime.
    bool loadHasheousOfflineIndex(
        const QString &dumpDir, QHash<QString, HasheousOfflineMatch> &indexByHash, QString &error);

    /// Lookup by normalized hash (any of crc32/md5/sha1/sha256 keys in @p indexByHash).
    bool lookupHasheousOfflineMatch(const QHash<QString, HasheousOfflineMatch> &indexByHash, const QString &crc32,
        const QString &md5, const QString &sha1, const QString &sha256, HasheousOfflineMatch &out);

} // namespace Compendium
} // namespace Remus
