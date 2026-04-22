#pragma once

#include <QStringList>

namespace Remus {
namespace Metadata {

/**
 * @brief Static helpers for building libretro thumbnail CDN URLs.
 *
 * These methods were originally static members of LocalDatabaseProvider.
 * Extracting them here allows CompendiumProvider, bundle helpers, and any
 * future provider to generate thumbnail candidates without a dependency on
 * LocalDatabaseProvider.
 *
 * URL format: https://thumbnails.libretro.com/{System}/{Type}/{Name}.png
 */
class ThumbnailUrlHelper
{
public:
    ThumbnailUrlHelper() = delete;

    /**
     * @brief Replace libretro-invalid characters with underscores.
     *
     * Per the libretro thumbnail spec, the characters &amp; * / : \ &lt; &gt; ? | "
     * must be replaced with underscore in both system and game name path segments.
     */
    static QString sanitizeThumbnailName(const QString &name);

    /**
     * @brief Strip parenthetical ISO 639-1 language tags from a game name.
     *
     * Examples removed: (En), (En,Ja), (En,Fr,De,Es,It).
     */
    static QString stripLanguageTags(const QString &name);

    /**
     * @brief Build a single fully-encoded thumbnail URL.
     * @param systemName Libretro system directory name, e.g. "Nintendo - NES"
     * @param gameName   Canonical game name (will be sanitized internally)
     * @param type       Thumbnail type: "Named_Boxarts", "Named_Snaps", "Named_Titles"
     */
    static QString buildThumbnailUrl(const QString &systemName,
                                     const QString &gameName,
                                     const QString &type);

    /**
     * @brief Generate a prioritised list of thumbnail URL candidates for a game.
     *
     * Candidate 1: exact name.
     * Candidate 2: language-tags-stripped name (when different from exact).
     */
    static QStringList generateThumbnailCandidates(const QString &systemName,
                                                   const QString &gameName,
                                                   const QString &type);
};

} // namespace Metadata
} // namespace Remus
