#pragma once

#include "database_types.h"

#include <QString>

namespace Remus {

/**
 * @brief Shared helpers for detecting and grouping multi-disc ROM sets.
 *
 * Filename heuristics match Redump-style "(Disc N)" naming. Persisted sets use
 * `files.disc_set_key` and `files.disc_number`; game-confirmed sets use
 * `game:<gameId>|<systemId>` keys.
 */
class DiscSetUtils {
public:
    /** Best label string for disc parsing (archive path, inner path, or filename). */
    static QString labelPath(const QString &currentPath, const QString &archivePath,
        const QString &archiveInternalPath, const QString &filename);

    static bool isMultiDisc(const QString &labelPath);
    static QString extractBaseTitle(const QString &labelPath);
    static int extractDiscNumber(const QString &labelPath);

    /** Stable filename-based group key: normalized base title + system display name. */
    static QString groupKey(const QString &labelPath, const QString &systemName);

    /** Stable match-based group key for confirmed multi-disc games. */
    static QString gameDiscSetKey(int gameId, int systemId);

    static QString discRowLabel(const QString &labelPath, int discNumber);

    /** Populate disc_set_key, disc_number, and base_title from paths (scan-time). */
    static void applyScanDiscMetadata(FileRecord &record, const QString &systemName);
};

} // namespace Remus
