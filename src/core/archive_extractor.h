#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

namespace Remus {

/**
 * @brief Supported archive formats
 */
enum class ArchiveFormat {
    Unknown,
    ZIP,
    SevenZip, // 7z
    RAR,
    GZip,
    TarGz,
    TarBz2,
    Tar,
    TarXz
};

/**
 * @brief Information about an archive file
 */
struct ArchiveInfo {
    QString path;
    ArchiveFormat format = ArchiveFormat::Unknown;
    qint64 compressedSize = 0; // Size of archive
    qint64 uncompressedSize = 0; // Total extracted size
    int fileCount = 0; // Number of files in archive
    QStringList contents; // List of contained files
    QMap<QString, qint64> entrySizes; // Uncompressed size per archive entry
    QStringList unsafeEntries; // Entries rejected as unsafe paths
};

/**
 * @brief Result of an extraction operation
 */
struct ExtractionResult {
    bool success = false;
    QString archivePath;
    QString outputDir;
    int filesExtracted = 0;
    int failedFiles = 0;
    qint64 bytesExtracted = 0;
    QString error;
    QStringList extractedFiles; // List of extracted file paths
};

/**
 * @brief Archive extractor supporting all formats provided by libarchive
 *        (ZIP, 7z, RAR, tar, gzip, bzip2, xz, and compound variants).
 *
 * Uses libarchive for all extraction — no external tools required.
 */
class ArchiveExtractor : public QObject {
    Q_OBJECT

public:
    explicit ArchiveExtractor(QObject *parent = nullptr);
    ~ArchiveExtractor() override = default;

    /**
     * @brief Check which extraction formats are available (always all supported formats).
     */
    QMap<ArchiveFormat, bool> getAvailableTools() const;

    bool canExtract(ArchiveFormat format) const;
    bool canExtract(const QString &path) const;

    // No-op setters kept for API compatibility
    void setUnzipPath(const QString &) { }
    void setSevenZipPath(const QString &) { }
    void setUnrarPath(const QString &) { }

    /**
     * @brief Get information about an archive without extracting
     */
    ArchiveInfo getArchiveInfo(const QString &path);

    /**
     * @brief Detect archive format from file extension
     */
    static ArchiveFormat detectFormat(const QString &path);

    /**
     * @brief Normalize an archive member path for safe reuse.
     * @return Clean relative path, or empty when the input is unsafe.
     */
    static QString normalizeArchiveMemberPath(const QString &path);

    /**
     * @brief Extract archive to directory
     * @param archivePath   Path to archive file
     * @param outputDir     Output directory (defaults to archive's directory)
     * @param createSubfolder Create subfolder named after the archive stem
     */
    ExtractionResult extract(
        const QString &archivePath, const QString &outputDir = QString(), bool createSubfolder = false);

    /**
     * @brief Extract a single named member from an archive
     */
    ExtractionResult extractFile(const QString &archivePath, const QString &fileName, const QString &outputDir);

    /**
     * @brief Batch extract multiple archives
     */
    QList<ExtractionResult> batchExtract(
        const QStringList &archivePaths, const QString &outputDir = QString(), bool createSubfolders = true);

    /**
     * @brief Stream the first maxBytes bytes from an archive member.
     *
     * Decompresses only the requested prefix — suitable for disc magic-byte
     * detection without extracting the full member.
     */
    QByteArray readMemberPrefix(const QString &archivePath, const QString &memberPath, qint64 maxBytes);

    void cancel() {
        m_cancelled = true;
    }
    bool isRunning() const {
        return false;
    } // extraction is synchronous

signals:
    void extractionStarted(const QString &archivePath, const QString &outputDir);
    void extractionProgress(int percent, const QString &currentFile);
    void extractionCompleted(const ExtractionResult &result);
    void batchProgress(int completed, int total);
    void errorOccurred(const QString &error);

private:
    bool m_cancelled = false;

    ExtractionResult extractToDir(
        const QString &archivePath, const QString &outputDir, const QString &singleMember = QString());
};

} // namespace Remus
