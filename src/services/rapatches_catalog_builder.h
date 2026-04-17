#pragma once

#include "mod_catalog_provider.h"

#include <QDir>
#include <QRegularExpression>
#include <QString>

namespace Remus {

/**
 * @brief Builds a ModEntry catalog from a locally cloned RAPatches repository.
 *
 * RAPatches is organised as: {System}/{Type}/{GAMEID-Title.zip}
 *   - Types: Fix, Hacks, Translation, Improvement, MSU-1, Subset, GTConversion
 *   - Filenames inside zips: Title (Region) (Lang) (Version) (Author).bps
 *   - Each zip contains a readme.txt with base ROM MD5/CRC32
 *
 * This builder walks the directory tree, parses the structure, and emits
 * ModEntry objects that slot directly into the existing catalog system.
 *
 * Tier 1 of the three-tier mod source architecture (local-first).
 */
class RAPatchesCatalogBuilder {
public:
    struct BuildResult {
        QList<ModEntry> mods;
        int filesScanned = 0;
        int filesSkipped = 0;
        QString error;
    };

    /**
     * @brief Build a catalog from a local RAPatches repo clone.
     * @param repoPath Root directory of the cloned RAPatches repository
     * @return BuildResult with the generated mod entries
     */
    BuildResult buildFromDirectory(const QString &repoPath) const;

    /**
     * @brief Write mod entries to a catalog JSON file.
     * @param mods The entries to serialise
     * @param outputPath Path for the output JSON file
     * @return Empty string on success, error description on failure
     */
    static QString writeCatalogJson(const QList<ModEntry> &mods,
                                    const QString &outputPath);

    /**
     * @brief Parse a RAPatches filename into mod metadata.
     *
     * Expected format: Title (Region) (Language) (Version) (Author).ext
     * Not all fields are always present.
     */
    struct ParsedFilename {
        QString title;
        QString region;
        QString language;
        QString version;
        QString author;
        QString format;   // bps, ips, xdelta, ppf, ups
    };

    static ParsedFilename parseFilename(const QString &filename);

    /**
     * @brief Extract base ROM hash info from a readme.txt inside a zip.
     */
    struct ReadmeHashes {
        QString baseMd5;
        QString baseCrc32;
        QString baseRomName;
    };

    static ReadmeHashes parseReadme(const QString &readmeContent);

    /**
     * @brief Map RAPatches directory names to normalised system names.
     */
    static QString normaliseSystemName(const QString &dirName);

    /**
     * @brief Map RAPatches type directory names to mod type strings.
     */
    static QString normaliseTypeName(const QString &dirName);

private:
    void scanSystemDir(const QDir &systemDir,
                       const QString &system,
                       BuildResult &result) const;

    void scanTypeDir(const QDir &typeDir,
                     const QString &system,
                     const QString &type,
                     BuildResult &result) const;

    ModEntry buildEntryFromZip(const QString &zipPath,
                               const QString &system,
                               const QString &type) const;

    ModEntry buildEntryFromPatch(const QString &patchPath,
                                 const QString &system,
                                 const QString &type) const;
};

} // namespace Remus
