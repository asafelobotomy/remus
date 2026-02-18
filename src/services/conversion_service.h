#ifndef REMUS_CONVERSION_SERVICE_H
#define REMUS_CONVERSION_SERVICE_H

#include <functional>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

#include "../core/chd_converter.h"
#include "../core/archive_extractor.h"
#include "../core/archive_creator.h"

namespace Remus {

class Database;

/**
 * @brief Shared conversion service (non-QObject, callback-based)
 *
 * Wraps CHDConverter + ArchiveExtractor.
 * Usable by both GUI controllers and TUI screens.
 */
class ConversionService {
public:
    using ProgressCallback = std::function<void(int percent, const QString &info)>;
    using LogCallback      = std::function<void(const QString &message)>;

    ConversionService();
    ~ConversionService();

    // ── CHD Conversion ────────────────────────────────────

    /**
     * @brief Convert a single disc image to CHD
     * @param path       Path to .cue/.iso/.gdi
     * @param codec      Compression codec
     * @param outputPath Optional output path (empty = same dir)
     * @param progressCb Progress callback (percent, info)
     * @return Conversion result
     */
    CHDConversionResult convertToCHD(const QString &path,
                                     CHDCodec codec = CHDCodec::Auto,
                                     const QString &outputPath = {},
                                     ProgressCallback progressCb = nullptr);

    /**
     * @brief Extract a CHD back to BIN/CUE
     * @param chdPath    Path to .chd
     * @param outputPath Optional output path
     * @param progressCb Progress callback
     * @return Conversion result
     */
    CHDConversionResult extractCHD(const QString &chdPath,
                                   const QString &outputPath = {},
                                   ProgressCallback progressCb = nullptr);

    /**
     * @brief Batch convert disc images to CHD
     * @param inputPaths  List of input paths
     * @param outputDir   Output directory
     * @param codec       Compression codec
     * @param progressCb  Per-file progress (percent, file)
     * @return List of conversion results
     */
    QList<CHDConversionResult> batchConvertToCHD(const QStringList &inputPaths,
                                                 const QString &outputDir,
                                                 CHDCodec codec = CHDCodec::Auto,
                                                 ProgressCallback progressCb = nullptr);

    /**
     * @brief Verify a CHD file
     */
    CHDVerifyResult verifyCHD(const QString &chdPath);

    /**
     * @brief Get information about a CHD file
     */
    CHDInfo getCHDInfo(const QString &chdPath);

    // ── Archive Extraction ────────────────────────────────

    /**
     * @brief Extract an archive
     * @param archivePath Path to archive (zip, 7z, rar)
     * @param outputDir   Output directory
     * @param progressCb  Progress callback
     * @return Extraction result
     */
    ExtractionResult extractArchive(const QString &archivePath,
                                    const QString &outputDir,
                                    ProgressCallback progressCb = nullptr);

    /**
     * @brief Extract an archive and update DB file paths
     * @param archivePath Path to archive
     * @param outputDir   Output directory
     * @param db          Database to update
     * @param progressCb  Progress callback
     * @return Extraction result
     */
    ExtractionResult extractArchiveWithDbUpdate(const QString &archivePath,
                                                const QString &outputDir,
                                                Database *db,
                                                ProgressCallback progressCb = nullptr);

    // ── Tool Status ───────────────────────────────────────

    /**
     * @brief Check if chdman is available
     */
    bool isChdmanAvailable() const;

    /**
     * @brief Get chdman version string
     */
    QString getChdmanVersion() const;

    /**
     * @brief Set custom chdman path
     */
    void setChdmanPath(const QString &path);

    /**
     * @brief Get available archive extraction tools
     */
    QMap<ArchiveFormat, bool> getArchiveToolStatus() const;

    /**
     * @brief Check if a specific archive format can be extracted
     */
    bool canExtract(const QString &path) const;

    // ── Archive Compression ───────────────────────────────

    /**
     * @brief Compress files into an archive
     * @param inputPaths   List of files to compress
     * @param outputArchive  Output archive path
     * @param format       Archive format (ZIP or SevenZip)
     * @param progressCb   Progress callback
     * @return Compression result
     */
    CompressionResult compressToArchive(const QStringList &inputPaths,
                                        const QString &outputArchive,
                                        ArchiveFormat format = ArchiveFormat::ZIP,
                                        ProgressCallback progressCb = nullptr);

    /**
     * @brief Batch compress directories into individual archives
     * @param dirs       Directories to compress
     * @param outputDir  Output directory
     * @param format     Archive format
     * @param progressCb Progress callback
     * @return List of compression results
     */
    QList<CompressionResult> batchCompressToArchive(const QStringList &dirs,
                                                    const QString &outputDir,
                                                    ArchiveFormat format = ArchiveFormat::ZIP,
                                                    ProgressCallback progressCb = nullptr);

    /**
     * @brief Check if a specific archive format can be created
     */
    bool canCompress(ArchiveFormat format) const;

    /**
     * @brief Get available archive compression tools
     */
    QMap<ArchiveFormat, bool> getArchiveCompressionToolStatus() const;

    /**
     * @brief Cancel any running conversion/extraction
     */
    void cancel();

    /**
     * @brief Check if a conversion is running
     */
    bool isRunning() const;

    /**
     * @brief Access underlying CHDConverter (for advanced use)
     */
    CHDConverter *chdConverter() { return m_chdConverter; }

    /**
     * @brief Access underlying ArchiveExtractor
     */
    ArchiveExtractor *archiveExtractor() { return m_archiveExtractor; }

    /**
     * @brief Access underlying ArchiveCreator
     */
    ArchiveCreator *archiveCreator() { return m_archiveCreator; }

private:
    CHDConverter     *m_chdConverter     = nullptr;
    ArchiveExtractor *m_archiveExtractor = nullptr;
    ArchiveCreator   *m_archiveCreator   = nullptr;
};

} // namespace Remus

#endif // REMUS_CONVERSION_SERVICE_H
