#pragma once

#include <QStringList>

class QSqlDatabase;

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
    class ThumbnailUrlHelper {
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
        static QString buildThumbnailUrl(const QString &systemName, const QString &gameName, const QString &type);

        /**
         * @brief Generate a prioritised list of thumbnail URL candidates for a game.
         *
         * Candidate 1: exact name.
         * Candidate 2: language-tags-stripped name (when different from exact).
         */
        static QStringList generateThumbnailCandidates(
            const QString &systemName, const QString &gameName, const QString &type);

        /**
         * @brief Map asset type shorthand to libretro thumbnail folder name.
         */
        static QString libretroFolderForAssetType(const QString &assetType);

        /**
         * @brief Resolve repo-relative storage path to absolute filesystem path.
         */
        static QString resolveStoragePath(const QString &repoRoot, const QString &storagePath);

        /**
         * @brief Derive repository root from a compendium database path (data/compendium/name.db).
         */
        static QString repoRootFromCompendiumDb(const QString &databasePath);

        /**
         * @brief Look up local blob path from game_assets for a game.
         */
        static QString lookupGameAssetPath(QSqlDatabase &db, const QString &gameId, const QString &assetType);

        /**
         * @brief Resolve artwork URL with local blob preference.
         */
        static QString resolveArtworkUrl(QSqlDatabase &db, const QString &repoRoot, const QString &gameId,
            const QString &systemName, const QString &canonicalTitle, const QString &assetType, bool strictOffline);
    };

} // namespace Metadata
} // namespace Remus
