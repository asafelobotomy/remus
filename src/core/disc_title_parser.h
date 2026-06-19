#pragma once

#include <QString>

namespace Remus {

/**
 * @brief Parsed disc/title metadata from a filename or DAT game name.
 *
 * Supports Redump-style "(Disc N)", "(Disc N of M)", TOSEC-style bracket tags,
 * and split-path subtitles such as "(Leon)" on a separate disc surface.
 */
struct DiscTitleInfo {
    QString rawTitle;
    /// Display-oriented base title with disc/region tags removed.
    QString baseTitle;
    /// Lowercase identity-normalized base for @ref DiscSetKey (excludes disc index and bracket variants).
    QString identityBase;
    int discNumber = 0;
    int discCount = 0; ///< 0 when unknown
    QString setVariant; ///< Pressing / ring code / edition suffix (e.g. @c 1S )
    QString pathSubtitle; ///< Split-path label (e.g. @c Leon ) when distinct from variant
    QString setRole; ///< @c game , @c audio , @c bonus , @c data
    bool isMultiDisc = false;
};

/**
 * @brief Shared disc title parsing for library scan and compendium ingest.
 */
class DiscTitleParser {
public:
    static bool isMultiDisc(const QString &label);
    static QString extractBaseTitle(const QString &label);
    static int extractDiscNumber(const QString &label);
    static int extractDiscCount(const QString &label);

    /// Full parse suitable for DAT game names and filenames.
    static DiscTitleInfo parseTitle(const QString &title);

    /**
     * @brief Normalize a title for conservative identity linking.
     *
     * Lowercases, strips punctuation/articles, and removes trailing disc/cd tags.
     */
    static QString normalizeForIdentity(const QString &raw);
};

} // namespace Remus
