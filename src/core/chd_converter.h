#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

namespace Remus {

/**
 * @brief CHD compression codec options
 */
enum class CHDCodec {
    LZMA,       // Best compression (default)
    ZLIB,       // Faster, less compression
    FLAC,       // For audio tracks
    Huffman,    // Huffman encoding
    Auto        // Let chdman decide
};

/**
 * @brief Result of a CHD conversion operation
 */
struct CHDConversionResult {
    bool success = false;
    QString inputPath;
    QString outputPath;
    qint64 inputSize = 0;      // Original file size in bytes
    qint64 outputSize = 0;     // CHD file size in bytes
    double compressionRatio = 0.0;  // e.g., 0.45 = 45% of original
    QString error;
    int exitCode = 0;
    QString stdOutput;
    QString stdError;
};

/**
 * @brief Result of CHD verification
 */
struct CHDVerifyResult {
    bool valid = false;
    QString path;
    QString error;
    QString details;           // Metadata from CHD
};

/**
 * @brief Information about a CHD file
 */
struct CHDInfo {
    QString path;
    int version = 0;           // CHD version (4, 5, etc.)
    QString compression;       // Compression type
    qint64 logicalSize = 0;    // Uncompressed size
    qint64 physicalSize = 0;   // Compressed size on disk
    QString sha1;              // SHA1 hash of raw data
    QString parentSha1;        // Parent SHA1 (if applicable)
    int diskType = 0;          // 0=unknown, 1=HDD, 2=CD, 3=DVD
};

/**
 * @brief Wrapper for chdman tool to convert disc images to CHD format
 * 
 * CHD (Compressed Hunks of Data) is a lossless compression format that
 * provides 30-60% space savings for disc-based games while maintaining
 * full compatibility with RetroArch and most emulators.
 * 
 * Requires chdman to be installed (part of MAME tools):
 * - Linux: `sudo apt install mame-tools` or `sudo pacman -S mame-tools`
 * - macOS: `brew install mame`
 * - Windows: Download from MAME releases
 */
class CHDConverter : public QObject {
    Q_OBJECT

public:
    explicit CHDConverter(QObject *parent = nullptr);
    ~CHDConverter() = default;

    /**
     * @brief Check if chdman is available on the system
     * @return True if chdman is found and executable
     */
    bool isChdmanAvailable() const;

    /**
     * @brief Get chdman version string
     * @return Version string or empty if not available
     */
    QString getChdmanVersion() const;

    /**
     * @brief Set path to chdman binary (optional, uses PATH by default)
     */
    void setChdmanPath(const QString &path);

    /**
     * @brief Set number of processors to use for conversion
     * @param numProcessors Number of threads (0 = auto-detect)
     */
    void setNumProcessors(int numProcessors);

    /**
     * @brief Set compression codec
     */
    void setCodec(CHDCodec codec);

    /**
     * @brief Convert BIN/CUE to CHD
     * @param cuePath Path to .cue file
     * @param outputPath Output .chd path (optional, defaults to same location)
     * @return Conversion result
     */
    CHDConversionResult convertCueToCHD(const QString &cuePath, 
                                         const QString &outputPath = QString());

    /**
     * @brief Convert ISO to CHD
     * @param isoPath Path to .iso file
     * @param outputPath Output .chd path (optional)
     * @return Conversion result
     */
    CHDConversionResult convertIsoToCHD(const QString &isoPath,
                                         const QString &outputPath = QString());

    /**
     * @brief Convert GDI to CHD (Dreamcast)
     * @param gdiPath Path to .gdi file
     * @param outputPath Output .chd path (optional)
     * @return Conversion result
     */
    CHDConversionResult convertGdiToCHD(const QString &gdiPath,
                                         const QString &outputPath = QString());

    /**
     * @brief Extract CHD back to BIN/CUE
     * @param chdPath Path to .chd file
     * @param outputPath Output .cue path (optional)
     * @return Conversion result
     */
    CHDConversionResult extractCHDToCue(const QString &chdPath,
                                         const QString &outputPath = QString());

    /**
     * @brief Verify CHD file integrity
     * @param chdPath Path to .chd file
     * @return Verification result
     */
    CHDVerifyResult verifyCHD(const QString &chdPath);

    /**
     * @brief Get information about a CHD file
     * @param chdPath Path to .chd file
     * @return CHD information
     */
    CHDInfo getCHDInfo(const QString &chdPath);

    /**
     * @brief Batch convert multiple files to CHD
     * @param inputPaths List of input file paths
     * @param outputDir Output directory (optional)
     * @return List of conversion results
     */
    QList<CHDConversionResult> batchConvert(const QStringList &inputPaths,
                                             const QString &outputDir = QString());

    /**
     * @brief Cancel current conversion
     */
    void cancel();

    /**
     * @brief Check if conversion is in progress
     */
    bool isRunning() const;

signals:
    /**
     * @brief Emitted when conversion starts
     */
    void conversionStarted(const QString &inputPath, const QString &outputPath);

    /**
     * @brief Emitted with progress updates (if available from chdman output)
     */
    void conversionProgress(int percent, const QString &status);

    /**
     * @brief Emitted when a single conversion completes
     */
    void conversionCompleted(const CHDConversionResult &result);

    /**
     * @brief Emitted during batch processing
     */
    void batchProgress(int completed, int total);

    /**
     * @brief Emitted when conversion is cancelled
     */
    void conversionCancelled();

    /**
     * @brief Emitted on error
     */
    void errorOccurred(const QString &error);

private:
    /**
     * @brief Run chdman command and wait for completion
     */
    CHDConversionResult runChdman(const QStringList &args,
                                   const QString &inputPath,
                                   const QString &outputPath);

    /**
     * @brief Get default output path (replace extension with .chd)
     */
    QString getDefaultOutputPath(const QString &inputPath, const QString &targetExt = "chd");

    /**
     * @brief Get codec string for chdman
     */
    QString getCodecString() const;

    /**
     * @brief Calculate file size
     */
    qint64 getFileSize(const QString &path) const;

    QString m_chdmanPath;
    int m_numProcessors = 0;
    CHDCodec m_codec = CHDCodec::Auto;
    QProcess *m_process = nullptr;
    bool m_cancelled = false;
};

} // namespace Remus
