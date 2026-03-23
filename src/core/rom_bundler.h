#pragma once

#include <QObject>
#include <QString>

#include "database.h"
#include "archive_extractor.h"
#include "archive_creator.h"
#include "chd_converter.h"
#include "../metadata/metadata_provider.h"

namespace Remus {

/**
 * @brief Packages a matched ROM into a self-contained archive bundle.
 *
 * A bundle is a ZIP (or 7z) archive containing:
 *   <GameTitle (Region) (Year)>.ext   — the ROM file
 *   .remus.md                         — hidden processing marker + metadata
 *   artwork/boxfront.jpg              — box-art (optional)
 *
 * The presence of .remus.md inside an archive is the canonical signal
 * that Remus has already processed that file; the organise step skips
 * any archive that already contains this marker.
 */
class RomBundler : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Configuration for a bundle operation
     */
    struct BundleConfig {
        bool         includeBoxArt   = true;
        bool         dryRun          = false;
        ArchiveFormat outputFormat   = ArchiveFormat::ZIP;
        /// Path to an already-downloaded box-art file (handled by caller before bundling).
        /// If empty and includeBoxArt is true, the step is silently skipped.
        QString      artworkPath;
        /// When true, supported disc-image inputs (.cue/.iso/.img/.gdi) are
        /// converted to a CHD before packaging the final bundle archive.
        bool         convertDiscsToChd = false;
    };

    /**
     * @brief Result of a bundle operation
     */
    struct BundleResult {
        bool    success               = false;
        QString outputPath;           ///< Final archive path
        bool    skippedAlreadyBundled = false;
        QString error;
    };

    explicit RomBundler(Database &db, QObject *parent = nullptr);

    /**
     * @brief Check whether an archive already contains a .remus.md marker.
     *
     * Uses ArchiveExtractor::getArchiveInfo() — does not extract anything.
     *
     * @param archivePath  Path to a ZIP or 7z file
     * @return true if .remus.md is present in the archive's content list
     */
    bool isAlreadyBundled(const QString &archivePath);

    /**
     * @brief Bundle a matched ROM.
     *
     * For compressed input: extracts to a temp directory, adds metadata
     * files, and repacks into the destination.
     * For uncompressed input: copies into a temp directory, adds metadata
     * files, and packs into the destination.
     *
     * On success, marks the file record as processed ("bundled") in the DB.
     *
     * @param file         FileRecord from the database
     * @param match        Match result (game title, confidence, method, …)
     * @param metadata     Full game metadata from provider
     * @param destinationDir  Target directory for the output archive
     * @param config       Bundle options
     * @return BundleResult
     */
    BundleResult bundle(const FileRecord              &file,
                        const Database::MatchResult   &match,
                        const GameMetadata            &metadata,
                        const QString                 &destinationDir,
                        const BundleConfig            &config);

signals:
    void progressMessage(const QString &msg);

private:
    /**
     * @brief Generate the contents of the .remus.md marker file.
     */
    QString generateMarkerContent(const FileRecord            &file,
                                  const Database::MatchResult &match,
                                  const GameMetadata          &metadata) const;

    Database        &m_db;
    ArchiveExtractor m_extractor;
    ArchiveCreator   m_creator;
};

} // namespace Remus
