#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QProcess>

namespace Remus {

/**
 * @brief Supported archive formats
 */
enum class ArchiveFormat {
    Unknown,
    ZIP,
    SevenZip,  // 7z
    RAR,
    GZip,
    TarGz,
    TarBz2
};

/**
 * @brief Information about an archive file
 */
struct ArchiveInfo {
    QString path;
    ArchiveFormat format = ArchiveFormat::Unknown;
    qint64 compressedSize = 0;     // Size of archive
    qint64 uncompressedSize = 0;   // Total extracted size
    int fileCount = 0;             // Number of files in archive
    QStringList contents;          // List of contained files
};

/**
 * @brief Result of an extraction operation
 */
struct ExtractionResult {
    bool success = false;
    QString archivePath;
    QString outputDir;
    int filesExtracted = 0;
    qint64 bytesExtracted = 0;
    QString error;
    QStringList extractedFiles;    // List of extracted file paths
};

/**
 * @brief Archive extractor supporting ZIP, 7z, and RAR formats
 * 
 * Uses external tools for extraction:
 * - ZIP: unzip (standard on most systems)
 * - 7z: 7z or 7za (7-zip command line)
 * - RAR: unrar or rar
 * 
 * Automatically detects format from file extension.
 */
class ArchiveExtractor : public QObject {
    Q_OBJECT

public:
    explicit ArchiveExtractor(QObject *parent = nullptr);
    ~ArchiveExtractor() = default;

    /**
     * @brief Check which extraction tools are available
     * @return Map of format -> available
     */
    QMap<ArchiveFormat, bool> getAvailableTools() const;

    /**
     * @brief Check if a specific format can be extracted
     */
    bool canExtract(ArchiveFormat format) const;
    bool canExtract(const QString &path) const;

    /**
     * @brief Set custom path for extraction tools
     */
    void setUnzipPath(const QString &path);
    void setSevenZipPath(const QString &path);
    void setUnrarPath(const QString &path);

    /**
     * @brief Get information about an archive without extracting
     */
    ArchiveInfo getArchiveInfo(const QString &path);

    /**
     * @brief Detect archive format from file path
     */
    static ArchiveFormat detectFormat(const QString &path);

    /**
     * @brief Extract archive to directory
     * @param archivePath Path to archive file
     * @param outputDir Output directory (optional, uses archive directory)
     * @param createSubfolder Create subfolder with archive name
     * @return Extraction result
     */
    ExtractionResult extract(const QString &archivePath,
                             const QString &outputDir = QString(),
                             bool createSubfolder = false);

    /**
     * @brief Extract specific file from archive
     * @param archivePath Path to archive
     * @param fileName File to extract (relative path within archive)
     * @param outputDir Output directory
     * @return Extraction result
     */
    ExtractionResult extractFile(const QString &archivePath,
                                  const QString &fileName,
                                  const QString &outputDir);

    /**
     * @brief Batch extract multiple archives
     * @param archivePaths List of archive paths
     * @param outputDir Output directory for all
     * @param createSubfolders Create subfolder for each archive
     * @return List of extraction results
     */
    QList<ExtractionResult> batchExtract(const QStringList &archivePaths,
                                          const QString &outputDir = QString(),
                                          bool createSubfolders = true);

    /**
     * @brief Cancel current extraction
     */
    void cancel();

    /**
     * @brief Check if extraction is running
     */
    bool isRunning() const;

signals:
    void extractionStarted(const QString &archivePath, const QString &outputDir);
    void extractionProgress(int percent, const QString &currentFile);
    void extractionCompleted(const ExtractionResult &result);
    void batchProgress(int completed, int total);
    void errorOccurred(const QString &error);

protected:
    struct ProcessResult {
        bool started = false;
        bool finished = false;
        int exitCode = -1;
        QProcess::ExitStatus exitStatus = QProcess::NormalExit;
        QString stdOutput;
        QString stdError;
    };

    virtual ProcessResult runProcess(const QString &program,
                                     const QStringList &args,
                                     int timeoutMs);
    virtual ProcessResult runProcessTracked(const QString &program,
                                            const QStringList &args,
                                            int timeoutMs);
    virtual QStringList listFiles(const QString &dirPath) const;

private:

    ExtractionResult extractZip(const QString &archivePath, const QString &outputDir);
    ExtractionResult extract7z(const QString &archivePath, const QString &outputDir);
    ExtractionResult extractRar(const QString &archivePath, const QString &outputDir);
    
    bool isToolAvailable(const QString &tool) const;
    QString findTool(const QStringList &candidates) const;
    
    QString m_unzipPath;
    QString m_sevenZipPath;
    QString m_unrarPath;
    
    bool m_cancelled = false;
    ::QProcess *m_process = nullptr;
};

} // namespace Remus
